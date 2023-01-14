#include "data.h"

#include <stdio.h>
#include <stdlib.h>

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
    return type->class->scope.size;
  }
}

bool expr_lvalue(const expr_t *expr)
{
  return expr->loc != -1;
}

void scope_new(scope_t *scope)
{
  scope->map_var = map_new();
  scope->map_class = map_new();
  scope->map_fn = map_new();
  scope->size = 0;
}

void scope_free(scope_t *scope)
{
  map_flush(scope->map_var);
  map_flush(scope->map_class);
  map_flush(scope->map_fn);
}

var_t *scope_add_var(scope_t *scope, const type_t *type, const char *ident)
{
  var_t *var = malloc(sizeof(var_t));
  var->type = *type;
  var->loc = scope->size;
  
  scope->size += type_size(type);
  
  map_put(scope->map_var, ident, var);
  
  return var;
}

var_t *scope_find_var(const scope_t *scope, const char *ident)
{
  return map_get(scope->map_var, ident);
}

class_t *scope_add_class(scope_t *scope, const char *ident, const scope_t *class_scope)
{
  class_t *class = malloc(sizeof(class_t));
  class->scope = *class_scope;
  class->ident = ident;
  
  map_put(scope->map_class, ident, class);
  
  return class;
}

class_t *scope_find_class(const scope_t *scope, const char *ident)
{
  return map_get(scope->map_class, ident);
}

fn_t *scope_add_fn(scope_t *scope, s_node_t *node, const char *ident)
{
  fn_t *fn = malloc(sizeof(fn_t));
  fn->node = node;
  
  map_put(scope->map_fn, ident, fn);
  
  return fn;
}

fn_t *scope_find_fn(const scope_t *scope, const char *ident)
{
  return map_get(scope->map_fn, ident);
}
