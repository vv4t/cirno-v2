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

typedef struct var_s {
  type_t      type;
  expr_t      expr;
  const char  *ident;
  var_s       *next;
} var_t;

typedef struct {
  var_t var[1021];
} sym_t;

static var_t *sym_find(sym_t *sym, const char *ident);

static void expr_print(const expr_t *expr);

static void int_stmt(const s_node_t *node);
static void int_decl(const s_node_t *node);
static expr_t int_expr(const s_node_t *node);
static expr_t int_binop(const s_node_t *node);
static expr_t int_constant(const s_node_t *node);

void interpret(const s_node_t *node)
{
  if (node)
    int_stmt(node);
}

void int_stmt(const s_node_t *node)
{
  expr_t expr;
  switch (node->stmt.body->node_type) {
  case S_BINOP:
  case S_CONSTANT:
    expr = int_expr(node->stmt.body);
    expr_print(&expr);
    break;
  case S_DECL:
    int_decl(node);
    break;
  }
}

void int_decl(const s_node_t *node)
{
  
}

expr_t int_expr(const s_node_t *node)
{
  switch (node->node_type) {
  case S_BINOP:
    return int_binop(node);
  case S_CONSTANT:
    return int_constant(node);
  }
}

expr_t int_binop(const s_node_t *node)
{
  expr_t lhs = int_expr(node->binop.lhs);
  expr_t rhs = int_expr(node->binop.rhs);
  
  expr_t expr;
  if (lhs.type.spec == SPEC_I32 && rhs.type.spec == SPEC_I32) {
    switch (node->binop.op->token) {
    case '+':
      expr.i32 = lhs.i32 + rhs.i32;
      break;
    case '-':
      expr.i32 = lhs.i32 - rhs.i32;
      break;
    case '*':
      expr.i32 = lhs.i32 * rhs.i32;
      break;
    case '/':
      expr.i32 = lhs.i32 / rhs.i32;
      break;
    }
    expr.type.spec = SPEC_I32;
  } else if (lhs.type.spec == SPEC_F32 && rhs.type.spec == SPEC_F32) {
    switch (node->binop.op->token) {
    case '+':
      expr.f32 = lhs.f32 + rhs.f32;
      break;
    case '-':
      expr.f32 = lhs.f32 - rhs.f32;
      break;
    case '*':
      expr.f32 = lhs.f32 * rhs.f32;
      break;
    case '/':
      expr.f32 = lhs.f32 / rhs.f32;
      break;
    }
    expr.type.spec = SPEC_F32;
  } else {
    
  }
  
  return expr;
}

expr_t int_constant(const s_node_t *node)
{
  expr_t expr;
  switch (node->constant.lexeme->token) {
  case TK_CONST_INTEGER:
    expr.type.spec = SPEC_I32;
    expr.i32 = node->constant.lexeme->data.i32;
    break;
  case TK_CONST_FLOAT:
    expr.type.spec = SPEC_F32;
    expr.f32 = node->constant.lexeme->data.f32;
    break;
  }
  
  return expr;
}

void expr_print(const expr_t *expr)
{
  switch (expr->type.spec) {
  case SPEC_I32:
    printf("%i\n", expr->i32);
    break;
  case SPEC_F32:
    printf("%f\n", expr->f32);
    break;
  }
}
