#include "ast.h"

#include "log.h"
#include <stdlib.h>

static ast_expr_t   *ast_constant(const s_node_t *node);
static ast_expr_t   *ast_binop(const s_node_t *node);
static ast_expr_t   *ast_expr(const s_node_t *node);
static ast_expr_t   *ast_assign(const ast_expr_t *lhs, const ast_expr_t *rhs);
static ast_lvalue_t *ast_lvalue(const s_node_t *node);

static ast_decl_t   *ast_decl(const s_node_t *node);
static type_t       *ast_type(const s_node_t *node);

static ast_stmt_t   *make_stmt_expr(const ast_expr_t *expr, const ast_stmt_t *next);
static ast_stmt_t   *make_stmt_decl(const ast_decl_t *decl, const ast_stmt_t *next);

static ast_decl_t   *make_decl(const type_t *type, const char *ident, const ast_expr_t *init);
static type_t       *make_type(spec_t spec);

static ast_expr_t   *make_expr_lvalue(const ast_lvalue_t *lvalue);
static ast_expr_t   *make_binop(const ast_expr_t *lhs, ast_op_t op, const ast_expr_t *rhs, const type_t *type);
static ast_expr_t   *make_const_i32(int i32);
static ast_expr_t   *make_expr(ast_expr_type_t node_type, const type_t *type);

type_t type_i32 = { .spec = TYPE_SPEC_I32 };

ast_stmt_t *ast_stmt(const s_node_t *node)
{
  if (!node)
    return NULL;
  
  switch (node->stmt.body->node_type) {
  case S_CONSTANT:
  case S_BINOP:
    return make_stmt_expr(ast_expr(node->stmt.body), ast_stmt(node->stmt.next));
  case S_DECL:
    return make_stmt_decl(ast_decl(node->stmt.body), ast_stmt(node->stmt.next));
  }
  
  return NULL;
}

ast_decl_t *ast_decl(const s_node_t *node)
{
  type_t *type = ast_type(node->decl.type);
  const char *ident = node->decl.ident->data.ident;
  ast_expr_t *init = ast_expr(node->decl.init);
  return make_decl(type, ident, init);
}

type_t *ast_type(const s_node_t *node)
{
  spec_t spec;
  switch (node->type.spec->token) {
  case TK_I32:
    spec = TYPE_SPEC_I32;
    break;
  case TK_F32:
    spec = TYPE_SPEC_F32;
    break;
  }
  
  return make_type(spec);
}

ast_expr_t *ast_assign()
{
  
}

ast_lvalue_t *ast_value(const s_node_t *node)
{
  
}

ast_expr_t *ast_expr(const s_node_t *node)
{
  switch (node->node_type) {
  case S_CONSTANT:
    return ast_constant(node);
  case S_BINOP:
    return ast_binop(node);
  }
  
  return NULL;
}

ast_lvalue_t *ast_lvalue(const s_node_t *node)
{
  const char *ident = ident;
  switch (node->node_type) {
  case S_CONSTANT:
    ident = node->constant.lexeme->data.ident;
    break;
  default:
    return NULL;
  }
  
  ast_lvalue_t *lvalue = malloc(sizeof(ast_lvalue_t));
  lvalue->ident = ident;
  lvalue->type = NULL;
  
  return lvalue;
}

ast_expr_t *ast_binop(const s_node_t *node)
{
  ast_expr_t *lhs = ast_expr(node->binop.lhs);
  ast_expr_t *rhs = ast_expr(node->binop.rhs);
  
  ast_op_t op;
  switch (node->binop.op->token) {
  case '+':
    op = AST_OP_ADD;
    break;
  case '-':
    op = AST_OP_SUB;
    break;
  case '*':
    op = AST_OP_MUL;
    break;
  case '/':
    op = AST_OP_DIV;
    break;
  case '=':
    return ast_assign(lhs, rhs);
  default:
    LOG_ERROR("unknown operator '%c'", node->binop.op->token);
    return NULL;
  }
  
  return make_binop(lhs, op, rhs, lhs->type);
}

static ast_expr_t *ast_constant(const s_node_t *node)
{
  switch (node->constant.lexeme->token) {
  case TK_CONST_INTEGER:
    return make_const_i32(node->constant.lexeme->data.i32);
  case TK_IDENTIFIER:
    return make_expr_lvalue(ast_lvalue(node));
  }
  
  return NULL;
}

static ast_stmt_t *make_stmt_expr(const ast_expr_t *expr, const ast_stmt_t *next)
{
  ast_stmt_t *node = malloc(sizeof(ast_expr_t));
  *node = (ast_stmt_t) { 0 };
  node->stmt_type = AST_STMT_EXPR;
  node->expr = expr;
  node->next = next;
  return node;
}

static ast_stmt_t *make_stmt_decl(const ast_decl_t *decl, const ast_stmt_t *next)
{
  ast_stmt_t *node = malloc(sizeof(ast_expr_t));
  *node = (ast_stmt_t) { 0 };
  node->stmt_type = AST_STMT_DECL;
  node->decl = decl;
  node->next = next;
  return node;
}

static ast_decl_t *make_decl(const type_t *type, const char *ident, const ast_expr_t *init)
{
  ast_decl_t *node = malloc(sizeof(ast_decl_t));
  node->type = type;
  node->ident = ident;
  node->init = init;
  return node;
}

static type_t *make_type(spec_t spec)
{
  type_t *type = malloc(sizeof(type_t));
  type->spec = spec;
  return type;
}

static ast_expr_t *make_expr_lvalue(const ast_lvalue_t *lvalue)
{
  ast_expr_t *node = make_expr(AST_EXPR_LVALUE, lvalue->type);
  node->lvalue = lvalue;
  return node;
}

static ast_expr_t *make_binop(const ast_expr_t *lhs, ast_op_t op, const ast_expr_t *rhs, const type_t *type)
{
  ast_expr_t *node = make_expr(AST_EXPR_BINOP, type);
  node->binop.lhs = lhs;
  node->binop.op = op;
  node->binop.rhs = rhs;
  return node;
}

static ast_expr_t *make_const_i32(int i32)
{
  ast_expr_t *node = make_expr(AST_EXPR_CONST_I32, &type_i32);
  node->i32 = i32;
  return node;
}

static ast_expr_t *make_expr(ast_expr_type_t node_type, const type_t *type)
{
  ast_expr_t *node = malloc(sizeof(ast_expr_t));
  *node = (ast_expr_t) { 0 };
  node->node_type = node_type;
  node->type = type;
  return node;
}
