#include "lex.h"

#include "log.h"
#include "zone.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char  *file;
  const char  *src;
  const char  *c;
  int         line;
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
  { "class",      TK_CLASS      },
  { "class_def",  TK_CLASS_DEF  },
  { "print",      TK_PRINT      },
  { "while",      TK_WHILE      },
  { "i32",        TK_I32        },
  { "f32",        TK_F32        },
  { "fn",         TK_FN         },
  { "return",     TK_RETURN     },
  { "new",        TK_NEW        },
  { "string",     TK_STRING     },
  { "array_init", TK_ARRAY_INIT },
  { "if",         TK_IF         }
};

static int num_word_table = sizeof(word_table) / sizeof(word_t);

static op_t op_table[] = {
  { "->",   TK_PTR_OP   },
  { "++",   TK_INC      },
  { "--",   TK_DEC      },
  { ">=",   TK_GE       },
  { "<=",   TK_LE       },
  { "==",   TK_EQ       },
  { "!=",   TK_NE       },
  { "&&",   TK_AND      },
  { "||",   TK_OR       },
  { "-",    '-'         },
  { "+",    '+'         },
  { "*",    '*'         },
  { "/",    '/'         },
  { "<",    '<'         },
  { ">",    '>'         },
  { "=",    '='         },
  { ".",    '.'         },
  { ",",    ','         },
  { ";",    ';'         },
  { ":",    ':'         },
  { "&",    '&'         },
  { "!",    '!'         },
  { "{",    '{'         },
  { "}",    '}'         },
  { "[",    '['         },
  { "]",    ']'         },
  { "(",    '('         },
  { ")",    ')'         }
};

static int num_op_table = sizeof(op_table) / sizeof(op_t);

static lexeme_t *make_lexeme(token_t token, const lex_file_t *lex);
static lexeme_t *match_const_integer(lex_file_t *lex);
static lexeme_t *match_string_literal(lex_file_t *lex);
static lexeme_t *match_word(lex_file_t *lex);
static lexeme_t *match_op(lex_file_t *lex);
static void     token_print(token_t token);
static void     lexeme_print(const lexeme_t *lexeme);
static void     lex_printf(const lexeme_t *lexeme, const char *fmt, va_list args);

bool lex_parse(lex_t *lex, const char *src)
{
  FILE *fp = fopen(src, "rb");
  
  if (!fp)
    return false;
  
  fseek(fp, 0, SEEK_END);
  size_t size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *buffer = ZONE_ALLOC(size + 1);
  fread(buffer, 1, size, fp);
  buffer[size] = 0;
  fclose(fp);
  
  lex_file_t lex_file = {
    .file = src,
    .src = buffer,
    .c = buffer,
    .line = 1
  };
  
  lexeme_t *head = NULL, *body = NULL;
  
  while (*lex_file.c) {
    lexeme_t *lexeme = match_const_integer(&lex_file);
    if (!lexeme)
      lexeme = match_string_literal(&lex_file);
    if (!lexeme)
      lexeme = match_word(&lex_file);
    if (!lexeme)
      lexeme = match_op(&lex_file);
    
    if (!lexeme) {
      switch (*lex_file.c) {
      case '\n':
        lex_file.line++;
      case 0:
      case ' ':
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
  
  head->next = make_lexeme(TK_EOF, &lex_file);
  
  *lex = (lex_t) {
    .src = src,
    .buffer = buffer,
    .lexeme = body,
    .start = body,
    .eof_line = lex_file.line
  };
  
  return true;
}

void lex_free(lex_t *lex)
{
  ZONE_FREE(lex->buffer);
  
  lexeme_t *lexeme = lex->start;
  while (lexeme) {
    switch (lexeme->token) {
    case TK_IDENTIFIER:
      ZONE_FREE(lexeme->data.ident);
      break;
    case TK_STRING_LITERAL:
      ZONE_FREE(lexeme->data.string_literal);
      break;
    }
    
    lexeme_t *next = lexeme->next;
    ZONE_FREE(lexeme);
    lexeme = next;
  }
}

void lex_next(lex_t *lex)
{
  if (!lex->lexeme)
    return;
  
  lex->lexeme = lex->lexeme->next;
}

const lexeme_t *lex_match(lex_t *lex, token_t token)
{
  if (!lex->lexeme)
    return NULL;
  
  const lexeme_t *lexeme = NULL;
  
  if (lex->lexeme->token == token) {
    lexeme = lex->lexeme;
    lex->lexeme = lex->lexeme->next;
  }
  
  return lexeme;
}

static lexeme_t *make_lexeme(token_t token, const lex_file_t *lex)
{
  lexeme_t *lexeme = ZONE_ALLOC(sizeof(lexeme_t));
  lexeme->token = token;
  lexeme->line = lex->line;
  lexeme->src = lex->file;
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
  
  if (*lex->c == '.' && isdigit(lex->c[1])) {
    lex->c++;
    float f_num = (float) num;
    float digit = 0.1;
    
    while (isdigit(*lex->c)) {
      f_num += (float) (*lex->c - '0') * digit;
      digit *= 0.1;
      lex->c++;
    }
    
    lexeme_t *lexeme = make_lexeme(TK_CONST_FLOAT, lex);
    lexeme->data.f32 = num;
    return lexeme;
  }
  
  lexeme_t *lexeme = make_lexeme(TK_CONST_INTEGER, lex);
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
  
  *letter++ = 0;
  
  for (int i = 0; i < num_word_table; i++)
    if (strcmp(word, word_table[i].str) == 0)
      return make_lexeme(word_table[i].tk, lex);
  
  int str_len = strlen(word) + 1;
  char *str_ident = ZONE_ALLOC(str_len);
  memcpy(str_ident, word, str_len);
  
  lexeme_t *lexeme = make_lexeme(TK_IDENTIFIER, lex);
  lexeme->data.ident = str_ident;
  return lexeme;
}

static lexeme_t *match_string_literal(lex_file_t *lex)
{
  if (*lex->c != '"')
    return NULL;
  
  const char *str_start = lex->c;
  int str_len = 0;
  
  do
    lex->c++;
  while (*lex->c != '"' && *lex->c != 0);
  
  if (*lex->c == 0) {
    printf("%s:%i:error: missing terminating '\"'\n", lex->file, lex->line);
    return NULL;
  }
  
  str_len = lex->c - str_start - 1;
  lex->c++;
  
  char *string_literal = ZONE_ALLOC(str_len + 1);
  strncpy(string_literal, str_start + 1, str_len);
  
  lexeme_t *lexeme = make_lexeme(TK_STRING_LITERAL, lex);
  lexeme->data.string_literal = string_literal;
  return lexeme;
}

static lexeme_t *match_op(lex_file_t *lex)
{
  for (int i = 0; i < num_op_table; i++) {
     if (strncmp(op_table[i].str, lex->c, strlen(op_table[i].str)) == 0) {
      lex->c += strlen(op_table[i].str);
      return make_lexeme(op_table[i].tk, lex); 
    }
  }
  
  return NULL;
}
