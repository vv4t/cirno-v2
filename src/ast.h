#ifndef AST_H
#define AST_H

#include "syntax.h"

typedef enum {
  AST_OP_ADD,
  AST_OP_SUB,
  AST_OP_MUL,
  AST_OP_DIV
} ast_op_t;

typedef enum {
  AST_EXPR_CONST_I32,
  AST_EXPR_BINOP
} ast_expr_type_t;

typedef enum {
  TYPE_SPEC_I32
} type_spec_t;

typedef struct {
  type_spec_t type_spec;
} type_t;

extern type_t type_i32;

typedef struct ast_expr_s {
  union {
    int   i32;
    struct {
      const struct ast_expr_s *lhs;
      const struct ast_expr_s *rhs;
      ast_op_t                op;
    } binop;
  };
  const type_t    *data_type;
  ast_expr_type_t node_type;
} ast_expr_t;

ast_expr_t *ast_parse(const s_node_t *s_node);

#endif
