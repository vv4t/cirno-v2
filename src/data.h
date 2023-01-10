#ifndef DATA_H
#define DATA_H

typedef struct var_s var_t;

typedef enum {
  SPEC_NONE,
  SPEC_I32,
  SPEC_F32
} spec_t;

typedef struct {
  spec_t  spec;
  int     size;
} type_t;

typedef struct {
  var_t *var;
} lvalue_t;

typedef struct {
  union {
    int   i32;
    float f32;
    int   *arr_i32;
    float *arr_f32;
  };
  lvalue_t  lvalue;
  type_t    type;
} expr_t;

struct var_s {
  type_t        type;
  expr_t        expr;
  struct var_s  *next;
};

#endif
