#ifndef SYNTAX_H
#define SYNTAX_H

#include "lex.h"

typedef enum {
  S_CONSTANT,
  S_BINOP,
  S_TYPE,
  S_DECL,
  S_STMT,
  S_INDEX
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
      struct s_node_s *size;
    } type;
    struct {
      struct s_node_s *type;
      const lexeme_t  *ident;
      struct s_node_s *init;
    } decl;
    struct {
      struct s_node_s *lhs;
      struct s_node_s *rhs;
      const lexeme_t  *op;
    } binop;
    struct {
      struct s_node_s *base;
      struct s_node_s *index;
      const lexeme_t  *left_bracket;
    } index;
  };
  s_node_type_t node_type;
} s_node_t;

extern s_node_t *s_parse(lex_t *lex);
extern void s_print(s_node_t *s_tree);

#endif
