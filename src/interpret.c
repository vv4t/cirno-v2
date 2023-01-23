#include "interpret.h"

#include "log.h"
#include "zone.h"
#include <string.h>

typedef struct heap_block_s {
  char  *block;
  bool  use;
  int   size;
  
  struct heap_block_s *next;
  struct heap_block_s *prev;
} heap_block_t;

static heap_block_t *heap_block_list = NULL;
static heap_block_t *heap_alloc(int size);
static void         heap_clean();
static void         heap_clean_R(scope_t *scope);
static void         heap_clean_class(heap_block_t *heap_block, const scope_t *class);
static void         heap_clean_array_class(heap_block_t *heap_block, const scope_t *class);
static void         heap_clean_var(var_t *var);
static void         heap_clean_expr(expr_t *expr);

static scope_t  scope_global;

static char mem_stack[512];
static void mem_load(char *loc_base, int loc_offset, const type_t *type, expr_t *expr);
static void mem_assign(char *loc_base, int loc_offset, const type_t *type, expr_t *expr);
static int  mem_stack_end = 256;

static bool int_fn(scope_t *scope, const s_node_t *node, const scope_t *scope_class);
static bool int_body(scope_t *scope, const s_node_t *node);
static bool int_body_scope(scope_t *scope, const s_node_t *node);
static bool int_stmt(scope_t *scope, const s_node_t *node);
static bool int_print(scope_t *scope, const s_node_t *node);
static bool int_ret_stmt(scope_t *scope, const s_node_t *node);
static bool int_if_stmt(scope_t *scope, const s_node_t *node);
static bool int_while_stmt(scope_t *scope, const s_node_t *node);
static bool int_for_stmt(scope_t *scope, const s_node_t *node);
static bool int_decl(scope_t *scope, const s_node_t *node, bool init);
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
static bool int_array_init(scope_t *scope, expr_t *expr, const s_node_t *node);
static bool int_post_op(scope_t *scope, expr_t *expr, const s_node_t *node);

static bool int_load_ident(const scope_t *scope, heap_block_t *heap_block, expr_t *expr, const lexeme_t *lexeme);

void int_init()
{
  scope_new(&scope_global, NULL, &type_none, NULL, NULL, false);
}

bool int_run(const s_node_t *node)
{
  return int_body(&scope_global, node);
}

void int_stop()
{
  scope_free(&scope_global);
  scope_global.scope_child = NULL;
  
  heap_clean();
}

bool int_call(const char *ident, expr_t *arg_list, int num_arg_list)
{
  fn_t *fn = scope_find_fn(&scope_global, ident);
  if (!fn) {
    printf("int_call: error: function '%s' undeclared\n", ident);
    return false;
  }
  
  scope_t new_scope;
  scope_new(&new_scope, NULL, &fn->type, &scope_global, fn->scope_parent, true);
  new_scope.size += scope_global.size;
  
  int arg_num = 0;
  s_node_t *head = fn->param;
  while (head) {
    if (arg_num >= num_arg_list) {
      printf("int_call: error: %s(): too few arguments\n", ident);
      return false;
    }
    
    type_t type;
    if (!int_type(&new_scope, &type, head->param_decl.type))
      return false;
    
    if (!type_cmp(&type, &arg_list[arg_num].type))
      return false;
    
    if (scope_find_var(&new_scope, head->param_decl.ident->data.ident)) {
      c_error(
        head->param_decl.ident,
        "redefinition of param '%s'",
        head->param_decl.ident->data.ident);
      return false;
    }
    
    var_t *var = scope_add_var(&new_scope, &type, head->param_decl.ident->data.ident);
    mem_assign(mem_stack, var->loc, &var->type, &arg_list[arg_num]);
    
    head = head->param_decl.next;
    arg_num++;
  }
  
  if (arg_num < num_arg_list) {
    LOG_ERROR("%s(): too many arguments", ident);
    return false;
  }
  
  int_body(&new_scope, fn->node);
  
  scope_free(&new_scope);
  
  return true;
}

void int_bind(const char *ident, xaction_t xaction)
{
  scope_add_fn(
    &scope_global,
    &type_none,
    NULL,
    NULL,
    xaction,
    NULL,
    ident);
}

bool int_arg_load(const scope_t *scope, expr_t *expr, char *ident)
{
  var_t *var = scope_find_var(scope, ident);
  if (!var)
    return false;
  
  mem_load(mem_stack, var->loc, &var->type, expr);
  return true;
}

bool int_body(scope_t *scope, const s_node_t *node)
{
  const s_node_t *head = node;
  
  while (head && !scope->ret_flag) {
    if (!int_stmt(scope, head))
      return false;
    
    head = head->stmt.next;
  }
  
  return true;
}

bool int_body_scope(scope_t *scope, const s_node_t *node)
{
  scope_t new_scope;
  scope_new(&new_scope, NULL, &scope->ret_type, scope, scope, false);
  new_scope.size = scope->size;
  
  if (!int_body(&new_scope, node))
    return false;
  
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
  case S_ARRAY_INIT:
  case S_POST_OP:
    if (!int_expr(scope, &expr, node->stmt.body))
      return false;
    break;
  case S_DECL:
    if (!int_decl(scope, node->stmt.body, true))
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
  case S_FOR_STMT:
    if (!int_for_stmt(scope, node->stmt.body))
      return false;
    break;
  case S_FN:
    if (!int_fn(scope, node->stmt.body, NULL))
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

bool int_fn(scope_t *scope, const s_node_t *node, const scope_t *scope_class)
{
  if (node->fn.body) {
    if (scope_find_fn(scope, node->fn.fn_ident->data.ident)) {
      c_error(node->fn.fn_ident, "redefinition of function '%s'", node->fn.fn_ident->data.ident);
      return false;
    }
  }
  
  type_t type = type_none;
  if (node->fn.type) {
    if (!int_type(scope, &type, node->fn.type))
      return false;
  }
  
  if (!node->fn.body) {
    fn_t *fn = scope_find_fn(scope, node->fn.fn_ident->data.ident);
    
    if (!fn) {
      c_error(
        node->fn.fn_ident,
        "bodyless function is unboud '%s'",
        node->fn.fn_ident->data.ident);
      return false;
    }
    
    fn->type = type;
    fn->param = node->fn.param_decl;
  } else {
    scope_add_fn(
      scope,
      &type,
      node->fn.param_decl,
      node->fn.body,
      NULL,
      scope_class,
      node->fn.fn_ident->data.ident);
  }
  
  return true;
}

bool int_while_stmt(scope_t *scope, const s_node_t *node)
{
  expr_t cond;
  if (!int_expr(scope, &cond, node->while_stmt.cond))
    return false;
  
  while (cond.i32 != 0) {
    if (!int_body_scope(scope, node->while_stmt.body))
      return false;
    
    if (!int_expr(scope, &cond, node->while_stmt.cond))
      return false;
  }
  
  return true;
}

bool int_for_stmt(scope_t *scope, const s_node_t *node)
{
  scope_t new_scope;
  scope_new(&new_scope, NULL, &scope->ret_type, scope, scope, false);
  new_scope.size = scope->size;
  
  if (node->for_stmt.decl) {
    if (!int_stmt(&new_scope, node->for_stmt.decl))
      return false;
  }
  
  expr_t cond;
  if (!int_expr(&new_scope, &cond, node->for_stmt.cond))
    return false;
  
  while (cond.i32 != 0) {
    if (!int_body_scope(&new_scope, node->for_stmt.body))
      return false;
    
    if (node->for_stmt.inc) {
      if (!int_expr(&new_scope, &cond, node->for_stmt.inc))
        return false;
    }
    
    if (!int_expr(&new_scope, &cond, node->for_stmt.cond))
      return false;
  }
  
  scope->ret_flag = new_scope.ret_flag;
  scope->ret_value = new_scope.ret_value;
  
  heap_clean();
  
  scope_free(&new_scope);
  scope->scope_child = NULL;
  
  return true;
}

bool int_if_stmt(scope_t *scope, const s_node_t *node)
{
  expr_t cond;
  if (!int_expr(scope, &cond, node->if_stmt.cond))
    return false;
  
  if (cond.i32 != 0) {
    if (!int_body_scope(scope, node->if_stmt.body))
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
  
  scope_t class;
  scope_new(&class, node->class_def.ident->data.ident, &type_none, NULL, scope, true);
  scope_t *class_scope = scope_add_class(scope, node->class_def.ident->data.ident, &class);
  
  s_node_t *head = node->class_def.class_decl;
  while (head) {
    switch (head->stmt.body->node_type) {
    case S_FN:
      if (!int_fn(class_scope, head->stmt.body, class_scope))
        return false;
      break;
    case S_DECL:
      if (!int_decl(class_scope, head->stmt.body, false))
        return false;
      break;
    default:
      LOG_ERROR("unknown node_type (%i)", head->stmt.body->node_type);
      break;
    }
    
    head = head->stmt.next;
  }
  
  return true;
}

bool int_decl(scope_t *scope, const s_node_t *node, bool init)
{
  type_t type;
  if (!int_type(scope, &type, node->decl.type))
    return false;
  
  var_t *var = scope_add_var(scope, &type, node->decl.ident->data.ident);
  if (!var) {
    c_error(
      node->decl.ident,
      "redefinition of '%s'",
      node->decl.ident->data.ident);
    return false;
  }
  
  if (init) {
    expr_t expr;
    if (node->decl.init) {
      if (!int_expr(scope, &expr, node->decl.init))
        return false;
      
      if (!expr_cast(&expr, &type)) {
        c_error(
          node->decl.type->type.spec,
          "incompatible types when initializing '%z' with '%z'",
          &type, &expr.type);
        return false;
      }
    } else {
      expr = (expr_t) {0};
    }
    
    if (scope->size >= mem_stack_end) {
      LOG_ERROR("ran out of memory %i/%i", scope->size, mem_stack_end);
      return false;
    }
    
    mem_assign(mem_stack, var->loc, &var->type, &expr);
  } else {
    if (node->decl.init) {
      c_error(
        node->decl.ident,
        "cannot initialize '%s",
        node->decl.ident->data.ident);
      return false;
    }
  }
  
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
  case TK_STRING:
    type->spec = SPEC_STRING;
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
  
  type->arr = node->type.left_bracket != NULL;
  
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
  case S_ARRAY_INIT:
    return int_array_init(scope, expr, node);
  case S_POST_OP:
    return int_post_op(scope, expr, node);
  default:
    LOG_ERROR("unknown expr node_type (%i)", node->node_type);
    return false;
  }
}

bool int_proc(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t base;
  if (!int_expr(scope, &base, node->proc.base))
    return false;
  
  if (!type_fn(&base.type)) {
    c_error(
      node->proc.left_bracket,
      "attempt to call non-function");
    return false;
  }
  
  fn_t *fn = (fn_t*) base.align_of;
  
  if (!fn) {
    c_error(
      node->proc.left_bracket,
      "attempt to call invalid function");
    return false;
  }
  
  if (!fn->node && !fn->xaction) {
    c_error(
      node->proc.left_bracket,
      "attempt to call function without body");
    return false;
  }
  
  scope_t new_scope;
  if (fn->scope_class) {
    scope_new(&new_scope, NULL, &fn->type, scope, fn->scope_parent->scope_find, true);
    new_scope.size += scope->size;
    
    expr_t self_expr;
    self_expr.type.spec = SPEC_CLASS;
    self_expr.type.arr = false;
    self_expr.type.class = fn->scope_class;
    self_expr.align_of = (size_t) base.loc_base;
    self_expr.loc_base = NULL;
    self_expr.loc_offset = 0;
    
    var_t *var = scope_add_var(&new_scope, &self_expr.type, "this");
    mem_assign(mem_stack, var->loc, &var->type, &self_expr);
  } else {
    scope_new(&new_scope, NULL, &fn->type, scope, fn->scope_parent, true);
    new_scope.size += scope->size;
  }
  
  new_scope.ret_type = fn->type;
  
  s_node_t *arg = node->proc.arg;
  s_node_t *head = fn->param;
  while (head) {
    if (!arg) {
      c_error(
        node->proc.left_bracket,
        "too few arguments to function '%h'",
        node);
      return false;
    }
    
    type_t type;
    if (!int_type(&new_scope, &type, head->param_decl.type))
      return false;
    
    scope->size += new_scope.size;
    expr_t arg_value;
    if (!int_expr(scope, &arg_value, arg->arg.body))
      return false;
    scope->size -= new_scope.size;
    
    if (!expr_cast(&arg_value, &type)) {
      c_error(
        head->param_decl.ident,
        "expected '%z' but argument is of type '%z'",
        &type,
        &arg_value.type);
      return false;
    }
    
    var_t *var = scope_add_var(&new_scope, &type, head->param_decl.ident->data.ident);
    if (!var) {
      c_error(
        head->param_decl.ident,
        "redefinition of param '%s'",
        head->param_decl.ident->data.ident);
      return false;
    }
    mem_assign(mem_stack, var->loc, &var->type, &arg_value);
    
    head = head->param_decl.next;
    arg = arg->arg.next;
  }
  
  if (arg) {
    c_error(
      node->proc.left_bracket,
      "too many arguments to function '%h'",
      node);
    return false;
  }
  
  if (fn->node) {
    if (!int_body(&new_scope, fn->node)) {
err_cleanup:
      scope_free(&new_scope);
      scope->scope_child = NULL;
      return false;
    }
  } else {
    fn->xaction(&new_scope.ret_value, &new_scope);
  }
  
  *expr = new_scope.ret_value;
  scope_free(&new_scope);
  scope->scope_child = NULL;
  
  return true;
}

bool int_unary(scope_t *scope, expr_t *expr, const s_node_t *node)
{  
  expr_t rhs;
  if (!int_expr(scope, &rhs, node->unary.rhs))
    return false;
  
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  
  if (node->unary.op->token == '-') {
    if (type_cmp(&rhs.type, &type_i32)) {
      expr->i32 = -rhs.i32;
      expr->type = type_i32;
    } else if (type_cmp(&rhs.type, &type_f32)) {
      expr->f32 = -rhs.f32;
      expr->type = type_f32;
    } else
      goto err_no_op;
  } else if (node->unary.op->token == '!') {
    if (type_cmp(&rhs.type, &type_i32)) {
      expr->i32 = !rhs.i32;
      expr->type = type_i32;
    } else
      goto err_no_op;
  } else {
err_no_op:
    c_error(
      node->unary.op,
      "unknown operand type for '%t': '%z'",
      node->unary.op->token,
      &rhs.type);
    return false;
  }
  
  return true;
}

static bool binop_i32(expr_t *expr, const expr_t *lhs, token_t op, const expr_t *rhs)
{
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  expr->type = type_i32;
  switch (op) {
  case '+':
    expr->i32 = lhs->i32 + rhs->i32;
    break;
  case '-':
    expr->i32 = lhs->i32 - rhs->i32;
    break;
  case '*':
    expr->i32 = lhs->i32 * rhs->i32;
    break;
  case '/':
    expr->i32 = lhs->i32 / rhs->i32;
    break;
  case '<':
    expr->i32 = lhs->i32 < rhs->i32;
    break;
  case '>':
    expr->i32 = lhs->i32 > rhs->i32;
    break;
  case TK_LE:
    expr->i32 = lhs->i32 <= rhs->i32;
    break;
  case TK_GE:
    expr->i32 = lhs->i32 >= rhs->i32;
    break;
  case TK_EQ:
    expr->i32 = lhs->i32 == rhs->i32;
    break;
  case TK_NE:
    expr->i32 = lhs->i32 != rhs->i32;
    break;
  case TK_AND:
    expr->i32 = lhs->i32 && rhs->i32;
    break;
  case TK_OR:
    expr->i32 = lhs->i32 || rhs->i32;
    break;
  default:
    return false;
  }
  
  return true;
}

static bool binop_f32(expr_t *expr, const expr_t *lhs, token_t op, const expr_t *rhs)
{
  expr->type = type_f32;
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  switch (op) {
  case '+':
    expr->f32 = lhs->f32 + rhs->f32;
    break;
  case '-':
    expr->f32 = lhs->f32 - rhs->f32;
    break;
  case '*':
    expr->f32 = lhs->f32 * rhs->f32;
    break;
  case '/':
    expr->f32 = lhs->f32 / rhs->f32;
    break;
  case '<':
    expr->i32 = lhs->f32 < rhs->f32;
    expr->type = type_i32;
    break;
  case '>':
    expr->i32 = lhs->f32 > rhs->f32;
    expr->type = type_i32;
    break;
  case TK_GE:
    expr->f32 = lhs->f32 >= rhs->f32;
    expr->type = type_i32;
    break;
  case TK_LE:
    expr->f32 = lhs->f32 <= rhs->f32;
    expr->type = type_i32;
    break;
  case TK_EQ:
    expr->f32 = lhs->f32 == rhs->f32;
    expr->type = type_i32;
    break;
  case TK_NE:
    expr->f32 = lhs->f32 != rhs->f32;
    expr->type = type_i32;
    break;
  default:
    return false;
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
  
  if (node->binop.op->token == '='
  || node->binop.op->token >= TK_ADD_ASSIGN
  && node->binop.op->token <= TK_DIV_ASSIGN) {
    if (!expr_lvalue(&lhs)) {
      c_error(node->binop.op, "lvalue required as left operand of assignment");
      return false;
    }
    
    if (type_cmp(&lhs.type, &type_i32) && expr_cast(&rhs, &type_i32)) {
      switch (node->binop.op->token) {
      case '=':
        lhs.i32 = rhs.i32;
        break;
      case TK_ADD_ASSIGN:
        lhs.i32 += rhs.i32;
        break;
      case TK_SUB_ASSIGN:
        lhs.i32 -= rhs.i32;
        break;
      case TK_MUL_ASSIGN:
        lhs.i32 *= rhs.i32;
        break;
      case TK_DIV_ASSIGN:
        lhs.i32 /= rhs.i32;
        break;
      default:
        goto err_no_op;
      }
    } else if (type_cmp(&lhs.type, &type_f32) && expr_cast(&rhs, &type_f32)) {
      switch (node->binop.op->token) {
      case '=':
        lhs.f32 = rhs.f32;
        break;
      case TK_ADD_ASSIGN:
        lhs.f32 += rhs.f32;
        break;
      case TK_SUB_ASSIGN:
        lhs.f32 -= rhs.f32;
        break;
      case TK_MUL_ASSIGN:
        lhs.f32 *= rhs.f32;
        break;
      case TK_DIV_ASSIGN:
        lhs.f32 /= rhs.f32;
        break;
      default:
        goto err_no_op;
      }
    } else if ((type_class(&lhs.type) && type_class(&rhs.type))
    || (type_array(&lhs.type) && type_array(&rhs.type))
    || (type_cmp(&lhs.type, &type_string) && type_cmp(&rhs.type, &type_string))
    && type_cmp(&lhs.type, &rhs.type)) {
      switch (node->binop.op->token) {
      case '=':
        lhs.align_of = rhs.align_of;
        break;
      default:
        goto err_no_op;
      }
    } else
      goto err_no_op;
    
    mem_assign(lhs.loc_base, lhs.loc_offset, &lhs.type, &lhs);
    *expr = lhs;
  } else {
    expr->loc_base = NULL;
    expr->loc_offset = 0;
    if (type_cmp(&lhs.type, &type_i32) && type_cmp(&rhs.type, &type_i32)) {
      if (!binop_i32(expr, &lhs, node->binop.op->token, &rhs))
        goto err_no_op;
    } else if (expr_cast(&lhs, &type_f32) && expr_cast(&rhs, &type_f32)) {
      if (!binop_f32(expr, &lhs, node->binop.op->token, &rhs))
        goto err_no_op;
    } else {
  err_no_op:
      c_error(
        node->binop.op,
        "unknown operand type for '%t': '%z' and '%z' '%h'",
        node->binop.op->token,
        &lhs.type, &rhs.type,
        node);
      return false;
    }
  }
  
  return true;
}

static bool array_direct(expr_t *expr, const s_node_t *node, const expr_t *base)
{
  heap_block_t *heap_block = (heap_block_t*) base->align_of;
  
  if (strcmp(node->direct.child_ident->data.ident, "length") == 0) {
    expr_i32(expr, heap_block->size / type_size_base(&base->type));
  } else {
    c_error(
      node->direct.child_ident,
      "request for unknown member '%s' in array",
      node->direct.child_ident->data.ident);
    return false;
  }
  
  return true;
}

bool int_direct(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t base;
  if (!int_expr(scope, &base, node->direct.base))
    return false;
  
  if (type_array(&base.type))
    return array_direct(expr, node, &base);
  
  if (!type_class(&base.type)) {
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
  
  heap_block_t *heap_block = (heap_block_t*) base.align_of;
  if (!int_load_ident(base.type.class, heap_block, expr, node->direct.child_ident)) {
    c_error(
      node->direct.child_ident,
      "'class %s' has no member named '%s'",
      base.type.class->ident,
      node->direct.child_ident->data.ident);
    return false;
  }
  
  return true;
}

bool int_index(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t base;
  if (!int_expr(scope, &base, node->index.base))
    return false;
  
  if (!type_array(&base.type)) {
    c_error(node->index.left_bracket, "subscripted value is not an array '%h'", node);
    return false;
  }
  
  expr_t index;
  if (!int_expr(scope, &index, node->index.index))
    return false;
  
  if (!type_cmp(&index.type, &type_i32)) {
    c_error(
      node->index.left_bracket,
      "array subscript is of type '%z', not 'i32' '%h'",
      &index.type,
      node);
    return false;
  }
  
  int offset = index.i32 * type_size_base(&base.type);
  
  heap_block_t *heap_block = (heap_block_t*) base.align_of;
  if (!heap_block) {
    c_error(
      node->index.left_bracket,
      "cannot index into uninitialised array '%h'",
      node->index.base);
    return false;
  }
  
  if (offset >= heap_block->size) {
    c_error(node->index.left_bracket, "index out of bounds '%h'", node);
    return false;
  }
  
  *expr = base;
  expr->type.arr = false;
  expr->loc_base = (char*) heap_block->block;
  expr->loc_offset += offset;
  mem_load(expr->loc_base, expr->loc_offset, &expr->type, expr);
  
  return true;
}

bool int_new(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  scope_t *class = scope_find_class(scope, node->new.class_ident->data.ident);
  
  if (!class) {
    c_error(
      node->new.class_ident,
      "undeclared class '%s'",
      node->new.class_ident->data.ident);
    return false;
  }
  
  expr->type.spec = SPEC_CLASS;
  expr->type.arr = false;
  expr->type.class = class;
  expr->align_of = (size_t) heap_alloc(class->size);
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  
  return true;
}

bool int_array_init(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  type_t type;
  if (!int_type(scope, &type, node->array_init.type))
    return false;
  
  expr_t size;
  if (!int_expr(scope, &size, node->array_init.size))
    return false;
  
  if (!type_cmp(&size.type, &type_i32)) {
    c_error(
      node->array_init.array_init,
      "size of array has non-integer type");
    return false;
  }
  
  expr->type = type;
  expr->type.arr = true;
  expr->align_of = (size_t) heap_alloc(size.i32 * type_size(&type));
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  
  return true;
}

bool int_post_op(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t lhs;
  if (!int_expr(scope, &lhs, node->post_op.lhs))
    return false;
  
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  
  if (node->post_op.op->token == TK_INC) {
    if (!expr_lvalue(&lhs)) {
      c_error(node->post_op.op, "lvalue required as left operand '%t'", node->post_op.op->token);
      return false;
    }
    
    *expr = lhs;
    expr->loc_base = NULL;
    expr->loc_offset = 0;
    
    if (type_cmp(&lhs.type, &type_i32))
      lhs.i32 = lhs.i32 + 1;
    else if (type_cmp(&lhs.type, &type_f32))
      lhs.f32 = lhs.f32 + 1.0;
    
    mem_assign(lhs.loc_base, lhs.loc_offset, &type_i32, &lhs);
  } else if (node->post_op.op->token == TK_DEC) {
    if (!expr_lvalue(&lhs)) {
      c_error(node->post_op.op, "lvalue required as left operand '%t'", node->post_op.op->token);
      return false;
    }
    
    *expr = lhs;
    expr->loc_base = NULL;
    expr->loc_offset = 0;
    
    if (type_cmp(&lhs.type, &type_i32))
      lhs.i32 = lhs.i32 - 1;
    else if (type_cmp(&lhs.type, &type_f32))
      lhs.f32 = lhs.f32 - 1.0;
    
    mem_assign(lhs.loc_base, lhs.loc_offset, &type_i32, &lhs);
  } else {
    c_error(
      node->post_op.op,
      "unknown postfix operand type for '%t': '%z'",
      node->post_op.op->token,
      &lhs.type);
    return false;
  }
  
  return true;
}

bool int_constant(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  var_t *var;
  switch (node->constant.lexeme->token) {
  case TK_CONST_INTEGER:
    expr_i32(expr, node->constant.lexeme->data.i32);
    break;
  case TK_CONST_FLOAT:
    expr_f32(expr, node->constant.lexeme->data.f32);
    break;
  case TK_STRING_LITERAL:
    expr->type = type_string;
    expr->align_of = (size_t) node->constant.lexeme->data.string_literal;
    expr->loc_base = NULL;
    expr->loc_offset = 0;
    break;
  case TK_IDENTIFIER:
    if (!int_load_ident(scope, NULL, expr, node->constant.lexeme)) {
      c_error(
        node->constant.lexeme,
        "'%s' undeclared",
        node->constant.lexeme->data.ident);
      return false;
    }
    break;
  default:
    LOG_ERROR("unknown token (%i)", node->constant.lexeme->token);
    break;
  }
  
  return true;
}

static bool int_load_ident(const scope_t *scope, heap_block_t *heap_block, expr_t *expr, const lexeme_t *lexeme)
{
  var_t *var = scope_find_var(scope, lexeme->data.ident);
  if (var) {
    if (heap_block)
      mem_load(heap_block->block, var->loc, &var->type, expr);
    else
      mem_load(mem_stack, var->loc, &var->type, expr);
    return true;
  }
  
  fn_t *fn = scope_find_fn(scope, lexeme->data.ident);
  if (fn) {
    expr->type.spec = SPEC_FN;
    expr->type.arr = false;
    expr->type.class = NULL;
    expr->align_of = (size_t) fn;
    expr->loc_offset = 0;
    
    if (heap_block)
      expr->loc_base = (char*) heap_block;
    else
      expr->loc_base = NULL;
    
    return true;
  }
  
  return false;
}

static void mem_load(char *loc_base, int loc_offset, const type_t *type, expr_t *expr)
{
  expr->loc_base = loc_base;
  expr->loc_offset = loc_offset;
  expr->type = *type;
  if (type_array(type))
    expr->align_of = *((size_t*) &loc_base[loc_offset]);
  else if (type_class(type))
    expr->align_of = *((size_t*) &loc_base[loc_offset]);
  else if (type_cmp(type, &type_string))
    expr->align_of = *((size_t*) &loc_base[loc_offset]);
  else if (type_cmp(type, &type_i32))
    expr->i32 = *((int*) &loc_base[loc_offset]);
  else if (type_cmp(type, &type_f32))
    expr->f32 = *((float*) &loc_base[loc_offset]);
  else if (type_cmp(type, &type_string))
    expr->align_of = *((size_t*) &loc_base[loc_offset]);
  else
    LOG_DEBUG("unknown type");
}

static void mem_assign(char *loc_base, int loc_offset, const type_t *type, expr_t *expr)
{
  if (type_array(type))
    *((size_t*) &loc_base[loc_offset]) = (size_t) expr->align_of;
  else if (type_class(type))
    *((size_t*) &loc_base[loc_offset]) = (size_t) expr->align_of;
  else if (type_cmp(type, &type_string))
    *((size_t*) &loc_base[loc_offset]) = (size_t) expr->align_of;
  else if (type_cmp(type, &type_i32))
    *((int*) &loc_base[loc_offset]) = expr->i32;
  else if (type_cmp(type, &type_f32))
    *((float*) &loc_base[loc_offset]) = expr->f32;
  else if (type_cmp(type, &type_string))
    *((size_t*) &loc_base[loc_offset]) = expr->align_of;
  else if (type_cmp(type, &type_none))
    *((size_t*) &loc_base[loc_offset]) = 0;
  else
    LOG_DEBUG("unknown type");
}

static heap_block_t *heap_alloc(int size)
{
  heap_block_t *heap_block = ZONE_ALLOC(sizeof(heap_block_t));
  heap_block->block = ZONE_ALLOC(size);
  heap_block->use = true;
  heap_block->size = size;
  
  memset(heap_block->block, 0, size);
  
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
    if (heap_block->use) {
      heap_block = heap_block->next;
      continue;
    }
    
    if (heap_block->next)
      heap_block->next->prev = heap_block->prev;
    if (heap_block->prev)
      heap_block->prev->next = heap_block->next;
    
    if (heap_block == heap_block_list)
      heap_block_list = heap_block->next;
    
    ZONE_FREE(heap_block->block);
    ZONE_FREE(heap_block);
    
    heap_block = heap_block->next;
  }
}

static void heap_clean_R(scope_t *scope)
{
  if (!scope)
    return;
  
  if (scope->ret_flag)
    heap_clean_expr(&scope->ret_value);
  
  entry_t *entry = scope->map_var.start;
  while (entry) {
    var_t *var = (var_t*) entry->value;
    heap_clean_var(var);
    entry = entry->s_next;
  }
  
  heap_clean_R(scope->scope_child);
}

static void heap_clean_array_class(heap_block_t *heap_block, const scope_t *class)
{
  heap_block_t **array_class = (heap_block_t**) heap_block->block;
  int array_length = heap_block->size / sizeof(size_t);

  for (int i = 0; i < array_length; i++) {
    if (array_class[i]) {
      array_class[i]->use = true;
      heap_clean_class(array_class[i], class);
    }
  }
}

static void heap_clean_class(heap_block_t *heap_block, const scope_t *class)
{
  entry_t *entry = class->map_var.start;
  
  while (entry) {
    heap_clean_var((var_t*) entry->value);
    entry = entry->s_next;
  }
}

static void heap_clean_var(var_t *var)
{
  if (var->type.spec == SPEC_CLASS || var->type.spec == SPEC_STRING || var->type.arr) {
    expr_t expr;
    mem_load(mem_stack, var->loc, &var->type, &expr);
    heap_clean_expr(&expr);
  }
}

static void heap_clean_expr(expr_t *expr)
{
  if (expr->type.spec == SPEC_CLASS || expr->type.spec == SPEC_STRING || expr->type.arr) {
    heap_block_t *heap_block = (heap_block_t*) expr->align_of;
    
    if (!heap_block)
      return;
    
    heap_block->use = true;
    if (expr->type.spec == SPEC_CLASS) {
      if (expr->type.arr)
        heap_clean_array_class(heap_block, expr->type.class);
      else
        heap_clean_class(heap_block, expr->type.class);
    }
  }
}
