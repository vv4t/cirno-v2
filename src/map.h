#ifndef MAP_H
#define MAP_H

#include <stdbool.h>

typedef struct entry_s {
  void            *value;
  int             map_id;
  int             id;
  const char      *key;
  
  struct entry_s  *h_next;
  struct entry_s  *h_prev;
  
  struct entry_s  *s_next;
} entry_t;

typedef struct {
  int     map_id;
  entry_t *start;
} map_t;

void  map_new(map_t *map);
void  map_flush(map_t *map, void (*fn_free)(void *block));
void  *map_get(const map_t *map, const char *key);
bool  map_put(map_t *map, const char *key, void *value);

#endif
