#ifndef ZONE_H
#define ZONE_H

#define ZONE_DEBUG 1

#if ZONE_DEBUG == 1
  #define ZONE_ALLOC(size) zone_alloc(__FILE__, __LINE__, size)
  #define ZONE_FREE(block) zone_free(block)
#else
  #include <stdlib.h>
  #define ZONE_ALLOC malloc
  #define ZONE_FREE free
#endif

void *zone_alloc(const char *src, int line, int size);
void zone_free(void *block);
void zone_log();

#endif
