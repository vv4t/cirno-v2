#include "syntax.h"

#include "log.h"
#include "zone.h"
#include <stddef.h>
#include <stdio.h>

static bool s_err = false;

typedef struct {
  token_t op[8];
} op_set_t;

static op_set_t op_set_table[] = {
  { '=', TK_ADD_ASSIGN, TK_SUB_ASSIGN, TK_MUL_ASSIGN, TK_DIV_ASSIGN },
  { TK_OR },
  { TK_AND },
  { TK_EQ, TK_NE },
  { '<', '>', TK_GE, TK_LE },
  { '+', '-' },
  { '*', '/' }
};

static int num_op_set = sizeof(op_set_table) / sizeof(op_set_t);

static const lexeme_t *s_expect(lex_t *lex, token_t token);

static s_node_t *s_body(lex_t *lex);
static s_node_t *s_fn(lex_t *lex);
static s_node_t *s_param_decl(lex_t *lex);
static s_node_t *s_if_stmt(lex_t *lex);
static s_node_t *s_ret_stmt(lex_t *lex);
static s_node_t *s_while_stmt(lex_t *lex);
static s_node_t *s_ctrl_stmt(lex_t *lex);
static s_node_t *s_for_stmt(lex_t *lex);
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

typedef enum {
  R_BODY,
  R_STMT,
  R_TYPE,
  R_UNARY,
  R_ARG,
  R_EXPR
} rule_t;

typedef s_node_t *(*s_rule_t)(lex_t *lex);

static s_node_t *s_expect_rule(lex_t *lex, rule_t rule);

static s_node_t *make_fn(const lexeme_t *fn_ident, s_node_t *param_decl, s_node_t *type, s_node_t *body);
static s_node_t *make_param_decl(s_node_t *type, const lexeme_t *ident, s_node_t *next);
static s_node_t *make_stmt(s_node_t *body, s_node_t *next);
static s_node_t *make_if_stmt(s_node_t *cond, s_node_t *body, s_node_t *next);
static s_node_t *make_ctrl_stmt(const lexeme_t *lexeme);
static s_node_t *make_while_stmt(s_node_t *cond, s_node_t *body);
static s_node_t *make_for_stmt(s_node_t *decl, s_node_t *cond, s_node_t *inc, s_node_t *body);
static s_node_t *make_class_def(const lexeme_t *ident, s_node_t *class_decl);
static s_node_t *make_decl(s_node_t *type, const lexeme_t *ident, s_node_t *init);
static s_node_t *make_type(const lexeme_t *spec, const lexeme_t *left_bracket, const lexeme_t *class_ident);
static s_node_t *make_constant(const lexeme_t *lexeme);
static s_node_t *make_new(const lexeme_t *class_ident);
static s_node_t *make_unary(const lexeme_t *op, s_node_t *rhs);
static s_node_t *make_binop(s_node_t *lhs, const lexeme_t *op, s_node_t *rhs);
static s_node_t *make_index(s_node_t *base, s_node_t *index, const lexeme_t *left_bracket);
static s_node_t *make_direct(s_node_t *base, const lexeme_t *child_ident);
static s_node_t *make_print(s_node_t *body);
static s_node_t *make_ret_stmt(const lexeme_t *ret_token, s_node_t *body);
static s_node_t *make_proc(s_node_t *base, s_node_t *arg, const lexeme_t *left_bracket);
static s_node_t *make_arg(s_node_t *body, s_node_t *next);
static s_node_t *make_array_init(const lexeme_t *array_init, s_node_t *type, s_node_t *size, s_node_t *init);
static s_node_t *make_post_op(s_node_t *lhs, const lexeme_t *op);
static s_node_t *make_node(s_node_type_t type);

s_node_t *s_parse(lex_t *lex)
{
  s_node_t *body = NULL;
  s_node_t *head = NULL;
  
  s_node_t *stmt_body = s_stmt(lex);
  
  while (stmt_body) {
    if (head)
      head = head->stmt.next = make_stmt(stmt_body, NULL);
    else
      body = head = make_stmt(stmt_body, NULL);
    
    stmt_body = s_stmt(lex);
  }
  
  return body;
}

bool s_error()
{
  return s_err;
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
  
  s_node_t *body = NULL;
  if (!lex_match(lex, ';'))
    body = s_expect_rule(lex, R_BODY);
  
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
  
  node = s_ctrl_stmt(lex);
  if (node)
    return node;
  
  node = s_for_stmt(lex);
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
  
  node = s_ret_stmt(lex); 
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

static s_node_t *s_ctrl_stmt(lex_t *lex)
{
  const lexeme_t *lexeme = NULL;
  
  if ((lexeme = lex_match(lex, TK_BREAK)));
  else if ((lexeme = lex_match(lex, TK_CONTINUE)));
  else
    return NULL;
  
  s_expect(lex, ';');
  
  return make_ctrl_stmt(lexeme);
}

static s_node_t *s_ret_stmt(lex_t *lex)
{
  const lexeme_t *ret_token = NULL;
  if (!(ret_token = lex_match(lex, TK_RETURN)))
    return NULL;
  
  s_node_t *body = s_expect_rule(lex, R_EXPR);
  s_expect(lex, ';');
  
  return make_ret_stmt(ret_token, body);
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

static s_node_t *s_for_stmt(lex_t *lex)
{
  if (!lex_match(lex, TK_FOR))
    return NULL;
  
  s_expect(lex, '(');
  s_node_t *decl = make_stmt(s_decl(lex), NULL);
  if (!decl)
    decl = make_stmt(s_expr(lex), NULL);
  s_expect(lex, ';');
  
  s_node_t *cond = s_expect_rule(lex, R_EXPR);
  s_expect(lex, ';');
 
  s_node_t *inc = s_expr(lex);
  
  s_expect(lex, ')');
  s_node_t *body = s_expect_rule(lex, R_BODY);
  
  return make_for_stmt(decl, cond, inc, body);
}

static s_node_t *s_if_stmt(lex_t *lex)
{
  if (!lex_match(lex, TK_IF))
    return NULL;
  
  s_expect(lex, '(');
  s_node_t *cond = s_expect_rule(lex, R_EXPR);
  s_expect(lex, ')');
  
  s_node_t *body = s_expect_rule(lex, R_BODY);
  
  s_node_t *next = NULL;
  if (lex_match(lex, TK_ELSE))
    next = s_expect_rule(lex, R_BODY);
  
  return make_if_stmt(cond, body, next);
}

static s_node_t *s_print(lex_t *lex)
{
  if (!lex_match(lex, TK_PRINT))
    return NULL;
  
  s_node_t *body = s_expect_rule(lex, R_ARG);
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
  s_node_t *body = NULL;
  
  body = s_decl(lex);
  if (body) {
    s_expect(lex, ';');
    return make_stmt(body, s_class_decl(lex));
  }
  
  body = s_fn(lex);
  if (body)
    return make_stmt(body, s_class_decl(lex));
  
  return NULL;
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
  else if ((spec = lex_match(lex, TK_STRING)));
  else if ((spec = lex_match(lex, TK_CLASS))) {
    class_ident = s_expect(lex, TK_IDENTIFIER);
  } else
    return NULL;
  
  const lexeme_t *left_bracket = NULL;
  if ((left_bracket = lex_match(lex, '[')))
    s_expect(lex, ']');
  
  return make_type(spec, left_bracket, class_ident);
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
  
  while (1) {
    const lexeme_t *op = NULL;
    for (int i = 0; i < 8; i++) {
      op = lex_match(lex, op_set_table[op_set].op[i]);
      if (op)
        break;
    }
    
    if (!op)
      break;
    
    s_node_t *rhs = s_binop(lex, op_set + 1);
    if (!rhs) {
      c_error(lex->lexeme, "expected 'expression' before '%l'", lex->lexeme);
      s_err = true;
    }
    
    lhs = make_binop(lhs, op, rhs);
  }
  
  return lhs;
}

static s_node_t *s_unary(lex_t *lex)
{
  const lexeme_t *op = NULL;
  if ((op = lex_match(lex, '-')))
    return make_unary(op, s_unary(lex));
  else if ((op = lex_match(lex, '!')))
    return make_unary(op, s_unary(lex));
  return s_postfix(lex);
}

static s_node_t *s_postfix(lex_t *lex)
{
  s_node_t *base = s_primary(lex);
  
  while (1) {
    const lexeme_t *lexeme = NULL;
    if ((lexeme = lex_match(lex, '['))) {
      s_node_t *index = s_expect_rule(lex, R_EXPR);
      s_expect(lex, ']');
      base = make_index(base, index, lexeme);
    } else if (lex_match(lex, '.')) {
      const lexeme_t *child_ident = s_expect(lex, TK_IDENTIFIER);
      base = make_direct(base, child_ident);
    } else if ((lexeme = lex_match(lex, TK_INC))) {
      base = make_post_op(base, lexeme);
    } else if ((lexeme =lex_match(lex, TK_DEC))) {
      base = make_post_op(base, lexeme);
    } else if ((lexeme = lex_match(lex, '('))) {
      s_node_t *arg = s_arg(lex);
      s_expect(lex, ')');
      base = make_proc(base, arg, lexeme);
    } else if ((lexeme = lex_match(lex, TK_ARRAY_INIT))) {
      s_expect(lex, '<');
      s_node_t *type = s_type(lex);
      s_expect(lex, '>');
      
      s_node_t *init = NULL;
      s_node_t *size = NULL;
      if (lex_match(lex, '{')) {
        init = s_expect_rule(lex, R_ARG);
        s_expect(lex, '}');
      } else if (s_expect(lex, '(')) {
        size = s_expect_rule(lex, R_EXPR);
        s_expect(lex, ')');
      }
      
      return make_array_init(lexeme, type, size, init);
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
  else if ((lexeme = lex_match(lex, TK_NEW))) {
    const lexeme_t *class_ident = s_expect(lex, TK_IDENTIFIER);
    return make_new(class_ident);
  } else if ((lexeme = lex_match(lex, TK_IDENTIFIER)))
    return make_constant(lexeme);
  else if ((lexeme = lex_match(lex, TK_STRING_LITERAL)))
    return make_constant(lexeme);
  else if (lex_match(lex, '(')) {
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
    s_arg,    // R_ARG
    s_expr    // R_EXPR
  };
  
  static const char *str_rule_table[] = {
    "body-statement",             // R_BODY
    "statement",                  // R_STMT
    "type",                       // R_TYPE
    "unary",                      // R_UNARY
    "argument-expression-list",   // R_UNARY
    "expression"                  // R_EXPR
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

static s_node_t *make_ctrl_stmt(const lexeme_t *lexeme)
{
  s_node_t *node = make_node(S_CTRL_STMT);
  node->ctrl_stmt.lexeme = lexeme;
  return node;
}

static s_node_t *make_if_stmt(s_node_t *cond, s_node_t *body, s_node_t *next)
{
  s_node_t *node = make_node(S_IF_STMT);
  node->if_stmt.cond = cond;
  node->if_stmt.body = body;
  node->if_stmt.next = next;
  return node;
}

static s_node_t *make_while_stmt(s_node_t *cond, s_node_t *body)
{
  s_node_t *node = make_node(S_WHILE_STMT);
  node->while_stmt.cond = cond;
  node->while_stmt.body = body;
  return node;
}

static s_node_t *make_for_stmt(s_node_t *decl, s_node_t *cond, s_node_t *inc, s_node_t *body)
{
  s_node_t *node = make_node(S_FOR_STMT);
  node->for_stmt.decl = decl;
  node->for_stmt.cond = cond;
  node->for_stmt.inc = inc;
  node->for_stmt.body = body;
  return node;
}

static s_node_t *make_ret_stmt(const lexeme_t *ret_token, s_node_t *body)
{
  s_node_t *node = make_node(S_RET_STMT);
  node->ret_stmt.ret_token = ret_token;
  node->ret_stmt.body = body;
  return node;
}

static s_node_t *make_print(s_node_t *arg)
{
  s_node_t *node = make_node(S_PRINT);
  node->print.arg = arg;
  return node;
}

static s_node_t *make_class_def(const lexeme_t *ident, s_node_t *class_decl)
{
  s_node_t *node = make_node(S_CLASS_DEF);
  node->class_def.ident = ident;
  node->class_def.class_decl = class_decl;
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

static s_node_t *make_type(const lexeme_t *spec, const lexeme_t *left_bracket, const lexeme_t *class_ident)
{
  s_node_t *node = make_node(S_TYPE);
  node->type.spec = spec;
  node->type.left_bracket = left_bracket;
  node->type.class_ident = class_ident;
  return node;
}

static s_node_t *make_post_op(s_node_t *lhs, const lexeme_t *op)
{
  s_node_t *node = make_node(S_POST_OP);
  node->post_op.op = op;
  node->post_op.lhs = lhs;
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

static s_node_t *make_proc(s_node_t *base, s_node_t *arg, const lexeme_t *left_bracket)
{
  s_node_t *node = make_node(S_PROC);
  node->proc.base = base;
  node->proc.arg = arg;
  node->proc.left_bracket = left_bracket;
  return node;
}

static s_node_t *make_arg(s_node_t *body, s_node_t *next)
{
  s_node_t *node = make_node(S_ARG);
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

static s_node_t *make_new(const lexeme_t *class_ident)
{
  s_node_t *node = make_node(S_NEW);
  node->new.class_ident = class_ident;
  return node;
}

static s_node_t *make_array_init(const lexeme_t *array_init, s_node_t *type, s_node_t *size, s_node_t *init)
{
  s_node_t *node = make_node(S_ARRAY_INIT);
  node->array_init.array_init = array_init;
  node->array_init.type = type;
  node->array_init.size = size;
  node->array_init.init = init;
  return node;
}

static s_node_t *make_node(s_node_type_t node_type)
{
  s_node_t *node = ZONE_ALLOC(sizeof(s_node_t));
  *node = (s_node_t) { 0 };
  node->node_type = node_type;
  return node;
}

static void s_print_node_R(const s_node_t *node, int pad)
{
  if (!node) {
    LOG_DEBUG("%*s-", pad, "");
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
    s_print_node_R(node->if_stmt.next, pad + 2);
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
    s_print_node_R(node->print.arg, pad + 2);
    break;
  case S_DIRECT:
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
    s_print_node_R(node->param_decl.type, pad + 2);
    s_print_node_R(node->param_decl.next, pad);
    break;
  case S_PROC:
    LOG_DEBUG("%*sS_PROC", pad, "");
    s_print_node_R(node->proc.base, pad + 2);
    s_print_node_R(node->proc.arg, pad + 2);
    break;
  case S_ARG:
    LOG_DEBUG("%*sS_ARG", pad, "");
    s_print_node_R(node->arg.body, pad + 2);
    s_print_node_R(node->arg.next, pad);
    break;
  case S_RET_STMT:
    LOG_DEBUG("%*sS_RETURN", pad, "");
    s_print_node_R(node->ret_stmt.body, pad + 2);
    break;
  case S_NEW:
    LOG_DEBUG("%*sS_NEW", pad, "");
    break;
  case S_ARRAY_INIT:
    LOG_DEBUG("%*sS_ARRAY_INIT", pad, "");
    s_print_node_R(node->array_init.type, pad + 2);
    s_print_node_R(node->array_init.size, pad + 2);
    s_print_node_R(node->array_init.init, pad + 2);
    break;
  case S_POST_OP:
    LOG_DEBUG("%*sS_POST_OP", pad, "");
    s_print_node_R(node->post_op.lhs, pad + 2);
    break;
  case S_FOR_STMT:
    LOG_DEBUG("%*sS_FOR_STMT", pad, "");
    s_print_node_R(node->for_stmt.decl, pad + 2);
    s_print_node_R(node->for_stmt.cond, pad + 2);
    s_print_node_R(node->for_stmt.inc, pad + 2);
    s_print_node_R(node->for_stmt.body, pad + 2);
    break;
  case S_CTRL_STMT:
    LOG_DEBUG("%*sS_CTRL_STMT", pad, "");
    break;
  default:
    LOG_ERROR("unknown s_node_t (%i)", node->node_type);
    break;
  }
}

void s_print_node(const s_node_t *node)
{
  s_print_node_R(node, 0);
}

void s_free(s_node_t *node)
{
  if (!node)
    return;
  
  switch (node->node_type) {
  case S_CONSTANT:
    break;
  case S_BINOP:
    s_free(node->binop.lhs);
    s_free(node->binop.rhs);
    break;
  case S_TYPE:
    break;
  case S_DECL:
    s_free(node->decl.type);
    s_free(node->decl.init);
    break;
  case S_CLASS_DEF:
    s_free(node->class_def.class_decl);
    break;
  case S_INDEX:
    s_free(node->index.base);
    s_free(node->index.index);
    break;
  case S_STMT:
    s_free(node->stmt.body);
    s_free(node->stmt.next);
    break;
  case S_IF_STMT:
    s_free(node->if_stmt.cond);
    s_free(node->if_stmt.body);
    s_free(node->if_stmt.next);
    break;
  case S_UNARY:
    s_free(node->unary.rhs);
    break;
  case S_WHILE_STMT:
    s_free(node->while_stmt.cond);
    s_free(node->while_stmt.body);
    break;
  case S_PRINT:
    s_free(node->print.arg);
    break;
  case S_DIRECT:
    s_free(node->direct.base);
    break;
  case S_FN:
    s_free(node->fn.param_decl);
    s_free(node->fn.type);
    s_free(node->fn.body);
    break;
  case S_PARAM_DECL:
    s_free(node->param_decl.type);
    s_free(node->param_decl.next);
    break;
  case S_PROC:
    s_free(node->proc.base);
    s_free(node->proc.arg);
    break;
  case S_ARG:
    s_free(node->arg.body);
    s_free(node->arg.next);
    break;
  case S_RET_STMT:
    s_free(node->ret_stmt.body);
    break;
  case S_NEW:
    break;
  case S_ARRAY_INIT:
    s_free(node->array_init.type);
    s_free(node->array_init.size);
    s_free(node->array_init.init);
    break;
  case S_POST_OP:
    s_free(node->post_op.lhs);
    break;
  case S_FOR_STMT:
    s_free(node->for_stmt.decl);
    s_free(node->for_stmt.cond);
    s_free(node->for_stmt.inc);
    s_free(node->for_stmt.body);
    break;
  case S_CTRL_STMT:
    break;
  default:
    LOG_ERROR("unknown s_node_t (%i)", node->node_type);
    break;
  }
  
  ZONE_FREE(node);
}
