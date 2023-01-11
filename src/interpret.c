#include "interpret.h"

#include "log.h"
#include "map.h"
#include <stdlib.h>
#include <stdbool.h>

static type_t type_i32 = { .spec = SPEC_I32, .size = 0 };
static type_t type_f32 = { .spec = SPEC_F32, .size = 0 };

static bool type_cmp(const type_t *a, const type_t *b);
static bool type_array(const type_t *type);
static int  type_size(const type_t *type);
static int  type_size_base(const type_t *type);

static void   int_stmt(map_t scope, const s_node_t *node);
static void   int_decl(map_t scope, const s_node_t *node);
static type_t int_type(map_t scope, const s_node_t *node);
static expr_t int_expr(map_t scope, const s_node_t *node);
static expr_t int_index(map_t scope, const s_node_t *node);
static expr_t int_binop(map_t scope, const s_node_t *node);
static expr_t int_constant(map_t scope, const s_node_t *node);

static char mem_stack[128];
static int  mem_ptr = 0;
static char *mem_alloc(int size);

expr_t int_shell(const s_node_t *node)
{
  map_t scope = map_new();
  
  expr_t expr;
  const s_node_t *head = node;
  while (head) {
    switch (head->stmt.body->node_type) {
    case S_BINOP:
    case S_CONSTANT:
    case S_INDEX:
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
  
  var_t *var = malloc(sizeof(var_t));
  var->type = int_type(scope, node->decl.type);
  var->loc = mem_alloc(type_size(&var->type));
  
  if (node->decl.init) {
    expr_t expr = int_expr(scope, node->decl.init);
    if (!type_cmp(&var->type, &expr.type)) {
      c_error(
        node->decl.type->type.spec,
        "incompatible types when initializing '%z' with '%z'",
        &var->type, &expr.type);
    }
    
    if (type_cmp(&var->type, &type_i32))
      *((int*) var->loc) = expr.i32;
    else if (type_cmp(&var->type, &type_f32))
      *((float*) var->loc) = expr.f32;
  }
  
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
    type.size = size.i32;
  } else {
    type.size = 0;
  }
  
  return type;
}

expr_t int_expr(map_t scope, const s_node_t *node)
{
  switch (node->node_type) {
  case S_BINOP:
    return int_binop(scope, node);
  case S_INDEX:
    return int_index(scope, node);
  case S_CONSTANT:
    return int_constant(scope, node);
  }
}

expr_t int_binop(map_t scope, const s_node_t *node)
{
  expr_t lhs = int_expr(scope, node->binop.lhs);
  expr_t rhs = int_expr(scope, node->binop.rhs);
  
  expr_t expr;
  if (type_cmp(&lhs.type, &type_i32) && type_cmp(&rhs.type, &type_i32)) {
    expr.type = type_i32;
    expr.loc = NULL;
    
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
      if (!lhs.loc)
        c_error(node->binop.op, "lvalue required as left operand of assignment");
      *((int*)lhs.loc) = rhs.i32;
      lhs.i32 = rhs.i32;
      expr = lhs; 
      break;
    default:
      goto err_no_op; // lol lmao
    }
  } if (type_cmp(&lhs.type, &type_f32) && type_cmp(&rhs.type, &type_f32)) {
    expr.type = type_f32;
    expr.loc = NULL;
    
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
      if (!lhs.loc)
        c_error(node->binop.op, "lvalue required as left operand of assignment");
      *((float*)lhs.loc) = rhs.f32;
      lhs.f32 = rhs.f32;
      expr = lhs; 
      break;
    default:
      goto err_no_op; // lol lmao
    }
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

expr_t int_index(map_t scope, const s_node_t *node)
{
  expr_t base = int_expr(scope, node->index.base);
  if (!type_array(&base.type))
    c_error(node->index.left_bracket, "subscripted value is not an array");
  
  expr_t index = int_expr(scope, node->index.index);
  if (!type_cmp(&index.type, &type_i32))
    c_error(node->index.left_bracket, "array subscript is not an integer");
  
  base.type.size = 0;
  base.loc += index.i32 * type_size_base(&base.type);
  
  if (type_cmp(&base.type, &type_i32))
    base.i32 = *((int*) base.loc);
  else if (type_cmp(&base.type, &type_f32))
    base.f32 = *((float*) base.loc);
  
  return base;
}

expr_t int_constant(map_t scope, const s_node_t *node)
{
  var_t *var;
  expr_t expr;
  switch (node->constant.lexeme->token) {
  case TK_CONST_INTEGER:
    expr.type = type_i32;
    expr.i32 = node->constant.lexeme->data.i32;
    expr.loc = NULL;
    break;
  case TK_CONST_FLOAT:
    expr.type = type_f32;
    expr.f32 = node->constant.lexeme->data.f32;
    expr.loc = NULL;
    break;
  case TK_IDENTIFIER:
    var = map_get(scope, node->constant.lexeme->data.ident);
    
    if (!var) {
      c_error(
        node->constant.lexeme,
        "'%s' undeclared",
        node->constant.lexeme->data.ident);
    }
    
    if (type_cmp(&var->type, &type_i32))
      expr.i32 = *((int*) var->loc);
    else if (type_cmp(&var->type, &type_f32))
      expr.f32 = *((float*) var->loc);
    
    expr.loc = var->loc;
    expr.type = var->type;
    break;
  }
  
  return expr;
}

static bool type_cmp(const type_t *a, const type_t *b)
{
  return a->spec == b->spec && a->size == b->size;
}

static bool type_array(const type_t *type)
{
  return type->size > 0;
}

static int type_size(const type_t *type)
{
  if (type->size > 0)
    return type_size_base(type) * type->size;
  else
    return type_size_base(type);
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

static char *mem_alloc(int size)
{
  char *block = &mem_stack[mem_ptr];
  mem_ptr += size;
  return block;
}
