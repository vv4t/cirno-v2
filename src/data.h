#ifndef DATA_H
#define DATA_H

#include "map.h"
#include "syntax.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct scope_s scope_t;

typedef enum {
  SPEC_NONE,
  SPEC_I32,
  SPEC_F32,
  SPEC_CLASS,
  SPEC_FN,
  SPEC_STRING
} spec_t;

typedef struct {
  spec_t        spec;
  bool          arr;
  const scope_t *class;
} type_t;

typedef struct {
  union {
    int     i32;
    float   f32;
    size_t  align_of;
  };
  int     loc_offset;
  char    *loc_base;
  type_t  type;
} expr_t;

struct scope_s {
  const scope_t *scope_find;
  const char    *ident;
  
  scope_t *scope_parent;
  scope_t *scope_child;
  
  map_t   map_fn;
  map_t   map_var;
  map_t   map_class;
  
  type_t  ret_type;
  expr_t  ret_value;
  bool    ret_flag;
  
  int     size;
};

// HAHA THE OVERHEADS ITS LIKE 50B FOR ONE VARIABLE
typedef struct {
  type_t  type;
  int     loc;
} var_t;

typedef struct {
  s_node_t      *node;
  s_node_t      *param;
  type_t        type;
  const scope_t *scope_parent;
  const scope_t *scope_class;
} fn_t;

extern type_t type_none;
extern type_t type_i32;
extern type_t type_f32;
extern type_t type_string;

extern bool type_cmp(const type_t *a, const type_t *b);
extern bool type_class(const type_t *type);
extern bool type_fn(const type_t *type);
extern bool type_array(const type_t *type);
extern int  type_size(const type_t *type);
extern int  type_size_base(const type_t *type);

extern bool expr_lvalue(const expr_t *expr);

extern void     scope_new(scope_t *scope, const char *ident, const type_t *ret_type, scope_t *scope_parent, const scope_t *scope_find);
extern void     scope_free(scope_t *scope);

extern var_t    *scope_add_var(scope_t *scope, const type_t *type, const char *ident);
extern var_t    *scope_find_var(const scope_t *scope, const char *ident);
extern scope_t  *scope_add_class(scope_t *scope, const char *ident, const scope_t *class_data);
extern scope_t  *scope_find_class(const scope_t *scope, const char *ident);
extern fn_t     *scope_add_fn(scope_t *scope, const type_t *type, s_node_t *param, s_node_t *node, const scope_t *scope_class, const char *ident);
extern fn_t     *scope_find_fn(const scope_t *scope, const char *ident);

#endif
