#include "syntax.h"

#include "log.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static bool s_err = false;

typedef struct {
  token_t op[8];
} op_set_t;

static op_set_t op_set_table[] = {
  { '=' },
  { '<', '>' },
  { '+', '-' },
  { '*', '/' }
};

static int num_op_set = sizeof(op_set_table) / sizeof(op_set_t);

static const lexeme_t *s_expect(lex_t *lex, token_t token);

static s_node_t *s_body(lex_t *lex);
static s_node_t *s_fn(lex_t *lex);
static s_node_t *s_param_decl(lex_t *lex);
static s_node_t *s_if_stmt(lex_t *lex);
static s_node_t *s_while_stmt(lex_t *lex);
static s_node_t *s_class_def(lex_t *lex);
static s_node_t *s_class_decl(lex_t *lex);
static s_node_t *s_stmt(lex_t *lex);
static s_node_t *s_print(lex_t *lex);
static s_node_t *s_decl(lex_t *lex);
static s_node_t *s_type(lex_t *lex);
static s_node_t *s_expr(lex_t *lex);
static s_node_t *s_binop(lex_t *lex, int op_set);
static s_node_t *s_unary(lex_t *lex);
static s_node_t *s_postfix(lex_t *lex);
static s_node_t *s_arg(lex_t *lex);
static s_node_t *s_primary(lex_t *lex);

static void s_print_node_R(const s_node_t *node, int pad);
void s_print_node(const s_node_t *node);

typedef enum {
  R_BODY,
  R_STMT,
  R_TYPE,
  R_UNARY,
  R_EXPR
} rule_t;

typedef s_node_t *(*s_rule_t)(lex_t *lex);

static s_node_t *s_expect_rule(lex_t *lex, rule_t rule);

static s_node_t *make_fn(const lexeme_t *fn_ident, s_node_t *param_decl, s_node_t *type, s_node_t *body);
static s_node_t *make_param_decl(s_node_t *type, const lexeme_t *ident, s_node_t *next);
static s_node_t *make_stmt(s_node_t *body, s_node_t *next);
static s_node_t *make_if_stmt(s_node_t *cond, s_node_t *body);
static s_node_t *make_while_stmt(s_node_t *cond, s_node_t *body);
static s_node_t *make_class_def(const lexeme_t *ident, s_node_t *class_decl);
static s_node_t *make_decl(s_node_t *type, const lexeme_t *ident, s_node_t *init);
static s_node_t *make_class_decl(s_node_t *type, const lexeme_t *ident, s_node_t *next);
static s_node_t *make_type(const lexeme_t *spec, const lexeme_t *ptr, s_node_t *size, const lexeme_t *class_ident);
static s_node_t *make_constant(const lexeme_t *lexeme);
static s_node_t *make_unary(const lexeme_t *op, s_node_t *rhs);
static s_node_t *make_binop(s_node_t *lhs, const lexeme_t *op, s_node_t *rhs);
static s_node_t *make_index(s_node_t *base, s_node_t *index, const lexeme_t *left_bracket);
static s_node_t *make_direct(s_node_t *base, const lexeme_t *child_ident);
static s_node_t *make_indirect(s_node_t *base, const lexeme_t *child_ident);
static s_node_t *make_print(s_node_t *body);
static s_node_t *make_proc(const lexeme_t *func_ident, s_node_t *arg);
static s_node_t *make_arg(s_node_t *body, s_node_t *next);
static s_node_t *make_node(s_node_type_t type);

s_node_t *s_parse(lex_t *lex)
{
  s_node_t *node = s_body(lex);
  if (s_err)
    return NULL;
  return node;
}

static s_node_t *s_body(lex_t *lex)
{
  s_node_t *body = NULL;
  s_node_t *stmt_body = s_stmt(lex);
  if (stmt_body) {
    body = make_stmt(stmt_body, NULL);
  } else if (lex_match(lex, '{')) {
    s_node_t *head = NULL;
    stmt_body = s_stmt(lex);
    
    while (stmt_body) {
      if (head)
        head = head->stmt.next = make_stmt(stmt_body, NULL);
      else
        body = head = make_stmt(stmt_body, NULL);
      
      stmt_body = s_stmt(lex);
    }
    
    s_expect(lex, '}');
  }
  
  return body;
}

static s_node_t *s_fn(lex_t *lex)
{
  if (!lex_match(lex, TK_FN))
    return NULL;
  
  const lexeme_t *fn_ident = s_expect(lex, TK_IDENTIFIER);
  
  s_expect(lex, '(');
  s_node_t *param_decl = s_param_decl(lex);
  s_expect(lex, ')');
  
  s_node_t *type = NULL;
  if (lex_match(lex, ':'))
    type = s_expect_rule(lex, R_TYPE);
  
  s_node_t *body = s_expect_rule(lex, R_BODY);
  
  return make_fn(fn_ident, param_decl, type, body);
}

static s_node_t *s_param_decl(lex_t *lex)
{
  s_node_t *type = s_type(lex);
  if (!type)
    return NULL;
  
  const lexeme_t *ident = s_expect(lex, TK_IDENTIFIER);
  
  if (lex_match(lex, ',')) 
    return make_param_decl(type, ident, s_param_decl(lex));
  else
    return make_param_decl(type, ident, NULL);
}

static s_node_t *s_stmt(lex_t *lex)
{
  s_node_t *node = NULL;
  
  node = s_fn(lex);
  if (node)
    return node;
  
  node = s_while_stmt(lex);
  if (node)
    return node;
  
  node = s_if_stmt(lex);
  if (node)
    return node;
  
  node = s_print(lex);
  if (node)
    return node;
  
  node = s_decl(lex); 
  if (node) {
    s_expect(lex, ';');
    return node;
  }
  
  node = s_class_def(lex);
  if (node) {
    s_expect(lex, ';');
    return node;
  }
  
  node = s_expr(lex);
  if (node) {
    s_expect(lex, ';');
    return node;
  }
  
  return NULL;
}

static s_node_t *s_while_stmt(lex_t *lex)
{
  if (!lex_match(lex, TK_WHILE))
    return NULL;
  
  s_expect(lex, '(');
  s_node_t *cond = s_expect_rule(lex, R_EXPR);
  s_expect(lex, ')');
  
  s_node_t *body = s_expect_rule(lex, R_BODY);
  
  return make_while_stmt(cond, body);
}

static s_node_t *s_if_stmt(lex_t *lex)
{
  if (!lex_match(lex, TK_IF))
    return NULL;
  
  s_expect(lex, '(');
  s_node_t *cond = s_expect_rule(lex, R_EXPR);
  s_expect(lex, ')');
  
  s_node_t *body = s_expect_rule(lex, R_BODY);
  
  return make_if_stmt(cond, body);
}

static s_node_t *s_print(lex_t *lex)
{
  if (!lex_match(lex, TK_PRINT))
    return NULL;
  
  s_node_t *body = s_expect_rule(lex, R_EXPR);
  s_expect(lex, ';');
  
  return make_print(body);
}

static s_node_t *s_class_def(lex_t *lex)
{
  if (!lex_match(lex, TK_CLASS_DEF))
    return NULL;
  
  const lexeme_t *ident = s_expect(lex, TK_IDENTIFIER);
  
  s_expect(lex, '{');
  s_node_t *class_decl = s_class_decl(lex);
  s_expect(lex, '}');
  
  return make_class_def(ident, class_decl);
}

static s_node_t *s_class_decl(lex_t *lex)
{
  s_node_t *type = s_type(lex);
  if (!type)
    return NULL;
  
  const lexeme_t *ident = s_expect(lex, TK_IDENTIFIER);
  s_expect(lex, ';');
  
  return make_class_decl(type, ident, s_class_decl(lex));
}

static s_node_t *s_decl(lex_t *lex)
{
  s_node_t *type = s_type(lex);
  if (!type)
    return NULL;
  
  const lexeme_t *ident = s_expect(lex, TK_IDENTIFIER);
  
  s_node_t *init = NULL;
  if (lex_match(lex, '='))
    init = s_expect_rule(lex, R_EXPR);
  
  return make_decl(type, ident, init);
}

static s_node_t *s_type(lex_t *lex)
{
  const lexeme_t *spec = NULL;
  const lexeme_t *class_ident = NULL;
  
  if ((spec = lex_match(lex, TK_I32)));
  else if ((spec = lex_match(lex, TK_F32)));
  else if ((spec = lex_match(lex, TK_CLASS))) {
    class_ident = s_expect(lex, TK_IDENTIFIER);
  } else
    return NULL;
  
  const lexeme_t *ptr = lex_match(lex, '*');
  
  s_node_t *size = NULL;
  if (lex_match(lex, '[')) {
    size = s_expect_rule(lex, R_EXPR);
    s_expect(lex, ']');
  }
  
  return make_type(spec, ptr, size, class_ident);
}

static s_node_t *s_expr(lex_t *lex)
{
  return s_binop(lex, 0);
}

static s_node_t *s_binop(lex_t *lex, int op_set)
{
  if (op_set >= num_op_set)
    return s_unary(lex);
  
  s_node_t *lhs = s_binop(lex, op_set + 1);
  if (!lhs)
    return NULL;
  
  for (int i = 0; i < 8; i++) {
    const lexeme_t *op = NULL;
    while ((op = lex_match(lex, op_set_table[op_set].op[i]))) {
      s_node_t *rhs = s_binop(lex, op_set + 1);
      if (!rhs)
        c_error(lex->lexeme, "error: expected 'expression' before '%l'", lex->lexeme);
      
      lhs = make_binop(lhs, op, rhs);
    }
  }
  
  return lhs;
}

static s_node_t *s_unary(lex_t *lex)
{
  const lexeme_t *op = NULL;
  
  op = lex_match(lex, '&');
  if (op)
    return make_unary(op, s_expect_rule(lex, R_UNARY));
  
  op = lex_match(lex, '*');
  if (op)
    return make_unary(op, s_expect_rule(lex, R_UNARY));
  
  return s_postfix(lex);
}

static s_node_t *s_postfix(lex_t *lex)
{
  s_node_t *base = s_primary(lex);
  
  while (1) {
    const lexeme_t *left_bracket = NULL;
    if ((left_bracket = lex_match(lex, '['))) {
      s_node_t *index = s_expect_rule(lex, R_EXPR);
      s_expect(lex, ']');
      base = make_index(base, index, left_bracket);
    } else if (lex_match(lex, '.')) {
      const lexeme_t *child_ident = s_expect(lex, TK_IDENTIFIER);
      base = make_direct(base, child_ident);
    } else if (lex_match(lex, TK_PTR_OP)) {
      const lexeme_t *child_ident = s_expect(lex, TK_IDENTIFIER);
      base = make_indirect(base, child_ident);
    } else {
      return base;
    }
  }
}

static s_node_t *s_arg(lex_t *lex)
{
  s_node_t *body = s_expr(lex);
  if (!body)
    return NULL;
  
  if (lex_match(lex, ','))
    return make_arg(body, s_arg(lex));
  else
    return make_arg(body, NULL);
}

static s_node_t *s_primary(lex_t *lex)
{
  const lexeme_t *lexeme = NULL;
  
  if ((lexeme = lex_match(lex, TK_CONST_INTEGER)))
    return make_constant(lexeme);
  else if ((lexeme = lex_match(lex, TK_CONST_FLOAT)))
    return make_constant(lexeme);
  else if ((lexeme = lex_match(lex, TK_IDENTIFIER))) {
    if (lex_match(lex, '(')) {
      s_node_t *arg = s_arg(lex);
      s_node_t *proc = make_proc(lexeme, arg);
      s_expect(lex, ')');
      return proc;
    } 
    
    return make_constant(lexeme);
  } else if (lex_match(lex, '(')) {
    s_node_t *body = s_expect_rule(lex, R_EXPR);
    s_expect(lex, ')');
    return body;
  }
  
  return NULL;
}

static s_node_t *s_expect_rule(lex_t *lex, rule_t rule)
{
  static s_rule_t s_rule_table[] = {
    s_body,   // R_BODY
    s_stmt,   // R_STMT
    s_type,   // R_TYPE
    s_unary,  // R_STMT
    s_expr    // R_EXPR
  };
  
  static const char *str_rule_table[] = {
    "body-statement", // R_BODY
    "statement",      // R_STMT
    "type",           // R_TYPE
    "unary",          // R_UNARY
    "expression"      // R_EXPR
  };
  
  s_node_t *node = s_rule_table[rule](lex);
  
  if (!node) {
    c_error(lex->lexeme, "error: expected '%s' before '%l'", str_rule_table[rule], lex->lexeme);
    s_err = true;
  }
  
  return node;
}

static const lexeme_t *s_expect(lex_t *lex, token_t token)
{
  const lexeme_t *lexeme = lex_match(lex, token);
  
  if (!lexeme) {
    c_error(lex->lexeme, "error: expected '%t' before '%l'", token, lex->lexeme);
    s_err = true;
  }
  
  return lexeme;
}

static s_node_t *make_fn(const lexeme_t *fn_ident, s_node_t *param_decl, s_node_t *type, s_node_t *body)
{
  s_node_t *node = make_node(S_FN);
  node->fn.fn_ident = fn_ident;
  node->fn.param_decl = param_decl;
  node->fn.type = type;
  node->fn.body = body;
  return node;
}

static s_node_t *make_stmt(s_node_t *body, s_node_t *next)
{
  s_node_t *node = make_node(S_STMT);
  node->stmt.body = body;
  node->stmt.next = next;
  return node;
}

static s_node_t *make_if_stmt(s_node_t *cond, s_node_t *body)
{
  s_node_t *node = make_node(S_IF_STMT);
  node->if_stmt.cond = cond;
  node->if_stmt.body = body;
  return node;
}

static s_node_t *make_while_stmt(s_node_t *cond, s_node_t *body)
{
  s_node_t *node = make_node(S_WHILE_STMT);
  node->while_stmt.cond = cond;
  node->while_stmt.body = body;
  return node;
}

static s_node_t *make_print(s_node_t *body)
{
  s_node_t *node = make_node(S_PRINT);
  node->print.body = body;
  return node;
}

static s_node_t *make_class_def(const lexeme_t *ident, s_node_t *class_decl)
{
  s_node_t *node = make_node(S_CLASS_DEF);
  node->class_def.ident = ident;
  node->class_def.class_decl = class_decl;
  return node;
}

static s_node_t *make_class_decl(s_node_t *type, const lexeme_t *ident, s_node_t *next)
{
  s_node_t *node = make_node(S_CLASS_DECL);
  node->class_decl.type = type;
  node->class_decl.ident = ident;
  node->class_decl.next = next;
  return node;
}

static s_node_t *make_param_decl(s_node_t *type, const lexeme_t *ident, s_node_t *next)
{
  s_node_t *node = make_node(S_PARAM_DECL);
  node->param_decl.type = type;
  node->param_decl.ident = ident;
  node->param_decl.next = next;
  return node;
}

static s_node_t *make_decl(s_node_t *type, const lexeme_t *ident, s_node_t *init)
{
  s_node_t *node = make_node(S_DECL);
  node->decl.type = type;
  node->decl.ident = ident;
  node->decl.init = init;
  return node;
}

static s_node_t *make_type(const lexeme_t *spec, const lexeme_t *ptr, s_node_t *size, const lexeme_t *class_ident)
{
  s_node_t *node = make_node(S_TYPE);
  node->type.spec = spec;
  node->type.ptr = ptr;
  node->type.size = size;
  node->type.class_ident = class_ident;
  return node;
}

static s_node_t *make_unary(const lexeme_t *op, s_node_t *rhs)
{
  s_node_t *node = make_node(S_UNARY);
  node->unary.op = op;
  node->unary.rhs = rhs;
  return node;
}

static s_node_t *make_binop(s_node_t *lhs, const lexeme_t *op, s_node_t *rhs)
{
  s_node_t *node = make_node(S_BINOP);
  node->binop.lhs = lhs;
  node->binop.op = op;
  node->binop.rhs = rhs;
  return node;
}

static s_node_t *make_index(s_node_t *base, s_node_t *index, const lexeme_t *left_bracket)
{
  s_node_t *node = make_node(S_INDEX);
  node->index.base = base;
  node->index.index = index;
  node->index.left_bracket = left_bracket;
  return node;
}

static s_node_t *make_direct(s_node_t *base, const lexeme_t *child_ident)
{
  s_node_t *node = make_node(S_DIRECT);
  node->direct.base = base;
  node->direct.child_ident = child_ident;
  return node;
}

static s_node_t *make_indirect(s_node_t *base, const lexeme_t *child_ident)
{
  s_node_t *node = make_node(S_INDIRECT);
  node->indirect.base = base;
  node->indirect.child_ident = child_ident;
  return node;
}

static s_node_t *make_proc(const lexeme_t *func_ident, s_node_t *arg)
{
  s_node_t *node = make_node(S_PROC);
  node->proc.func_ident = func_ident;
  node->proc.arg = arg;
  return node;
}

static s_node_t *make_arg(s_node_t *body, s_node_t *next)
{
  s_node_t *node = make_node(S_PROC);
  node->arg.body = body;
  node->arg.next = next;
  return node;
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

static void s_print_node_R(const s_node_t *node, int pad)
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
    s_print_node_R(node->binop.lhs, pad + 2);
    s_print_node_R(node->binop.rhs, pad + 2);
    break;
  case S_TYPE:
    LOG_DEBUG("%*sS_TYPE", pad, "");
    s_print_node_R(node->type.size, pad + 2);
    break;
  case S_DECL:
    LOG_DEBUG("%*sS_DECL", pad, "");
    s_print_node_R(node->decl.type, pad + 2);
    s_print_node_R(node->decl.init, pad + 2);
    break;
  case S_CLASS_DEF:
    LOG_DEBUG("%*sS_CLASS_DEF", pad, "");
    s_print_node_R(node->class_def.class_decl, pad + 2);
    break;
  case S_CLASS_DECL:
    LOG_DEBUG("%*sS_CLASS_DECL", pad, "");
    s_print_node_R(node->class_decl.type, pad + 2);
    s_print_node_R(node->class_decl.next, pad);
    break;
  case S_INDEX:
    LOG_DEBUG("%*sS_INDEX", pad, "");
    s_print_node_R(node->index.base, pad + 2);
    s_print_node_R(node->index.index, pad + 2);
    break;
  case S_STMT:
    LOG_DEBUG("%*sS_STMT", pad, "");
    s_print_node_R(node->stmt.body, pad + 2);
    s_print_node_R(node->stmt.next, pad);
    break;
  case S_IF_STMT:
    LOG_DEBUG("%*sS_IF_STMT", pad, "");
    s_print_node_R(node->if_stmt.cond, pad + 2);
    s_print_node_R(node->if_stmt.body, pad + 2);
    break;
  case S_UNARY:
    LOG_DEBUG("%*sS_UNARY", pad, "");
    s_print_node_R(node->unary.rhs, pad + 2);
    break;
  case S_WHILE_STMT:
    LOG_DEBUG("%*sS_WHILE_STMT", pad, "");
    s_print_node_R(node->while_stmt.cond, pad + 2);
    s_print_node_R(node->while_stmt.body, pad + 2);
    break;
  case S_PRINT:
    LOG_DEBUG("%*sS_PRINT", pad, "");
    s_print_node_R(node->print.body, pad + 2);
    break;
  case S_DIRECT:
    LOG_DEBUG("%*sS_DIRECT", pad, "");
    s_print_node_R(node->direct.base, pad + 2);
    break;
  case S_INDIRECT:
    LOG_DEBUG("%*sS_INDIRECT", pad, "");
    s_print_node_R(node->direct.base, pad + 2);
    break;
  case S_FN:
    LOG_DEBUG("%*sS_FN", pad, "");
    s_print_node_R(node->fn.param_decl, pad + 2);
    s_print_node_R(node->fn.type, pad + 2);
    s_print_node_R(node->fn.body, pad + 2);
    break;
  case S_PARAM_DECL:
    LOG_DEBUG("%*sS_PARAM_DECL", pad, "");
    s_print_node_R(node->class_decl.type, pad + 2);
    s_print_node_R(node->class_decl.next, pad);
    break;
  case S_PROC:
    LOG_DEBUG("%*sS_PROC", pad, "");
    s_print_node_R(node->proc.arg, pad + 2);
    break;
  case S_ARG:
    LOG_DEBUG("%*sS_PROC", pad, "");
    s_print_node_R(node->arg.body, pad + 2);
    s_print_node_R(node->arg.next, pad);
    break;
  }
}

void s_print_node(const s_node_t *node)
{
  s_print_node_R(node, 0);
}
