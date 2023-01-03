#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "log.h"
#include "lex.h"
#include "syntax.h"
#include "interpret.h"

int main(int argc, char *argv[])
{
  FILE *fp = fopen("main.cn", "rb");
  
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
  
  lex_t lex = lex_parse(buffer);
  s_node_t *s_tree = s_parse(&lex);
  s_print(s_tree);
  
  // ast_expr_t *ast_expr = ast_parse(s_tree);
  
  // interpret(ast_expr);
  
  free(buffer);
  
  return 0;
}
