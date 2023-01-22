#ifndef ZONE_H
#define ZONE_H

#define ZONE_DEBUG 0

#if ZONE_DEBUG == 1
  #define ZONE_ALLOC(size) zone_alloc(__FILE__, __LINE__, size)
  #define ZONE_FREE(block) zone_free(block)
  #define ZONE_REALLOC(block, size) zone_realloc(block, size)
#else
  #include <stdlib.h>
  #define ZONE_ALLOC malloc
  #define ZONE_FREE free
  #define ZONE_REALLOC realloc
#endif

void *zone_alloc(const char *src, int line, int size);
void *zone_realloc(void *block, int size);
void zone_free(void *block);
void zone_log();

#endif
