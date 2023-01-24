#include "int_local.h"

bool int_body(scope_t *scope, const s_node_t *node)
{
  const s_node_t *head = node;
  
  bool cont_flag = scope->cont_flag;
  bool break_flag = scope->break_flag;
  
  
  while (head && !scope->ret_flag) {
    if (cont_flag && !scope->cont_flag)
      break;
    
    if (break_flag && !scope->break_flag)
      break;
    
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
  new_scope.cont_flag = scope->cont_flag;
  new_scope.break_flag = scope->break_flag;
  new_scope.size = scope->size;
  
  if (!int_body(&new_scope, node)) {
    scope_free(&new_scope);
    scope->scope_child = NULL;
    return false;
  }
  
  scope->ret_flag = new_scope.ret_flag;
  scope->cont_flag = new_scope.cont_flag;
  scope->break_flag = new_scope.break_flag;
  
  scope->ret_value = new_scope.ret_value;
  
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
  case S_CTRL_STMT:
    if (!int_ctrl_stmt(scope, node->stmt.body))
      return false;
    break;
  default:
    LOG_ERROR("unknown statement node_type (%i)", node->stmt.body->node_type);
    return false;
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
  scope_t new_scope;
  scope_new(&new_scope, NULL, &scope->ret_type, scope, scope, false);
  new_scope.cont_flag = true;
  new_scope.break_flag = true;
  new_scope.size = scope->size;
  
  expr_t cond;
  if (!int_expr(scope, &cond, node->while_stmt.cond))
    return false;
  
  while (cond.i32 != 0) {
    if (!int_body_scope(&new_scope, node->while_stmt.body))
      return false;
    
    if (!new_scope.break_flag)
      break;
    
    if (!int_expr(scope, &cond, node->while_stmt.cond)) {
err_cleanup:
      scope_free(&new_scope);
      scope->scope_child = NULL;
      return false;
    }
  }
  
  scope->ret_flag = new_scope.ret_flag;
  scope->ret_value = new_scope.ret_value;
  scope_free(&new_scope);
  scope->scope_child = NULL;
  
  return true;
}

bool int_for_stmt(scope_t *scope, const s_node_t *node)
{
  scope_t new_scope;
  scope_new(&new_scope, NULL, &scope->ret_type, scope, scope, false);
  new_scope.cont_flag = true;
  new_scope.break_flag = true;
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
      goto err_cleanup;
    
    if (node->for_stmt.inc) {
      if (!int_expr(&new_scope, &cond, node->for_stmt.inc))
        goto err_cleanup;
    }
    
    if (!int_expr(&new_scope, &cond, node->for_stmt.cond)) {
err_cleanup:
      scope_free(&new_scope);
      scope->scope_child = NULL;
      return false;
    }
  }
  
  scope->ret_flag = new_scope.ret_flag;
  scope->ret_value = new_scope.ret_value;
  scope_free(&new_scope);
  scope->scope_child = NULL;
  
  return true;
}

bool int_ctrl_stmt(scope_t *scope, const s_node_t *node)
{
  switch (node->ctrl_stmt.lexeme->token) {
  case TK_BREAK:
    if (!scope->break_flag) {
      c_error(
        node->ctrl_stmt.lexeme,
        "cannot break outside loop");
      return false;
    }
    scope->break_flag = false;
    break;
  case TK_CONTINUE:
    if (!scope->break_flag) {
      c_error(
        node->ctrl_stmt.lexeme,
        "cannot continue outside loop");
      return false;
    }
    scope->cont_flag= false;
    break;
  }
  
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
  } else {
    if (node->if_stmt.next)
      return int_stmt(scope, node->if_stmt.next);
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
