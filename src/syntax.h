#ifndef SYNTAX_H
#define SYNTAX_H

#include "lex.h"

typedef enum {
  S_CONSTANT,
  S_BINOP,
  S_TYPE,
  S_DECL,
  S_STMT
} s_node_type_t;

typedef struct s_node_s {
  union {
    struct {
      const struct s_node_s *body;
      const struct s_node_s *next;
    } stmt;
    struct {
      const lexeme_t *lexeme;
    } constant;
    struct {
      const lexeme_t *spec;
    } type;
    struct {
      const struct s_node_s *type;
      const lexeme_t        *ident;
      const struct s_node_s *init;
    } decl;
    struct {
      const struct s_node_s *lhs;
      const struct s_node_s *rhs;
      const lexeme_t        *op;
    } binop;
  };
  s_node_type_t node_type;
} s_node_t;

extern s_node_t *s_parse(lex_t *lex);
extern void s_print(const s_node_t *s_tree);

#endif
