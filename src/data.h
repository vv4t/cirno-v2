#ifndef DATA_H
#define DATA_H

#include "map.h"
#include <stdbool.h>

typedef struct {
  map_t   map_var;
  map_t   map_class;
  int     size;
} scope_t;

typedef struct {
  scope_t     scope;
  const char  *ident;
} class_t;

typedef enum {
  SPEC_NONE,
  SPEC_I32,
  SPEC_F32,
  SPEC_CLASS
} spec_t;

typedef struct {
  spec_t  spec;
  class_t *class;
  int     size;
} type_t;

typedef struct {
  union {
    int   i32;
    float f32;
  };
  int     loc;
  type_t  type;
} expr_t;

typedef struct {
  type_t  type;
  int     loc;
} var_t;

extern type_t type_i32;
extern type_t type_f32;

extern bool type_cmp(const type_t *a, const type_t *b);
extern bool type_array(const type_t *type);
extern int  type_size(const type_t *type);
extern int  type_size_base(const type_t *type);

extern void     scope_new(scope_t *scope);
extern var_t    *scope_add_var(scope_t *scope, const type_t *type, const char *ident);
extern var_t    *scope_find_var(const scope_t *scope, const char *ident);
extern class_t  *scope_add_class(scope_t *scope, const char *ident, const scope_t *class_scope);
extern class_t  *scope_find_class(const scope_t *scope, const char *ident);

#endif
