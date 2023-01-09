#include "lex.h"

#include "log.h"
#include <stdlib.h>
#include <ctype.h>
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
  { ";", ';' },
  { "(", '(' },
  { ")", ')' }
};

static int num_op_table = sizeof(op_table) / sizeof(op_t);

static lexeme_t *make_lexeme(token_t token, const lex_file_t *lex);
static lexeme_t *match_const_integer(lex_file_t *lex);
static lexeme_t *match_word(lex_file_t *lex);
static lexeme_t *match_op(lex_file_t *lex);
static void     token_print(token_t token);
static void     lexeme_print(const lexeme_t *lexeme);

lex_t lex_parse(const char *src)
{
  FILE *fp = fopen(src, "rb");
  
  if (!fp) {
    LOG_ERROR("failed to open file: main.bs");
    exit(1);
  }
  
  fseek(fp, 0, SEEK_END);
  size_t size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *buffer = malloc(size);
  fread(buffer, 1, size, fp);
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
  
  lex_t lex = {
    .src = src,
    .lexeme = body,
    .eof_line = lex_file.line
  };
  
  return lex;
}

void lex_printf(const lexeme_t *lexeme, const char *fmt, ...)
{
  if (!lexeme)
    return;
  
  printf("%s:%i:", lexeme->src, lexeme->line);
  
  va_list args;
  va_start(args, fmt);
  
  while (*fmt) {
    if (*fmt == '%') {
      switch (*++fmt) {
      case 't':
        token_print(va_arg(args, token_t));
        break;
      case 'l':
        lexeme_print(lexeme);
        break;
      case 's':
        printf("%s", va_arg(args, char*));
      }
      
      *++fmt;
    } else {
      printf("%c", *fmt++);
    }
  }
  
  va_end(args);
  putc('\n', stdout);
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
  lexeme_t *lexeme = malloc(sizeof(lexeme_t));
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
  
  if (*lex->c == '.') {
    lex->c++;
    float f_num = (float) num;
    float digit = 0.1;
    
    do {
      f_num += (float) (*lex->c - '0') * digit;
      digit *= 0.1;
      lex->c++;
    } while (isdigit(*lex->c));
    
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
  char *str_ident = malloc(str_len);
  memcpy(str_ident, word, str_len);
  
  lexeme_t *lexeme = make_lexeme(TK_IDENTIFIER, lex);
  lexeme->data.ident = str_ident;
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

static void lexeme_print(const lexeme_t *lexeme)
{
  if (lexeme) {
    switch (lexeme->token) {
    case TK_CONST_INTEGER:
      printf("%i", lexeme->data.i32);
      break;
    case TK_CONST_FLOAT:
      printf("%f", lexeme->data.f32);
      break;
    case TK_IDENTIFIER:
      printf("%s", lexeme->data.ident);
      break;
    default:
      token_print(lexeme->token);
      break;
    }
  } else {
    printf("EOF");
  }
}

static void token_print(token_t token)
{
  const char *str_token_table[] = {
    "integer",    // TK_CONST_INTEGER
    "float",      // TK_CONST_FLOAT
    "identifier", // TK_IDENTIFIER
    "i32",        // TK_I32
    "f32",        // TK_F32,
    "EOF",        // TK_EOF
  };
  
  if (token < TK_CONST_INTEGER) {
    printf("%c", token);
  } else if (token <= TK_EOF) {
    printf("%s", str_token_table[token - TK_CONST_INTEGER]);
  } else {
    printf("(unknown:%i)", token);
  }
}
