#include "cirno/cirno.h"
#include <unistd.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
  bool flag_debug = false;
  
  extern char *optarg;
  extern int optind;
  
  int c = 0;
  bool err = 0;
  
  static char usage[] = "usage: %s <file>\n";
  
  while ((c = getopt(argc, argv, "")) != -1) {
    switch (c) {
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
  
  cirno_init();
  if (cirno_load(file))
    cirno_unload();
  
  return 0;
}
