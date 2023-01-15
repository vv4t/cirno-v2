#ifndef MAP_H
#define MAP_H

typedef int map_t;

map_t map_new();
void  map_flush(map_t map);
void  *map_get(map_t map, const char *key);
int   map_put(map_t map, const char *key, void *value);

#endif
