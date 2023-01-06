#include "syntax.h"

#include "log.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  token_t op[8];
} op_set_t;

static op_set_t op_set_table[] = {
  { '=' },
  { '+', '-' },
  { '*', '/' }
};

static int num_op_set = sizeof(op_set_table) / sizeof(op_set_t);

static const lexeme_t *s_expect(lex_t *lex, token_t token);

static s_node_t *s_body(lex_t *lex);
static s_node_t *s_stmt(lex_t *lex);
static s_node_t *s_decl(lex_t *lex);
static s_node_t *s_type(lex_t *lex);
static s_node_t *s_expr(lex_t *lex);
static s_node_t *s_binop(lex_t *lex, int op_set);
static s_node_t *s_primary(lex_t *lex);

static s_node_t *make_stmt(const s_node_t *body, const s_node_t *next);
static s_node_t *make_decl(const s_node_t *type, const lexeme_t *ident, const s_node_t *init);
static s_node_t *make_type(const lexeme_t *spec);
static s_node_t *make_constant(const lexeme_t *lexeme);
static s_node_t *make_binop(const s_node_t *lhs, const lexeme_t *op, const s_node_t *rhs);
static s_node_t *make_node(s_node_type_t type);

static void s_print_R(const s_node_t *node, int pad)
{
  if (!node) {
    LOG_DEBUG("%*s", pad, "-");
    return;
  }
  
  switch (node->node_type) {
  case S_CONSTANT:
    LOG_DEBUG("%*sS_CONSTANT", pad, "");
    break;
  case S_BINOP:
    LOG_DEBUG("%*sS_BINOP", pad, "");
    s_print_R(node->binop.lhs, pad + 2);
    s_print_R(node->binop.rhs, pad + 2);
    break;
  case S_TYPE:
    LOG_DEBUG("%*sS_TYPE", pad, "");
    break;
  case S_DECL:
    LOG_DEBUG("%*sS_DECL", pad, "");
    s_print_R(node->decl.type, pad + 2);
    s_print_R(node->decl.init, pad + 2);
    break;
  case S_STMT:
    LOG_DEBUG("%*sS_STMT", pad, "");
    s_print_R(node->stmt.body, pad + 2);
    s_print_R(node->stmt.next, pad);
    break;
  }
}

void s_print(const s_node_t *s_tree)
{
  s_print_R(s_tree, 0);
}

s_node_t *s_parse(lex_t *lex)
{
  return s_body(lex);
}

static s_node_t *s_body(lex_t *lex)
{
  const s_node_t *body = s_stmt(lex);
  if (body)
    return make_stmt(body, s_body(lex));
  return NULL;
}

static s_node_t *s_stmt(lex_t *lex)
{
  s_node_t *node = NULL;
 
  node = s_decl(lex); 
  if (node) {
    if (!s_expect(lex, ';'))
      return NULL;
    return node;
  }
  
  node = s_expr(lex);
  if (node) {
    if (!s_expect(lex, ';'))
      return NULL;
    return node;
  }
  
  return NULL;
}

static s_node_t *s_decl(lex_t *lex)
{
  const s_node_t *type = s_type(lex);
  
  if (!type)
    return NULL;
  
  const lexeme_t *ident = lex_match(lex, TK_IDENTIFIER);
  
  const s_node_t *init = NULL;
  if (lex_match(lex, '=')) {
    init = s_expr(lex);
    if (!init) {
      lex_printf(lex, lex->lexeme, "error: expected expression before '%l'");
      return NULL;
    }
  }
  
  return make_decl(type, ident, init);
}

static s_node_t *s_type(lex_t *lex)
{
  const lexeme_t *spec = NULL;
  
  if ((spec = lex_match(lex, TK_I32)));
  else if ((spec = lex_match(lex, TK_F32)));
  else
    return NULL;
  
  return make_type(spec);
}

static s_node_t *s_expr(lex_t *lex)
{
  return s_binop(lex, 0);
}

static s_node_t *s_binop(lex_t *lex, int op_set)
{
  if (op_set >= num_op_set)
    return s_primary(lex);
  
  s_node_t *lhs = s_binop(lex, op_set + 1);
  
  for (int i = 0; i < 8; i++) {
    const lexeme_t *op = NULL;
    if ((op = lex_match(lex, op_set_table[op_set].op[i]))) {
      s_node_t *rhs = s_binop(lex, op_set);
      if (!rhs) {
        lex_printf(lex, lex->lexeme, "error: expected expression before '%l'");
        return NULL;
      }
      
      return make_binop(lhs, op, rhs);
    }
  }
  
  return lhs;
}

static s_node_t *s_primary(lex_t *lex)
{
  const lexeme_t *lexeme = NULL;
  
  if ((lexeme = lex_match(lex, TK_CONST_INTEGER)))
    return make_constant(lexeme);
  else if ((lexeme = lex_match(lex, TK_CONST_FLOAT)))
    return make_constant(lexeme);
  else if ((lexeme = lex_match(lex, TK_IDENTIFIER)))
    return make_constant(lexeme);
  else if (lex_match(lex, '(')) {
    s_node_t *body = s_expr(lex);
    
    if (!body) {
      lex_printf(lex, lex->lexeme, "error: expected expression before '%l'");
      return NULL;
    }
    
    if (!s_expect(lex, ')'))
      return NULL;
    
    return body;
  }
  
  return NULL;
}

static const lexeme_t *s_expect(lex_t *lex, token_t token)
{
  const lexeme_t *lexeme = lex_match(lex, token);
  
  if (!lexeme)
    lex_printf(lex, lex->lexeme, "error: expected '%t' before '%l'", token);
  
  return lexeme;
}

static s_node_t *make_stmt(const s_node_t *body, const s_node_t *next)
{
  s_node_t *node = make_node(S_STMT);
  node->stmt.body = body;
  node->stmt.next = next;
  return node;
}

static s_node_t *make_decl(const s_node_t *type, const lexeme_t *ident, const s_node_t *init)
{
  s_node_t *node = make_node(S_DECL);
  node->decl.type = type;
  node->decl.ident = ident;
  node->decl.init = init;
  return node;
}

static s_node_t *make_type(const lexeme_t *spec)
{
  s_node_t *node = make_node(S_TYPE);
  node->type.spec = spec;
  return node;
}

static s_node_t *make_binop(const s_node_t *lhs, const lexeme_t *op, const s_node_t *rhs)
{
  s_node_t *node = make_node(S_BINOP);
  node->binop.lhs = lhs;
  node->binop.op = op;
  node->binop.rhs = rhs;
}

static s_node_t *make_constant(const lexeme_t *lexeme)
{
  s_node_t *node = make_node(S_CONSTANT);
  node->constant.lexeme = lexeme;
  return node;
}

static s_node_t *make_node(s_node_type_t node_type)
{
  s_node_t *node = malloc(sizeof(s_node_t));
  *node = (s_node_t) { 0 };
  node->node_type = node_type;
  return node;
}
