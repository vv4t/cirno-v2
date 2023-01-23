#include "log.h"

#include "data.h"
#include "syntax.h"
#include <stdlib.h>

static void c_printf(const char *fmt, va_list args);
static void s_node_print(const s_node_t *node);
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
      case 'i':
        printf("%i", va_arg(args, int));
        break;
      case 'h': // s_node_t expr
        s_node_print(va_arg(args, const s_node_t *));
        break;
      default:
        putc('%', stdout);
        break;
      }
      
      *++fmt;
    } else {
      printf("%c", *fmt++);
    }
  }
}

static void s_node_print(const s_node_t *node)
{
  if (!node)
    return;
  
  switch (node->node_type) {
  case S_BINOP:
    s_node_print(node->binop.lhs);
    lexeme_print(node->binop.op);
    s_node_print(node->binop.rhs);
    break;
  case S_UNARY:
    break;
  case S_INDEX:
    s_node_print(node->index.base);
    putc('[', stdout);
    s_node_print(node->index.index);
    putc(']', stdout);
    break;
  case S_DIRECT:
    s_node_print(node->direct.base);
    putc('.', stdout);
    printf("%s", node->direct.child_ident->data.ident);
    break;
  case S_PROC:
    s_node_print(node->proc.base);
    putc('(', stdout);
    s_node_print(node->proc.arg);
    putc(')', stdout);
    break;
  case S_CONSTANT:
    lexeme_print(node->constant.lexeme);
    break;
  case S_NEW:
    printf("new %s", node->new.class_ident->data.ident);
    break;
  case S_ARG:
    s_node_print(node->arg.body);
    if (node->arg.next) {
      putc(',', stdout);
      s_node_print(node->arg.next);
    }
    break;
  default:
    LOG_ERROR("unknown node_type (%i)", node->node_type);
    break;
  }
}

static void expr_print(const expr_t *expr)
{
  if (type_array(&expr->type)) {
    type_print(&expr->type);
    printf(" (%p)", &expr->loc_base[expr->loc_offset]);
    return;
  }
  
  switch (expr->type.spec) {
  case SPEC_NONE:
    printf("(none)");
    break;
  case SPEC_I32:
    printf("%i", expr->i32);
    break;
  case SPEC_F32:
    printf("%f", expr->f32);
    break;
  case SPEC_CLASS:
    type_print(&expr->type);
    printf(" [ %p ]", expr->align_of);
    break;
  case SPEC_FN:
    printf("fn:");
    fn_t *fn = (fn_t*) expr->align_of;
    if (fn)
      type_print(&fn->type);
    break;
  case SPEC_STRING:
    printf("%s", (char*) (*(heap_block_t*) expr->align_of).block);
    break;
  }
}

static void type_print(const type_t *type)
{
  const char *str_spec_table[] = {
    "none",
    "i32",
    "f32",
    "class",
    "fn",
    "string"
  };
  
  printf("%s", str_spec_table[type->spec]);
  
  if (type->spec == SPEC_CLASS)
    printf(" %s", type->class->ident);
  
  if (type->arr)
    printf("[]");
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
    case TK_STRING_LITERAL:
      printf("\"%s\"", lexeme->data.string_literal);
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
    "integer",        // TK_CONST_INTEGER
    "float",          // TK_CONST_FLOAT
    "identifier",     // TK_IDENTIFIER
    "i32",            // TK_I32
    "f32",            // TK_F32
    "if",             // TK_IF
    "class",          // TK_CLASS
    "class_def",      // TK_CLASS_DEF
    "print",          // TK_PRINT
    "while",          // TK_WHILE
    "->",             // TK_PTR_OP
    "fn",             // TK_FN
    "return",         // TK_RETURN
    "new",            // TK_NEW
    "string-literal", // TK_STRING_LITERAL
    "string",         // TK_STRING
    "array_init",     // TK_ARRAY_INIT
    "++",             // TK_INC
    "--",             // TK_DEC
    ">=",             // TK_GE
    "<=",             // TK_LE
    "==",             // TK_EQ
    "!=",             // TK_NE
    "&&",             // TK_AND
    "||",             // TK_OR
    "+=",             // TK_ADD_ASSIGN
    "-=",             // TK_SUB_ASSIGN
    "*=",             // TK_MUL_ASSIGN
    "/=",             // TK_DIV_ASSIGN
    "for",            // TK_FOR
    "EOF"             // TK_EOF
  };
  
  if (token < TK_CONST_INTEGER) {
    printf("%c", token);
  } else if (token <= TK_EOF) {
    printf("%s", str_token_table[token - TK_CONST_INTEGER]);
  } else {
    printf("(unknown:%i)", token);
  }
}
