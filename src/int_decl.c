#include "int_local.h"

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
    case S_CLASS_NEW:
      if (!int_class_new(class_scope, head->stmt.body))
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

bool int_class_new(scope_t *scope, const s_node_t *node)
{
  type_t type = {
    .spec = SPEC_CLASS,
    .arr = false,
    .class = scope };
  
  scope_add_fn(
    scope,
    &type,
    node->class_new.param_decl,
    node->class_new.body,
    NULL,
    scope,
    "+new",
    true);
  
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
    
    if (scope->size >= stack_mem->size) {
      LOG_ERROR("ran out of memory %i/%i", scope->size, stack_mem->size);
      return false;
    }
    
    mem_assign(stack_mem, var->loc, &var->type, &expr);
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
      node->fn.fn_ident->data.ident,
      false);
  }
  
  return true;
}

