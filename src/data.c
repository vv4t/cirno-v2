#include "data.h"

#include "zone.h"
#include <stdio.h>

type_t type_none = {0};
type_t type_i32 = { .spec = SPEC_I32, .size = 0 };
type_t type_f32 = { .spec = SPEC_F32, .size = 0 };

bool type_cmp(const type_t *a, const type_t *b)
{
  return a->spec == b->spec
        && a->is_ptr == b->is_ptr
        && a->size == b->size
        && a->class == b->class;
}

bool type_array(const type_t *type)
{
  return type->size > 0;
}

bool type_ptr(const type_t *type)
{
  return type->is_ptr && type->size == 0;
}

int type_size(const type_t *type)
{
  if (type->size > 0)
    return type_size_base(type) * type->size;
  else
    return type_size_base(type);
}
  
int type_size_base(const type_t *type)
{
  if (type->is_ptr)
    return 4;
  
  switch (type->spec) {
  case SPEC_I32:
    return 4;
  case SPEC_F32:
    return 4;
  case SPEC_CLASS:
    return type->class->size;
  }
}

bool expr_lvalue(const expr_t *expr)
{
  return expr->loc != -1;
}

void class_new(class_t *class)
{
  map_new(&class->map_var);
  class->size = 0;
}

void class_free(class_t *class)
{
  map_flush(&class->map_var);
}

var_t *class_add_var(class_t *class, const type_t *type, const char *ident)
{
  var_t *var = ZONE_ALLOC(sizeof(var_t));
  var->type = *type;
  var->loc = class->size;
  
  class->size += type_size(type);
  
  map_put(&class->map_var, ident, var);
  
  return var;
}

var_t *class_find_var(const class_t *class, const char *ident)
{
  return map_get(&class->map_var, ident);
}

void scope_new(scope_t *scope, const type_t *ret_type, const scope_t *scope_parent)
{
  scope->scope_parent = scope_parent;
  
  map_new(&scope->map_var);
  map_new(&scope->map_class);
  map_new(&scope->map_fn);
  
  scope->ret_flag = false;
  scope->ret_type = *ret_type;
  scope->ret_value = (expr_t) {0};
  scope->size = 0;
}

void scope_free(scope_t *scope)
{
  entry_t *entry_class = scope->map_class.start;
  while (entry_class) {
    class_free((class_t*) entry_class->value);
    entry_class = entry_class->s_next;
  }
  
  map_flush(&scope->map_var);
  map_flush(&scope->map_class);
  map_flush(&scope->map_fn);
}

var_t *scope_add_var(scope_t *scope, const type_t *type, const char *ident)
{
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
    if (!scope->scope_parent)
      return NULL;
    return scope_find_var(scope->scope_parent, ident);
  }
  
  return var;
}

class_t *scope_add_class(scope_t *scope, const char *ident, const class_t *class_data)
{
  class_t *class = ZONE_ALLOC(sizeof(class_t));
  *class = *class_data;
  
  map_put(&scope->map_class, ident, class);
  
  return class;
}

class_t *scope_find_class(const scope_t *scope, const char *ident)
{
  class_t *class = map_get(&scope->map_class, ident);
  if (!class) {
    if (!scope->scope_parent)
      return NULL;
    return scope_find_class(scope->scope_parent, ident);
  }
  
  return class;
}

fn_t *scope_add_fn(scope_t *scope, const type_t *type, s_node_t *param, s_node_t *node, const char *ident)
{
  fn_t *fn = ZONE_ALLOC(sizeof(fn_t));
  fn->node = node;
  fn->param = param;
  fn->type = *type;
  fn->scope_parent = scope;
  
  map_put(&scope->map_fn, ident, fn);
  
  return fn;
}

fn_t *scope_find_fn(const scope_t *scope, const char *ident)
{
  fn_t *fn = map_get(&scope->map_fn, ident);
  if (!fn) {
    if (!scope->scope_parent)
      return NULL;
    return scope_find_fn(scope->scope_parent, ident);
  }
  
  return fn;
}
