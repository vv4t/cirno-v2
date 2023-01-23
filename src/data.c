#include "data.h"

#include "zone.h"
#include <stdio.h>

type_t type_none    = {0};
type_t type_i32     = { .spec = SPEC_I32,     .arr = false, .class = NULL };
type_t type_f32     = { .spec = SPEC_F32,     .arr = false, .class = NULL };
type_t type_string  = { .spec = SPEC_STRING,  .arr = false, .class = NULL };

bool type_cmp(const type_t *a, const type_t *b)
{
  return a->spec == b->spec
        && a->arr == b->arr
        && a->class == b->class;
}

bool type_fn(const type_t *type)
{
  return type->spec == SPEC_FN && !type_array(type);
}

bool type_class(const type_t *type)
{
  return type->spec == SPEC_CLASS && !type_array(type);
}

bool type_array(const type_t *type)
{
  return type->arr;
}

int type_size(const type_t *type)
{
  if (type->arr)
    return 8;
  
  switch (type->spec) {
  case SPEC_I32:
    return 4;
  case SPEC_F32:
    return 4;
  case SPEC_CLASS:
    return 8;
  }
}

int type_size_base(const type_t *type)
{
  switch (type->spec) {
  case SPEC_I32:
    return 4;
  case SPEC_F32:
    return 4;
  case SPEC_CLASS:
    return 8;
  }
}

void expr_i32(expr_t *expr, int i32)
{
  expr->type = type_i32;
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  expr->i32 = i32;
}

void expr_f32(expr_t *expr, float f32)
{
  expr->type = type_f32;
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  expr->f32 = f32;
}

bool expr_cast(expr_t *expr, const type_t *type)
{
  if (type_cmp(&expr->type, type))
    return true;
  
  if (type_cmp(&expr->type, &type_i32) && type_cmp(type, &type_f32)) {
    expr->type = type_i32;
    expr->f32 = (float) expr->i32;
  } else if (type_cmp(&expr->type, &type_f32) && type_cmp(type, &type_i32)) {
    expr->type = type_f32;
    expr->i32 = (int) expr->f32;
  } else
    return false;
  
  return true;
}


bool expr_lvalue(const expr_t *expr)
{
  return expr->loc_base != NULL;
}

static void _class_free(void *block)
{
  scope_free((scope_t*) block);
  ZONE_FREE(block);
}

static void _free(void *block)
{
  ZONE_FREE(block);
}

var_t *class_add_var(scope_t *class, const type_t *type, const char *ident)
{
  var_t *var = ZONE_ALLOC(sizeof(var_t));
  var->type = *type;
  var->loc = class->size;
  
  class->size += type_size(type);
  
  map_put(&class->map_var, ident, var);
  
  return var;
}

var_t *class_find_var(const scope_t *class, const char *ident)
{
  return map_get(&class->map_var, ident);
}

void scope_new(
  scope_t       *scope,
  const char    *ident,
  const type_t  *ret_type,
  scope_t       *scope_parent,
  const scope_t *scope_find,
  bool          block)
{
  if (scope_parent)
    scope_parent->scope_child = scope;
  
  scope->ident = ident;
  scope->scope_find = scope_find;
  scope->scope_parent = scope_parent;
  scope->scope_child  = NULL;
  
  map_new(&scope->map_var);
  map_new(&scope->map_class);
  map_new(&scope->map_fn);
  
  scope->ret_flag = false;
  scope->ret_type = *ret_type;
  scope->ret_value = (expr_t) {0};
  
  scope->block = block;
  
  scope->size = 0;
}

void scope_free(scope_t *scope)
{
  if (scope->scope_parent)
    scope->scope_parent->scope_child = NULL;
  
  map_flush(&scope->map_class, _class_free);
  map_flush(&scope->map_var, _free);
  map_flush(&scope->map_fn, _free);
}

var_t *scope_add_var(scope_t *scope, const type_t *type, const char *ident)
{
  const scope_t *scope_find = scope;
  while (scope_find) {
    if (map_get(&scope->map_var, ident))
      return NULL;
    
    if (scope_find->block)
      break;
    
    scope_find = scope->scope_find;
  }
  
  var_t *var = ZONE_ALLOC(sizeof(var_t));
  var->type = *type;
  var->loc = scope->size;
  
  scope->size += type_size(type);
  
  map_put(&scope->map_var, ident, var);
  
  return var;
}

var_t *scope_find_var(const scope_t *scope, const char *ident)
{
  var_t *var = map_get(&scope->map_var, ident);
  if (!var) {
    if (!scope->scope_find)
      return NULL;
    return scope_find_var(scope->scope_find, ident);
  }
  
  return var;
}

scope_t *scope_add_class(scope_t *scope, const char *ident, const scope_t *class_data)
{
  scope_t *class = ZONE_ALLOC(sizeof(scope_t));
  *class = *class_data;
  
  map_put(&scope->map_class, ident, class);
  
  return class;
}

scope_t *scope_find_class(const scope_t *scope, const char *ident)
{
  scope_t *class = map_get(&scope->map_class, ident);
  if (!class) {
    if (!scope->scope_find)
      return NULL;
    return scope_find_class(scope->scope_find, ident);
  }
  
  return class;
}

fn_t *scope_add_fn(
  scope_t       *scope,
  const type_t  *type,
  s_node_t      *param,
  s_node_t      *node,
  xaction_t     xaction,
  const scope_t *scope_class,
  const char    *ident)
{
  fn_t *fn = ZONE_ALLOC(sizeof(fn_t));
  fn->node = node;
  fn->xaction = xaction;
  fn->param = param;
  fn->type = *type;
  fn->scope_parent = scope;
  fn->scope_class = scope_class;
  
  map_put(&scope->map_fn, ident, fn);
  
  return fn;
}

fn_t *scope_find_fn(const scope_t *scope, const char *ident)
{
  fn_t *fn = map_get(&scope->map_fn, ident);
  if (!fn) {
    if (!scope->scope_parent)
      return NULL;
    return scope_find_fn(scope->scope_find, ident);
  }
  
  return fn;
}
