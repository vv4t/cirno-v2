#ifndef LEX_H
#define LEX_H

#include <stdarg.h>
#include <stdbool.h>

typedef enum {
  TK_CONST_INTEGER = 256,
  TK_CONST_FLOAT,
  TK_IDENTIFIER,
  TK_I32,
  TK_F32,
  TK_IF,
  TK_CLASS,
  TK_CLASS_DEF,
  TK_PRINT,
  TK_WHILE,
  TK_PTR_OP,
  TK_FN,
  TK_RETURN,
  TK_NEW,
  TK_STRING_LITERAL,
  TK_STRING,
  TK_ARRAY_INIT,
  TK_INC,
  TK_DEC,
  TK_GE,
  TK_LE,
  TK_EQ,
  TK_NE,
  TK_AND,
  TK_OR,
  TK_EOF
} token_t;

typedef struct lexeme_s {
  token_t token;
  union {
    int     i32;
    float   f32;
    char    *ident;
    char    *string_literal;
  } data;
  int             line;
  const char      *src;
  struct lexeme_s *next;
} lexeme_t;

typedef struct {
  const char  *src;
  char        *buffer;
  lexeme_t    *lexeme;
  lexeme_t    *start;
  int         eof_line;
} lex_t;

extern bool           lex_parse(lex_t *lex, const char *src);
extern const lexeme_t *lex_match(lex_t *lex, token_t);
extern void           lex_next(lex_t *lex);
extern void           lex_free(lex_t *lex);


#endif
