#include "lex.h"

#include "log.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

typedef struct {
  const char *src;
  const char *c;
} lex_file_t;

typedef struct {
  char    str[2];
  token_t tk;
} op_t;

typedef struct {
  char    str[32];
  token_t tk;
} word_t;

static word_t word_table[] = {
  { "i32", TK_I32 },
  { "f32", TK_F32 }
};

static int num_word_table = sizeof(word_table) / sizeof(word_t);

static op_t op_table[] = {
  { "-", '-' },
  { "+", '+' },
  { "*", '*' },
  { "/", '/' },
  { "=", '=' },
  { ";", ';' }
};

static int num_op_table = sizeof(op_table) / sizeof(op_t);

static lexeme_t *make_lexeme(token_t token)
{
  lexeme_t *lexeme = malloc(sizeof(lexeme_t));
  lexeme->token = token;
  lexeme->next = NULL;
  return lexeme;
}

static lexeme_t *match_const_integer(lex_file_t *lex)
{
  if (!isdigit(*lex->c))
    return NULL;
  
  int num = 0;
  
  do {
    num = num * 10 + *lex->c - '0';
    lex->c++;
  } while (isdigit(*lex->c));
  
  if (*lex->c == '.') {
    lex->c++;
    float f_num = (float) num;
    float digit = 0.1;
    
    do {
      f_num += (float) (*lex->c - '0') * digit;
      digit *= 0.1;
      lex->c++;
    } while (isdigit(*lex->c));
    
    lexeme_t *lexeme = make_lexeme(TK_CONST_FLOAT);
    lexeme->data.f32 = num;
    return lexeme;
  }
  
  lexeme_t *lexeme = make_lexeme(TK_CONST_INTEGER);
  lexeme->data.i32 = num;
  return lexeme;
}

static lexeme_t *match_word(lex_file_t *lex)
{
  if (!isalpha(*lex->c) && *lex->c != '_')
    return NULL;
  
  char word[128];
  char *letter = word;
  
  do
    *letter++ = *lex->c++;
  while (isalnum(*lex->c) || *lex->c == '_');
  
  for (int i = 0; i < num_word_table; i++)
    if (strcmp(word, word_table[i].str) == 0)
      return make_lexeme(word_table[i].tk);
  
  int str_len = strlen(word) + 1;
  char *str_ident = malloc(str_len);
  memcpy(str_ident, word, str_len);
  
  lexeme_t *lexeme = make_lexeme(TK_IDENTIFIER);
  lexeme->data.ident = str_ident;
  return lexeme;
}

static lexeme_t *match_op(lex_file_t *lex)
{
  for (int i = 0; i < num_op_table; i++) {
     if (strncmp(op_table[i].str, lex->c, strlen(op_table[i].str)) == 0) {
      lex->c += strlen(op_table[i].str);
      return make_lexeme(op_table[i].tk); 
    }
  }
  
  return NULL;
}

lex_t lex_parse(const char *src)
{
  lex_file_t lex_file = {
    .src = src,
    .c = src
  };
  
  lexeme_t *head = NULL, *body = NULL;
  
  while (*lex_file.c) {
    lexeme_t *lexeme = match_const_integer(&lex_file);
    if (!lexeme)
      lexeme = match_word(&lex_file);
    if (!lexeme)
      lexeme = match_op(&lex_file);
    
    if (!lexeme) {
      switch (*lex_file.c) {
      case 0:
      case ' ':
      case '\n':
      case '\t':
        lex_file.c++;
        break;
      default:
        LOG_DEBUG("skipping unknown character: %i", *lex_file.c);
        lex_file.c++;
      }
    } else {
      if (head)
        head = head->next = lexeme;
      else
        body = head = lexeme;
    }
  }
  
  lex_t lex = {
    .curr_lexeme = body
  };
  
  return lex;
}

const lexeme_t *lex_match(lex_t *lex, token_t token)
{
  if (!lex->curr_lexeme)
    return NULL;
  
  const lexeme_t *lexeme = NULL;
  
  if (lex->curr_lexeme->token == token) {
    lexeme = lex->curr_lexeme;
    lex->curr_lexeme = lex->curr_lexeme->next;
  }
  
  return lexeme;
}
