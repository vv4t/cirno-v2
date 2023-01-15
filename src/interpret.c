#include "interpret.h"

#include "log.h"

typedef struct {
  union {
    int   i32;
    float f32;
  };
} tmp_t;

static bool int_expr(tmp_t *tmp, const ast_expr_t *ast_expr);
static bool int_binop(tmp_t *tmp, const ast_expr_t *ast_expr);

bool interpret(const ast_stmt_t *node)
{
  tmp_t tmp;
  
  const ast_stmt_t *head = node;
  while (head) {
    switch (head->ast_stmt_type) {
    case AST_STMT_EXPR:
      if (!int_expr(&tmp, head->expr))
        return false;
      LOG_DEBUG("%i", tmp.i32);
      break;
    }
    head = head->next;
  }
  
  return true;
}

static bool int_expr(tmp_t *tmp, const ast_expr_t *ast_expr)
{
  switch (ast_expr->ast_expr_type) {
  case AST_EXPR_I32:
    tmp->i32 = ast_expr->i32;
    return true;
  case AST_EXPR_F32:
    tmp->f32 = ast_expr->f32;
    return true;
  case AST_EXPR_BINOP:
    return int_binop(tmp, ast_expr);
  default:
    LOG_ERROR("unknown");
    return false;
  }
}

static bool int_binop(tmp_t *tmp, const ast_expr_t *ast_expr)
{
  tmp_t lhs;
  if (!int_expr(&lhs, ast_expr->binop.lhs))
    return false;
  
  tmp_t rhs;
  if (!int_expr(&rhs, ast_expr->binop.rhs))
    return false;
  
  switch (ast_expr->binop.op) {
  case AST_OP_ADD_I32:
    tmp->i32 = lhs.i32 + rhs.i32;
    break;
  case AST_OP_SUB_I32:
    tmp->i32 = lhs.i32 - rhs.i32;
    break;
  case AST_OP_MUL_I32:
    tmp->i32 = lhs.i32 * rhs.i32;
    break;
  case AST_OP_DIV_I32:
    tmp->i32 = lhs.i32 / rhs.i32;
    break;
  case AST_OP_ADD_F32:
    tmp->f32 = lhs.f32 + rhs.f32;
    break;
  case AST_OP_SUB_F32:
    tmp->f32 = lhs.f32 - rhs.f32;
    break;
  case AST_OP_MUL_F32:
    tmp->f32 = lhs.f32 * rhs.f32;
    break;
  case AST_OP_DIV_F32:
    tmp->f32 = lhs.f32 / rhs.f32;
    break;
  default:
    LOG_ERROR("unknown");
    break;
  }
  
  return true;
}
