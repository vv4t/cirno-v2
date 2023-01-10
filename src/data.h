#ifndef DATA_H
#define DATA_H

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
  union {
    int   i32;
    float f32;
    int   *mem;
  };
  type_t type;
} expr_t;

typedef struct var_s {
  type_t        type;
  expr_t        expr;
  struct var_s  *next;
} var_t;

#endif
