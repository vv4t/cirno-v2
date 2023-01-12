#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "lex.h"
#include "syntax.h"
#include "interpret.h"

int main(int argc, char *argv[])
{
  lex_t lex = lex_parse("main.cn");
  s_node_t *node = s_parse(&lex);
  
  // s_print_node(node);
  if (!node)
    return 0;
  
  bool err = interpret(node);
  printf("status: %i\n", err);
  
  // lex_free(&lex);
  
  return 0;
}
