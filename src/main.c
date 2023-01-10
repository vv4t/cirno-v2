#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "lex.h"
#include "syntax.h"
#include "interpret.h"

int main(int argc, char *argv[])
{
  lex_t lex = lex_parse("main.cn");
  s_node_t *s_tree = s_parse(&lex);
  s_print(s_tree);
  expr_t expr = int_shell(s_tree);
  
  c_debug("%w", &expr);
  // lex_free(&lex);
  
  return 0;
}
