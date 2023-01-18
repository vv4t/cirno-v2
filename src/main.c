#include <stdio.h>
#include <stdlib.h>

#include "map.h"
#include "log.h"
#include "lex.h"
#include "syntax.h"
#include "zone.h"
#include "interpret.h"
#include <stdbool.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
  bool flag_debug = false;
  
  extern char *optarg;
  extern int optind;
  
  int c = 0;
  bool err = 0;
  
  static char usage[] = "usage: %s [-D] <file>\n";
  
  while ((c = getopt(argc, argv, "D")) != -1) {
    switch (c) {
    case 'D':
      flag_debug = true;
      break;
    case '?':
      err = true;
      break;
    }
  }
  
  if ((optind+1) > argc) {
    printf("%s: missing input file\n", argv[0]);
    printf(usage, argv[0]);
    return 1;
  } else if (err) {
    printf(usage, argv[0]);
    return 1;
  }
  
  const char *file = argv[optind];
  
  lex_t lex;
  if (!lex_parse(&lex, file)) {
    printf("%s: could not open '%s'\n", argv[0], file);
    return 1;
  }
  
  s_node_t *node = s_parse(&lex);
  
  if (!s_error())
    interpret(node);
  
  lex_free(&lex);
  s_free(node);
  
  if (flag_debug)
    zone_log();
  
  return 0;
}
