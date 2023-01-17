#include "interpret.h"

#include "log.h"
#include "zone.h"

static bool int_fn(scope_t *scope, const s_node_t *node);
static bool int_body(scope_t *scope, const s_node_t *node);
static bool int_stmt(scope_t *scope, const s_node_t *node);
static bool int_print(scope_t *scope, const s_node_t *node);
static bool int_ret_stmt(scope_t *scope, const s_node_t *node);
static bool int_if_stmt(scope_t *scope, const s_node_t *node);
static bool int_while_stmt(scope_t *scope, const s_node_t *node);
static bool int_decl(scope_t *scope, const s_node_t *node);
static bool int_class_def(scope_t *scope, const s_node_t *node);

static bool int_type(scope_t *scope, type_t *type, const s_node_t *node);

static bool int_expr(scope_t *scope, expr_t *expr, const s_node_t *node);
static bool int_unary(scope_t *scope, expr_t *expr, const s_node_t *node);
static bool int_index(scope_t *scope, expr_t *expr, const s_node_t *node);
static bool int_direct(scope_t *scope, expr_t *expr, const s_node_t *node);
static bool int_proc(scope_t *scope, expr_t *expr, const s_node_t *node);
static bool int_binop(scope_t *scope, expr_t *expr, const s_node_t *node);
static bool int_constant(scope_t *scope, expr_t *expr, const s_node_t *node);
static bool int_new(scope_t *scope, expr_t *expr, const s_node_t *node);

static char mem_stack[512];
static void mem_load(char *loc_base, int loc_offset, const type_t *type, expr_t *expr);
static void mem_assign(char *loc_base, int loc_offset, const type_t *type, expr_t *expr);
static int  mem_stack_end = 256;

typedef struct heap_block_s {
  char  *block;
  bool  use;
  
  struct heap_block_s *next;
  struct heap_block_s *prev;
} heap_block_t;

static heap_block_t  *heap_block_list = NULL;

static heap_block_t  *heap_alloc(int size);
static void         heap_clean();
static void         heap_clean_R(scope_t *scope);

static scope_t      scope_global;

bool interpret(const s_node_t *node)
{
  scope_new(&scope_global, &type_none, NULL, NULL);
  
  bool err = int_body(&scope_global, node);
  
  scope_free(&scope_global);
  scope_global.scope_child = NULL;
  
  return err;
}

bool int_body(scope_t *scope, const s_node_t *node)
{
  scope_t new_scope;
  scope_new(&new_scope, &scope->ret_type, scope, scope);
  new_scope.size = scope->size;
  
  const s_node_t *head = node;
  while (head && !new_scope.ret_flag) {
    if (!int_stmt(&new_scope, head)) {
      scope_free(&new_scope);
      scope->scope_child = NULL;
      heap_clean();
      return false;
    }
    
    head = head->stmt.next;
  }
  
  scope->ret_flag = new_scope.ret_flag;
  scope->ret_value = new_scope.ret_value;
  
  heap_clean();
  
  scope_free(&new_scope);
  scope->scope_child = NULL;
  
  return true;
}

bool int_stmt(scope_t *scope, const s_node_t *node)
{
  expr_t expr;
  switch (node->stmt.body->node_type) {
  case S_BINOP:
  case S_CONSTANT:
  case S_INDEX:
  case S_DIRECT:
  case S_UNARY:
  case S_PROC:
  case S_NEW:
    if (!int_expr(scope, &expr, node->stmt.body))
      return false;
    break;
  case S_DECL:
    if (!int_decl(scope, node->stmt.body))
      return false;
    break;
  case S_CLASS_DEF:
    if (!int_class_def(scope, node->stmt.body))
      return false;
    break;
  case S_PRINT:
    if (!int_print(scope, node->stmt.body))
      return false;
    break;
  case S_IF_STMT:
    if (!int_if_stmt(scope, node->stmt.body))
      return false;
    break;
  case S_WHILE_STMT:
    if (!int_while_stmt(scope, node->stmt.body))
      return false;
    break;
  case S_FN:
    if (!int_fn(scope, node->stmt.body))
      return false;
    break;
  case S_RET_STMT:
    if (!int_ret_stmt(scope, node->stmt.body))
      return false;
    break;
  default:
    LOG_ERROR("unknown statement node_type (%i)", node->stmt.body->node_type);
    break;
  }
  
  return true;
}

bool int_fn(scope_t *scope, const s_node_t *node)
{
  if (scope_find_fn(scope, node->fn.fn_ident->data.ident)) {
    c_error(node->fn.fn_ident, "redefinition of function '%s'", node->fn.fn_ident->data.ident);
    return false;
  }
  
  type_t type = type_none;
  if (node->fn.type) {
    if (!int_type(scope, &type, node->fn.type))
      return false;
  }
  
  scope_add_fn(
    scope,
    &type,
    node->fn.param_decl,
    node->fn.body,
    node->fn.fn_ident->data.ident);
  
  return true;
}

bool int_while_stmt(scope_t *scope, const s_node_t *node)
{
  expr_t cond;
  if (!int_expr(scope, &cond, node->while_stmt.cond))
    return false;
  
  while (cond.i32 != 0) {
    if (!int_body(scope, node->while_stmt.body))
      return false;
    
    if (!int_expr(scope, &cond, node->while_stmt.cond))
      return false;
  }
  
  return true;
}

bool int_if_stmt(scope_t *scope, const s_node_t *node)
{
  expr_t cond;
  if (!int_expr(scope, &cond, node->if_stmt.cond))
    return false;
  
  if (cond.i32 != 0) {
    if (!int_body(scope, node->if_stmt.body))
      return false;
  }
  
  return true;
}

bool int_ret_stmt(scope_t *scope, const s_node_t *node)
{
  expr_t expr;
  if (!int_expr(scope, &expr, node->ret_stmt.body))
    return false;
  
  if (!type_cmp(&expr.type, &scope->ret_type)) {
    c_error(
      node->ret_stmt.ret_token,
      "incompatible types when returning type '%z' but '%z' was expected",
      &expr.type,
      &scope->ret_type);
    return false;
  }
  
  scope->ret_flag = true;
  scope->ret_value = expr;
  
  return true;
}

bool int_print(scope_t *scope, const s_node_t *node)
{
  s_node_t *arg = node->print.arg;
  
  c_debug("out: ");
  
  while (arg) {
    expr_t expr;
    if (!int_expr(scope, &expr, arg->arg.body))
      return false;
    
    c_debug("%w ", &expr);
    arg = arg->arg.next;
  }
  
  c_debug("\n");
  
  return true;
}

bool int_class_def(scope_t *scope, const s_node_t *node)
{
  if (scope_find_class(scope, node->class_def.ident->data.ident)) {
    c_error(node->decl.ident, "redefinition of class '%s'", node->class_def.ident->data.ident);
    return false;
  }
  
  class_t class;
  class_new(&class, node->class_def.ident->data.ident);
  
  s_node_t *head = node->class_def.class_decl;
  while (head) {
    type_t type;
    if (!int_type(scope, &type, head->class_decl.type))
      return false;
    
    if (class_find_var(&class, head->class_decl.ident->data.ident)) {
      c_error(
        node->class_def.ident,
        "redefinition of '%s' in class '%s'",
        head->class_decl.ident->data.ident,
        node->class_def.ident->data.ident);
      return false;
    }
    
    class_add_var(&class, &type, head->class_decl.ident->data.ident);
    head = head->class_decl.next;
  }
  
  scope_add_class(scope, node->class_def.ident->data.ident, &class);
  
  return true;
}

bool int_decl(scope_t *scope, const s_node_t *node)
{
  if (scope_find_var(scope, node->decl.ident->data.ident)) {
    c_error(node->decl.ident, "redefinition of '%s'", node->decl.ident->data.ident);
    return false;
  }
  
  type_t type;
  if (!int_type(scope, &type, node->decl.type))
    return false;
  
  expr_t expr;
  if (node->decl.init) {
    if (!int_expr(scope, &expr, node->decl.init))
      return false;
    
    if (!type_cmp(&type, &expr.type)) {
      c_error(
        node->decl.type->type.spec,
        "incompatible types when initializing '%z' with '%z'",
        &type, &expr.type);
      return false;
    }
  } else {
    expr = (expr_t) {0};
  }
  
  var_t *var = scope_add_var(scope, &type, node->decl.ident->data.ident);
  
  if (scope->size >= mem_stack_end) {
    LOG_ERROR("ran out of memory %i/%i", scope->size, mem_stack_end);
    return false;
  }
  
  mem_assign(mem_stack, var->loc, &var->type, &expr);
  
  return true;
}

bool int_type(scope_t *scope, type_t *type, const s_node_t *node)
{
  type->class = NULL;
  switch (node->type.spec->token) {
  case TK_I32:
    type->spec = SPEC_I32;
    break;
  case TK_F32:
    type->spec = SPEC_F32;
    break;
  case TK_CLASS:
    type->spec = SPEC_CLASS;
    
    type->class = scope_find_class(scope, node->type.class_ident->data.ident);
    if (!type->class) {
      c_error(
        node->type.class_ident,
        "use of undefined class '%s'",
        node->type.class_ident->data.ident);
      return false;
    }
    
    break;
  }
  
  if (node->type.size) {
    expr_t size;
    
    if (!int_expr(scope, &size, node->type.size))
      return false;
    
    if (!type_cmp(&size.type, &type_i32)) {
      c_error(node->type.spec, "size of array has non-integer type");
      return false;
    }
    
    type->size = size.i32;
  } else {
    type->size = 0;
  }
  
  return true;
}

bool int_expr(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  switch (node->node_type) {
  case S_BINOP:
    return int_binop(scope, expr, node);
  case S_UNARY:
    return int_unary(scope, expr, node);
  case S_INDEX:
    return int_index(scope, expr, node);
  case S_DIRECT:
    return int_direct(scope, expr, node);
  case S_PROC:
    return int_proc(scope, expr, node);
  case S_CONSTANT:
    return int_constant(scope, expr, node);
  case S_NEW:
    return int_new(scope, expr, node);
  default:
    LOG_ERROR("unknown expr node_type (%i)", node->node_type);
    return false;
  }
}

bool int_proc(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  fn_t *fn = scope_find_fn(scope, node->proc.func_ident->data.ident);
  
  if (!fn) {
    c_error(
      node->proc.func_ident,
      "'%s' undeclared",
      node->proc.func_ident->data.ident);
    return false;
  }
  
  scope_t new_scope;
  scope_new(&new_scope, &fn->type, scope, fn->scope_parent);
  new_scope.ret_type = fn->type;
  new_scope.size += scope->size;
  
  s_node_t *arg = node->proc.arg;
  s_node_t *head = fn->param;
  while (head) {
    if (!arg) {
      c_error(
        node->proc.func_ident,
        "too few arguments to function '%s'",
        node->proc.func_ident->data.ident);
      return false;
    }
    
    type_t type;
    if (!int_type(&new_scope, &type, head->param_decl.type))
      return false;
    
    expr_t arg_value;
    if (!int_expr(scope, &arg_value, arg->arg.body))
      return false;
    
    if (!type_cmp(&type, &arg_value.type)) {
      c_error(
        head->param_decl.ident,
        "expected '%z' but argument is of type '%z'",
        &type,
        &arg_value.type);
      return false;
    }
    
    if (scope_find_var(&new_scope, head->param_decl.ident->data.ident)) {
      c_error(
        head->param_decl.ident,
        "redefinition of param '%s'",
        head->param_decl.ident->data.ident);
      return false;
    }
    
    var_t *var = scope_add_var(&new_scope, &type, head->param_decl.ident->data.ident);
    mem_assign(mem_stack, var->loc, &var->type, &arg_value);
    
    head = head->param_decl.next;
    arg = arg->arg.next;
  }
  
  if (arg) {
    c_error(
      node->proc.func_ident,
      "too many arguments to function '%s'",
      node->proc.func_ident->data.ident);
    return false;
  }
  
  if (!int_body(&new_scope, fn->node)) {
err_cleanup:
    scope_free(&new_scope);
    scope->scope_child = NULL;
    return false;
  }
  
  *expr = new_scope.ret_value;
  scope_free(&new_scope);
  scope->scope_child = NULL;
  
  return true;
}

bool int_unary(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t base;
  switch (node->unary.op->token) {
  default:
    break;
  }
  return true;
}

bool int_binop(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t lhs;
  if (!int_expr(scope, &lhs, node->binop.lhs))
    return false;
  
  expr_t rhs;
  if (!int_expr(scope, &rhs, node->binop.rhs))
    return false;
  
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  if (type_cmp(&lhs.type, &type_i32) && type_cmp(&rhs.type, &type_i32)) {
    expr->type = type_i32;
    switch (node->binop.op->token) {
    case '+':
      expr->i32 = lhs.i32 + rhs.i32;
      break;
    case '-':
      expr->i32 = lhs.i32 - rhs.i32;
      break;
    case '*':
      expr->i32 = lhs.i32 * rhs.i32;
      break;
    case '/':
      expr->i32 = lhs.i32 / rhs.i32;
      break;
    case '<':
      expr->i32 = lhs.i32 < rhs.i32;
      break;
    case '>':
      expr->i32 = lhs.i32 > rhs.i32;
      break;
    case '=':
      if (!expr_lvalue(&lhs)) {
        c_error(node->binop.op, "lvalue required as left operand of assignment");
        return false;
      }
      
      mem_assign(lhs.loc_base, lhs.loc_offset, &type_i32, &rhs);
      *expr = lhs;
      expr->i32 = rhs.i32;
      
      break;
    default:
      goto err_no_op;
    }
  } else if (type_cmp(&lhs.type, &type_f32) && type_cmp(&rhs.type, &type_f32)) {
    expr->type = type_f32;
    switch (node->binop.op->token) {
    case '+':
      expr->f32 = lhs.f32 + rhs.f32;
      break;
    case '-':
      expr->f32 = lhs.f32 - rhs.f32;
      break;
    case '*':
      expr->f32 = lhs.f32 * rhs.f32;
      break;
    case '/':
      expr->f32 = lhs.f32 / rhs.f32;
      break;
    case '<':
      expr->type = type_i32;
      expr->i32 = lhs.f32 < rhs.f32;
      break;
    case '>':
      expr->type = type_i32;
      expr->i32 = lhs.f32 > rhs.f32;
      break;
    case '=':
      if (!expr_lvalue(&lhs)) {
        c_error(node->binop.op, "lvalue required as left operand of assignment");
        return false;
      }
      
      mem_assign(lhs.loc_base, lhs.loc_offset, &type_f32, &rhs);
      *expr = lhs;
      expr->f32 = rhs.f32;
      
      break;
    default:
      goto err_no_op;
    }
  } else {
err_no_op:
    c_error(
      node->binop.op,
      "unknown operand type for '%t': '%z' and '%z'",
      node->binop.op->token,
      &lhs.type, &rhs.type);
    return false;
  }
  
  return true;
}

bool int_direct(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t base;
  if (!int_expr(scope, &base, node->direct.base))
    return false;
  
  if (base.type.spec != SPEC_CLASS || type_array(&base.type)) {
    c_error(
      node->direct.child_ident,
      "request for member '%s' in non-class",
      node->direct.child_ident->data.ident);
    return false;
  }
  
  if (!base.align_of) {
    c_error(
      node->direct.child_ident,
      "request for member '%s' in uninitialised class",
      node->direct.child_ident->data.ident);
    return false;
  }
  
  var_t *var = class_find_var(base.type.class, node->direct.child_ident->data.ident);
  if (!var) {
    c_error(
      node->direct.child_ident,
      "'class %s' has no member named '%s'",
      base.type.class->ident,
      node->direct.child_ident->data.ident);
    return false;
  }
  
  heap_block_t *heap_block = (heap_block_t*) base.align_of;
  
  expr->type = var->type;
  expr->loc_base = heap_block->block;
  expr->loc_offset = var->loc;
  mem_load(expr->loc_base, expr->loc_offset, &expr->type, expr);
  
  return true;
}

bool int_index(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t base;
  if (!int_expr(scope, &base, node->index.base))
    return false;
  
  if (!type_array(&base.type)) {
    c_error(node->index.left_bracket, "subscripted value is not an array");
    return false;
  }
  
  expr_t index;
  if (!int_expr(scope, &index, node->index.index))
    return false;
  
  if (!type_cmp(&index.type, &type_i32)) {
    c_error(node->index.left_bracket, "array subscript is not an integer");
    return false;
  }
  
  *expr = base;
  expr->type.size = 0;
  expr->loc_offset += index.i32 * type_size_base(&base.type);
  mem_load(expr->loc_base, expr->loc_offset, &expr->type, expr);
  
  return true;
}

bool int_new(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  class_t *class = scope_find_class(scope, node->new.class_ident->data.ident);
  
  if (!class) {
    c_error(
      node->new.class_ident,
      "undeclared class '%s'",
      node->new.class_ident->data.ident);
    return false;
  }
  
  expr->type.spec = SPEC_CLASS;
  expr->type.size = 0;
  expr->type.class = class;
  expr->align_of = (size_t) heap_alloc(class->size);
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  
  return true;
}

bool int_constant(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  var_t *var;
  switch (node->constant.lexeme->token) {
  case TK_CONST_INTEGER:
    expr->type = type_i32;
    expr->i32 = node->constant.lexeme->data.i32;
    expr->loc_base = NULL;
    expr->loc_offset = 0;
    break;
  case TK_CONST_FLOAT:
    expr->type = type_f32;
    expr->f32 = node->constant.lexeme->data.f32;
    expr->loc_base = NULL;
    expr->loc_offset = 0;
    break;
  case TK_IDENTIFIER:
    var = scope_find_var(scope, node->constant.lexeme->data.ident);
    
    if (!var) {
      c_error(
        node->constant.lexeme,
        "'%s' undeclared",
        node->constant.lexeme->data.ident);
      return false;
    }
    
    mem_load(mem_stack, var->loc, &var->type, expr);
    break;
  }
  
  return true;
}

static void mem_load(char *loc_base, int loc_offset, const type_t *type, expr_t *expr)
{
  expr->loc_base = loc_base;
  expr->loc_offset = loc_offset;
  expr->type = *type;
  
  if (type_class(type))
    expr->align_of = *((size_t*) &loc_base[loc_offset]);
  else if (type_cmp(type, &type_i32))
    expr->i32 = *((int*) &loc_base[loc_offset]);
  else if (type_cmp(type, &type_f32))
    expr->f32 = *((float*) &loc_base[loc_offset]);
}

static void mem_assign(char *loc_base, int loc_offset, const type_t *type, expr_t *expr)
{
  if (type_class(type))
    *((unsigned long long*) &loc_base[loc_offset]) = (size_t) expr->align_of;
  else if (type_cmp(type, &type_i32))
    *((int*) &loc_base[loc_offset]) = expr->i32;
  else if (type_cmp(type, &type_f32))
    *((float*) &loc_base[loc_offset]) = expr->f32;
  else if (type_cmp(type, &type_none))
    *((size_t*) &loc_base[loc_offset]) = 0;
}

static heap_block_t *heap_alloc(int size)
{
  heap_block_t *heap_block = ZONE_ALLOC(sizeof(heap_block_t));
  heap_block->block = ZONE_ALLOC(size);
  heap_block->use = false;
  
  if (heap_block_list) {
    heap_block->next = heap_block_list;
    heap_block->prev = NULL;
    heap_block_list->prev = heap_block;
    heap_block_list = heap_block;
  } else {
    heap_block_list = heap_block;
    heap_block_list->prev = NULL;
    heap_block_list->next = NULL;
  }
  
  return heap_block;
}

static void heap_clean()
{
  heap_block_t *heap_block = heap_block_list;
  while (heap_block) {
    heap_block->use = false;
    heap_block = heap_block->next;
  }
  
  heap_clean_R(&scope_global);
  
  heap_block = heap_block_list;
  while (heap_block) {
    if (!heap_block->use) {
      if (heap_block->next)
        heap_block->next->prev = heap_block->prev;
      if (heap_block->prev)
        heap_block->prev->next = heap_block->next;
      
      if (heap_block == heap_block_list)
        heap_block_list = heap_block->next;
      
      ZONE_FREE(heap_block->block);
      ZONE_FREE(heap_block);
    }
    
    heap_block = heap_block->next;
  }
}

static void heap_clean_R(scope_t *scope)
{
  if (!scope)
    return;
  
  entry_t *entry = scope->map_var.start;
  while (entry) {
    var_t *var = (var_t*) entry->value;
    
    if (type_class(&var->type)) {
      expr_t expr;
      mem_load(mem_stack, var->loc, &var->type, &expr);
      
      heap_block_t *heap_block = (heap_block_t*) expr.align_of;
      
      if (heap_block)
        heap_block->use = true;
    }
    
    entry = entry->s_next;
  }
  
  heap_clean_R(scope->scope_child);
}
