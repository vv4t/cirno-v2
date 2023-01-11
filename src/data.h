#ifndef DATA_H
#define DATA_H

#include <stdbool.h>

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
  union {
    int   i32;
    float f32;
  };
  char    *loc;
  type_t  type;
} expr_t;

struct var_s {
  type_t        type;
  char          *loc;
  struct var_s  *next;
};

#endif
