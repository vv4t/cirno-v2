#ifndef AST_H
#define AST_H

#include "syntax.h"
#include "data.h"

typedef struct ast_expr_s ast_expr_t;
typedef struct ast_stmt_s ast_stmt_t;

typedef enum {
  AST_OP_ADD_I32,
  AST_OP_SUB_I32,
  AST_OP_MUL_I32,
  AST_OP_DIV_I32,
  
  AST_OP_ADD_F32,
  AST_OP_SUB_F32,
  AST_OP_MUL_F32,
  AST_OP_DIV_F32,
  
  AST_OP_STORE_32
} ast_op_t;

typedef enum {
  AST_EXPR_I32,
  AST_EXPR_F32,
  AST_EXPR_BINOP,
  AST_EXPR_LOAD
} ast_expr_type_t;

typedef enum {
  LD_GLOBAL,
  LD_LOCAL
} ld_type_t;

struct ast_expr_s {
  union {
    int   i32;
    float f32;
    struct {
      ast_op_t    op;
      ast_expr_t  *lhs;
      ast_expr_t  *rhs;
    } binop;
    struct {
      ast_expr_t  *base;
      ld_type_t   ld_type;
    } load;
  };
  type_t          type;
  ast_expr_type_t ast_expr_type;
};

typedef enum {
  AST_STMT_EXPR
} ast_stmt_type_t;

struct ast_stmt_s {
  union {
    ast_expr_t  *expr;
  };
  ast_stmt_t      *next;
  ast_stmt_type_t ast_stmt_type;
};

ast_stmt_t *ast_parse(const s_node_t *node);

#endif
