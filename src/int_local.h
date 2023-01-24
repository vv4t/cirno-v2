#ifndef INT_LOCAL_H
#define INT_LOCAL_H

#include "int_main.h"

#include "log.h"
#include "mem.h"
#include <string.h>

// int_main.c
extern bool int_load_ident(const scope_t *scope, heap_block_t *heap_block, expr_t *expr, const lexeme_t *lexeme);

// int_stmt.h
extern bool int_fn(scope_t *scope, const s_node_t *node, const scope_t *scope_class);
extern bool int_body(scope_t *scope, const s_node_t *node);
extern bool int_body_scope(scope_t *scope, const s_node_t *node);
extern bool int_stmt(scope_t *scope, const s_node_t *node);
extern bool int_print(scope_t *scope, const s_node_t *node);
extern bool int_ret_stmt(scope_t *scope, const s_node_t *node);
extern bool int_if_stmt(scope_t *scope, const s_node_t *node);
extern bool int_ctrl_stmt(scope_t *scope, const s_node_t *node);
extern bool int_while_stmt(scope_t *scope, const s_node_t *node);
extern bool int_for_stmt(scope_t *scope, const s_node_t *node);

// int_decl.h
extern bool int_decl(scope_t *scope, const s_node_t *node, bool init);
extern bool int_class_def(scope_t *scope, const s_node_t *node);
extern bool int_type(scope_t *scope, type_t *type, const s_node_t *node);

// int_expr.h
extern bool int_expr(scope_t *scope, expr_t *expr, const s_node_t *node);
extern bool int_unary(scope_t *scope, expr_t *expr, const s_node_t *node);
extern bool int_index(scope_t *scope, expr_t *expr, const s_node_t *node);
extern bool int_direct(scope_t *scope, expr_t *expr, const s_node_t *node);
extern bool int_proc(scope_t *scope, expr_t *expr, const s_node_t *node);
extern bool int_binop(scope_t *scope, expr_t *expr, const s_node_t *node);
extern bool int_constant(scope_t *scope, expr_t *expr, const s_node_t *node);
extern bool int_new(scope_t *scope, expr_t *expr, const s_node_t *node);
extern bool int_array_init(scope_t *scope, expr_t *expr, const s_node_t *node);
extern bool int_post_op(scope_t *scope, expr_t *expr, const s_node_t *node);

#endif
