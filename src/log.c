#include "log.h"

#include "data.h"
#include <stdlib.h>

static void c_printf(const char *fmt, va_list args);
static void expr_print(const expr_t *expr);
static void type_print(const type_t *type);
static void lexeme_print(const lexeme_t *lexeme);
static void token_print(token_t token);

void c_error(const lexeme_t *lexeme, const char *fmt, ...)
{
  if (!lexeme)
    return;
  
  va_list args;
  va_start(args, fmt);
  printf("%s:%i:error: ", lexeme->src, lexeme->line);
  c_printf(fmt, args);
  va_end(args);
  
  putc('\n', stdout);
}

void c_debug(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  c_printf(fmt, args);
  va_end(args);
  
  putc('\n', stdout);
}

static void c_printf(const char *fmt, va_list args)
{
  while (*fmt) {
    if (*fmt == '%') {
      switch (*++fmt) {
      case 't': // token
        token_print(va_arg(args, token_t));
        break;
      case 'z': // type
        type_print(va_arg(args, const type_t *));
        break;
      case 'w': // expression
        expr_print(va_arg(args, const expr_t *));
        break;
      case 'l': // lexeme
        lexeme_print(va_arg(args, const lexeme_t *));
        break;
      case 's':
        printf("%s", va_arg(args, char*));
        break;
      }
      
      *++fmt;
    } else {
      printf("%c", *fmt++);
    }
  }
}

static void expr_print(const expr_t *expr)
{
  switch (expr->type.spec) {
  case SPEC_I32:
    printf("%i", expr->i32);
    break;
  case SPEC_F32:
    printf("%f", expr->f32);
    break;
  }
}

static void type_print(const type_t *type)
{
  const char *str_spec_table[] = {
    "none",
    "i32",
    "f32",
    "class"
  };
  
  printf("%s", str_spec_table[type->spec]);
  
  if (type->size)
    printf("[%i]", type->size);
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
    "f32",        // TK_F32
    "if",         // TK_IF
    "class",      // TK_CLASS
    "class_def",  // TK_CLASS_DEF
    "print",      // TK_PRINT
    "while",      // TK_WHILE
    "EOF"         // TK_EOF
  };
  
  if (token < TK_CONST_INTEGER) {
    printf("%c", token);
  } else if (token <= TK_EOF) {
    printf("%s", str_token_table[token - TK_CONST_INTEGER]);
  } else {
    printf("(unknown:%i)", token);
  }
}
