#include <stdio.h>
#include <stdlib.h>

#include "map.h"
#include "log.h"
#include "lex.h"
#include "syntax.h"
#include "zone.h"
#include "interpret.h"

int main(int argc, char *argv[])
{
  lex_t lex = lex_parse("main.cn");
  s_node_t *node = s_parse(&lex);
  
  if (!s_error()) {
    bool err = interpret(node);
    printf("status: %i\n", err);
  }
  
  lex_free(&lex);
  s_free(node);
  
  zone_log();
  
  return 0;
}
