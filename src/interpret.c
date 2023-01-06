#include "interpret.h"

#include "log.h"

typedef enum {
  SPEC_I32,
  SPEC_F32
} spec_t;

typedef struct {
  spec_t spec;
} type_t;

typedef struct {
  union {
    int   i32;
    float f32;
  };
  type_t type;
} expr_t;

static void i_stmt(const s_node_t *node);
static expr_t i_expr(const s_node_t *node);
static expr_t i_binop(const s_node_t *node);
static expr_t i_constant(const s_node_t *node);

void interpret(const s_node_t *node)
{
  i_stmt(node);
}

void i_stmt(const s_node_t *node)
{
  switch (node->stmt.body->node_type) {
  case S_BINOP:
  case S_CONSTANT:
    i_expr(node);
    break;
  }
}

expr_t i_expr(const s_node_t *node)
{
  switch (node->node_type) {
  case S_BINOP:
    return i_binop(node);
  case S_CONSTANT:
    return i_constant(node);
  }
}

expr_t i_binop(const s_node_t *node)
{
  expr_t lhs = i_expr(node->binop.lhs);
  expr_t rhs = i_expr(node->binop.rhs);
  
  switch (node->binop.op) {
  case '+':
  case '-':
  case '*':
  case '/':
    LOG_DEBUG("%c", node->binop.op);
    break;
  }
}

expr_t i_constant(const s_node_t *node)
{
  expr_t expr;
  switch (node->constant.lexeme->token) {
  case TK_CONST_INTEGER:
    expr.type.spec = SPEC_I32;
    expr.i32 = node->constant.lexeme->data.i32;
    break;
  }
  
  return expr;
}
