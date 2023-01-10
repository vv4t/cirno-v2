#include "interpret.h"

#include "log.h"
#include "map.h"
#include <stdlib.h>
#include <stdbool.h>

static type_t type_i32 = { .spec = SPEC_I32, .size = 0 };
static type_t type_f32 = { .spec = SPEC_F32, .size = 0 };

static bool type_cmp(const type_t *a, const type_t *b);
static int  type_size_base(const type_t *type);

static bool expr_lvalue(const expr_t *expr);

static void   int_stmt(map_t scope, const s_node_t *node);
static void   int_decl(map_t scope, const s_node_t *node);
static type_t int_type(map_t scope, const s_node_t *node);
static expr_t int_expr(map_t scope, const s_node_t *node);
static expr_t int_binop(map_t scope, const s_node_t *node);
static expr_t int_constant(map_t scope, const s_node_t *node);

static int mem_stack[128];
static int mem_ptr = 0;

static int *mem_alloc(int size);

expr_t int_shell(const s_node_t *node)
{
  map_t scope = map_new();
  
  expr_t expr = { 0 };
  const s_node_t *head = node;
  while (head) {
    switch (head->stmt.body->node_type) {
    case S_BINOP:
    case S_CONSTANT:
      expr = int_expr(scope, head->stmt.body);
      break;
    case S_DECL:
      int_decl(scope, head->stmt.body);
      break;
    }
    head = head->stmt.next;
  }
  
  return expr;
}

void int_decl(map_t scope, const s_node_t *node)
{
  if (map_get(scope, node->decl.ident->data.ident))
    c_error(node->decl.ident, "redefinition of '%s'", node->decl.ident->data.ident);
  
  if (!node->decl.init && !node->decl.type->type.size)
    c_error(node->decl.ident, "'%s' is uninitialized", node->decl.ident->data.ident);
  
  var_t *var = malloc(sizeof(var_t));
  var->type = int_type(scope, node->decl.type);
  var->expr = (expr_t) { 0 };
  
  if (node->decl.init) {
    var->expr = int_expr(scope, node->decl.init);
    if (var->type.spec != var->expr.type.spec) {
      c_error(
        node->decl.type->type.spec,
        "incompatible types: initializing '%z' using '%z'",
        &var->type, &var->expr.type);
    }
  } else if (var->type.size > 0) {
    var->expr.type = var->type;
    var->expr.mem = mem_alloc(var->type.size * type_size_base(&var->expr.type));
  }
  
  var->expr.lvalue.var = var;
  
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
  
  if (node->type.size) {
    expr_t size = int_expr(scope, node->type.size);
    if (!type_cmp(&size.type, &type_i32))
      c_error(node->type.spec, "size of array has non-integer type");
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
  
  expr_t expr = { 0 };
  if (type_cmp(&lhs.type, &type_i32) && type_cmp(&rhs.type, &type_i32)) {
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
    case '=':
      if (!expr_lvalue(&lhs))
        c_error(node->binop.op, "lvalue required as left operand of assignment");
      lhs.lvalue.var->expr.i32 = rhs.i32;
      expr = lhs.lvalue.var->expr;
      break;
    default:
      goto err_no_op; // lol lmao
    }
    expr.type = type_i32;
  } if (type_cmp(&lhs.type, &type_f32) && type_cmp(&rhs.type, &type_f32)) {
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
    case '=':
      if (!expr_lvalue(&lhs))
        c_error(node->binop.op, "lvalue required as left operand of assignment");
      lhs.lvalue.var->expr.f32 = rhs.f32;
      expr = lhs.lvalue.var->expr;
      break;
    default:
      goto err_no_op; // lol lmao
    }
    expr.type = type_f32;
  } else {
err_no_op: // i actually used a goto lol
    c_error(
      node->binop.op,
      "unknown operand type for '%t': '%z' and '%z'",
      node->binop.op->token,
      &lhs.type, &rhs.type);
  }
  
  return expr;
}

expr_t int_constant(map_t scope, const s_node_t *node)
{
  var_t *var;
  expr_t expr = { 0 };
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
    
    if (!var) {
      c_error(
        node->constant.lexeme,
        "'%s' undeclared",
        node->constant.lexeme->data.ident);
    }
    
    expr = var->expr;
    break;
  }
  
  return expr;
}

static bool type_cmp(const type_t *a, const type_t *b)
{
  return a->spec == b->spec && a->size == b->size;
}

static bool expr_lvalue(const expr_t *expr)
{
  return expr->lvalue.var != NULL;
}
  
static int type_size_base(const type_t *type)
{
  switch (type->spec) {
  case SPEC_I32:
    return 4;
  case SPEC_F32:
    return 4;
  }
}

static int *mem_alloc(int size)
{
  int *block = &mem_stack[mem_ptr];
  mem_ptr += (size + 3) / 4;
  return block;
}
