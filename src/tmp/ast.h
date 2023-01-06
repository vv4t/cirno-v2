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
  AST_STMT_EXPR,
  AST_STMT_DECL
} ast_stmt_type_t;

typedef enum {
  AST_EXPR_CONST_I32,
  AST_EXPR_BINOP,
  AST_EXPR_LVALUE
} ast_expr_type_t;

typedef enum {
  TYPE_SPEC_I32,
  TYPE_SPEC_F32
} spec_t;

typedef struct {
  spec_t spec;
} type_t;

extern type_t type_i32;

typedef struct {
  const char    *ident;
  const type_t  *type;
} ast_lvalue_t;

typedef struct ast_expr_s {
  union {
    int                 i32;
    const ast_lvalue_t  *lvalue;
    struct {
      const struct ast_expr_s *lhs;
      const struct ast_expr_s *rhs;
      ast_op_t                op;
    } binop;
  };
  const type_t    *type;
  ast_expr_type_t node_type;
} ast_expr_t;

typedef struct {
  const char        *ident;
  const type_t      *type;
  const ast_expr_t  *init;
} ast_decl_t;

typedef struct ast_stmt_s {
  union {
    const ast_expr_t *expr;
    const ast_decl_t *decl;
  };
  ast_stmt_type_t         stmt_type;
  const struct ast_stmt_s *next;
} ast_stmt_t;

ast_stmt_t *ast_stmt(const s_node_t *s_node);

#endif
