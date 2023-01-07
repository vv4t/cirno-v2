#include "interpret.h"

#include "log.h"
#include "map.h"
#include <stdlib.h>

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
  type_t        type;
  expr_t        expr;
  struct var_s  *next;
} var_t;

static void expr_print(const expr_t *expr);

static void   int_stmt(map_t scope, const s_node_t *node);
static void   int_decl(map_t scope, const s_node_t *node);
static type_t int_type(map_t scope, const s_node_t *node);
static expr_t int_expr(map_t scope, const s_node_t *node);
static expr_t int_binop(map_t scope, const s_node_t *node);
static expr_t int_constant(map_t scope, const s_node_t *node);

void interpret(const s_node_t *node)
{
  map_t scope = map_new();
  if (node)
    int_stmt(scope, node);
}

void int_stmt(map_t scope, const s_node_t *node)
{
  expr_t expr;
  switch (node->stmt.body->node_type) {
  case S_BINOP:
  case S_CONSTANT:
    expr = int_expr(scope, node->stmt.body);
    expr_print(&expr);
    break;
  case S_DECL:
    int_decl(scope, node->stmt.body);
    break;
  }
  
  if (node->stmt.next)
    int_stmt(scope, node->stmt.next);
}

void int_decl(map_t scope, const s_node_t *node)
{
  if (map_get(scope, node->decl.ident->data.ident)) {
    LOG_DEBUG("redefinition of '%s'");
    return;
  }
  
  var_t *var = malloc(sizeof(var_t));
  var->type = int_type(scope, node->decl.type);
  var->expr = int_expr(scope, node->decl.init);
  
  map_put(scope, node->decl.ident->data.ident, var);
}

type_t int_type(map_t scope, const s_node_t *node)
{
  type_t type;
  switch (node->type.spec->token) {
  case TK_I32:
    type.spec = SPEC_I32;
    break;
  case TK_F32:
    type.spec = SPEC_F32;
    break;
  }
  
  return type;
}

expr_t int_expr(map_t scope, const s_node_t *node)
{
  switch (node->node_type) {
  case S_BINOP:
    return int_binop(scope, node);
  case S_CONSTANT:
    return int_constant(scope, node);
  }
}

expr_t int_binop(map_t scope, const s_node_t *node)
{
  expr_t lhs = int_expr(scope, node->binop.lhs);
  expr_t rhs = int_expr(scope, node->binop.rhs);
  
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

expr_t int_constant(map_t scope, const s_node_t *node)
{
  var_t *var;
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
  case TK_IDENTIFIER:
    var = map_get(scope, node->constant.lexeme->data.ident);
    expr = var->expr;
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
