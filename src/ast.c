#include "ast.h"

#include "log.h"
#include <stdlib.h>

static ast_expr_t *ast_constant(const s_node_t *node);
static ast_expr_t *ast_binop(const s_node_t *node);

static ast_expr_t *make_binop(const ast_expr_t *lhs, ast_op_t op, const ast_expr_t *rhs, const type_t *data_type);
static ast_expr_t *make_const_i32(int i32);
static ast_expr_t *make_expr(ast_expr_type_t node_type, const type_t *type);

type_t type_i32 = { .type_spec = TYPE_SPEC_I32 };

ast_expr_t *ast_parse(const s_node_t *node)
{
  switch (node->node_type) {
  case S_CONSTANT:
    return ast_constant(node);
  case S_BINOP:
    return ast_binop(node);
  }
  
  return NULL;
}

ast_expr_t *ast_binop(const s_node_t *node)
{
  ast_expr_t *lhs = ast_parse(node->binop.lhs);
  ast_expr_t *rhs = ast_parse(node->binop.rhs);
  
  ast_op_t op = 0;
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
  default:
    LOG_ERROR("unknown operator '%c'", node->binop.op->token);
    return NULL;
  }
  
  return make_binop(lhs, op, rhs, lhs->data_type);
}

static ast_expr_t *ast_constant(const s_node_t *node)
{
  switch (node->constant.lexeme->token) {
  case TK_CONST_INTEGER:
    return make_const_i32(node->constant.lexeme->data.i32);
  }
  
  return NULL;
}

static ast_expr_t *make_binop(const ast_expr_t *lhs, ast_op_t op, const ast_expr_t *rhs, const type_t *data_type)
{
  ast_expr_t *node = make_expr(AST_EXPR_BINOP, data_type);
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

static ast_expr_t *make_expr(ast_expr_type_t node_type, const type_t *data_type)
{
  ast_expr_t *node = malloc(sizeof(ast_expr_t));
  *node = (ast_expr_t) { 0 };
  node->node_type = node_type;
  node->data_type = data_type;
  return node;
}
