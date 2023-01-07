#ifndef LEX_H
#define LEX_H

#include <stdarg.h>

typedef enum {
  TK_CONST_INTEGER = 256,
  TK_CONST_FLOAT,
  TK_IDENTIFIER,
  TK_I32,
  TK_F32,
  TK_EOF
} token_t;

typedef struct lexeme_s {
  token_t token;
  union {
    int         i32;
    float       f32;
    const char  *ident;
  } data;
  int             line;
  const char      *src;
  struct lexeme_s *next;
} lexeme_t;

typedef struct {
  const char      *src;
  const lexeme_t  *lexeme;
  int             eof_line;
} lex_t;

extern lex_t          lex_parse(const char *src);
extern void           lex_printf(const lexeme_t *lexeme, const char *fmt, ...);
extern const lexeme_t *lex_match(lex_t *lex, token_t);

#endif
