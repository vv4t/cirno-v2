#ifndef LEX_H
#define LEX_H

typedef enum {
  TK_CONST_INTEGER = 256,
  TK_CONST_FLOAT,
  TK_IDENTIFIER,
  TK_I32,
  TK_F32
} token_t;

typedef struct lexeme_s {
  token_t token;
  union {
    int         i32;
    float       f32;
    const char  *ident;
  } data;
  struct lexeme_s *next;
} lexeme_t;

typedef struct {
  const lexeme_t *curr_lexeme;
} lex_t;

extern lex_t lex_parse(const char *src);
extern const lexeme_t *lex_match(lex_t *lex, token_t);

#endif
