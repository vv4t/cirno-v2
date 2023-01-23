#include "zone.h"
#include "sdl.h"
#include "lex.h"
#include "syntax.h"
#include "interpret.h"
#include "lib.h"
#include <unistd.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
  bool flag_sdl = false;
  
  extern char *optarg;
  extern int optind;
  
  int c = 0;
  bool err = 0;
  
  static char usage[] = "usage: %s [-w] <file>\n";
  
  while ((c = getopt(argc, argv, "w")) != -1) {
    switch (c) {
    case 'w':
      flag_sdl = true;
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
    printf("cirno: could not open '%s'\n", file);
    return 1;
  }
  
  s_node_t *node = s_parse(&lex);
  if (!s_error()) {
    int_init();
    
    lib_load_stdlib();
    lib_load_math();
    
    if (flag_sdl)
      sdl_init();
    
    if (int_run(node)) {
      if (flag_sdl)
        while (sdl_frame());
    }
    
    int_stop();
  }
  
  s_free(node);
  lex_free(&lex);
  
  zone_log();
  
  return 0;
}
