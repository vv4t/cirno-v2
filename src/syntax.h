#ifndef SYNTAX_H
#define SYNTAX_H

#include "lex.h"
#include <stdbool.h>

typedef enum {
  S_CONSTANT,
  S_BINOP,
  S_TYPE,
  S_DECL,
  S_CLASS_DEF,
  S_STMT,
  S_INDEX,
  S_DIRECT,
  S_UNARY,
  S_PRINT,
  S_FN,
  S_PARAM_DECL,
  S_IF_STMT,
  S_WHILE_STMT,
  S_RET_STMT,
  S_PROC,
  S_NEW,
  S_ARRAY_INIT,
  S_ARG
} s_node_type_t;

typedef struct s_node_s {
  union {
    struct {
      struct s_node_s *body;
      struct s_node_s *next;
    } stmt;
    struct {
      const lexeme_t  *lexeme;
    } constant;
    struct {
      const lexeme_t  *spec;
      const lexeme_t  *left_bracket;
      const lexeme_t  *class_ident;
    } type;
    struct {
      struct s_node_s *type;
      const lexeme_t  *ident;
      struct s_node_s *init;
    } decl;
    struct {
      const lexeme_t  *ident;
      struct s_node_s *class_decl;
    } class_def;
    struct {
      const lexeme_t  *op;
      struct s_node_s *rhs;
    } unary;
    struct {
      struct s_node_s *lhs;
      struct s_node_s *rhs;
      const lexeme_t  *op;
    } binop;
    struct {
      struct s_node_s *base;
      const lexeme_t  *child_ident;
    } direct;
    struct {
      struct s_node_s *base;
      struct s_node_s *index;
      const lexeme_t  *left_bracket;
    } index;
    struct {
      struct s_node_s *arg;
    } print;
    struct {
      struct s_node_s *cond;
      struct s_node_s *body;
    } if_stmt;
    struct {
      const lexeme_t  *ret_token;
      struct s_node_s *body;
    } ret_stmt;
    struct {
      struct s_node_s *cond;
      struct s_node_s *body;
    } while_stmt;
    struct {
      const lexeme_t  *fn_ident;
      struct s_node_s *param_decl;
      struct s_node_s *type;
      struct s_node_s *body;
    } fn;
    struct {
      struct s_node_s *type;
      const lexeme_t  *ident;
      struct s_node_s *next;
    } param_decl;
    struct {
      struct s_node_s *base;
      struct s_node_s *arg;
      const lexeme_t  *left_bracket;
    } proc;
    struct {
      struct s_node_s *body;
      struct s_node_s *next;
    } arg;
    struct {
      const lexeme_t  *class_ident;
    } new;
    struct {
      const lexeme_t *array_init;
      struct s_node_s *type;
      struct s_node_s *size;
    } array_init;
  };
  s_node_type_t node_type;
} s_node_t;

extern s_node_t *s_parse(lex_t *lex);
extern void s_free(s_node_t *node);
extern void s_print_node(const s_node_t *node);
extern bool s_error();

#endif
