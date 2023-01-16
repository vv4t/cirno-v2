#include "map.h"

#include "zone.h"
#include <stdio.h>
#include <string.h>

#define MAX_ENTRIES 1021

typedef unsigned int hash_t;

static int      map_id = 1;
static entry_t  *entry_dict[MAX_ENTRIES] = {0};

static hash_t hash_key(const char *key)
{
  hash_t hash = 5381;
  
  const char *c = key;
  while (*c)
    hash = ((hash << 5) + hash) + (unsigned char) *c++;
  
  return hash;
}

void map_new(map_t *map)
{
  map->map_id = map_id++;
  map->start = NULL;
}

static entry_t *new_entry(map_t *map, const char *key, void *value, int id)
{
  entry_t *entry = ZONE_ALLOC(sizeof(entry_t));
  entry->value = value;
  entry->map_id = map->map_id;
  entry->key = key;
  entry->id = id % MAX_ENTRIES;
  
  entry->h_next = NULL;
  entry->h_prev = NULL;
  
  entry->s_next = NULL;
  
  return entry;
}

void map_flush(map_t *map)
{
  entry_t *entry = map->start;
  
  while (entry) {
    entry_t *next = entry->s_next;
    
    if (entry->h_next)
      entry->h_next->h_prev = entry->h_prev;
    if (entry->h_prev)
      entry->h_prev->h_next = entry->h_next;
    
    if (entry == entry_dict[entry->id])
      entry_dict[entry->id] = entry->h_next;
    
    ZONE_FREE(entry->value);
    ZONE_FREE(entry);
    
    entry = next;
  }
}

bool map_put(map_t *map, const char *key, void *value)
{
  int id = hash_key(key) % MAX_ENTRIES;
  
  entry_t *entry = entry_dict[id];
  
  if (entry) {
    while (entry->h_next) {
      if (strcmp(entry->key, key) == 0 && entry->map_id == map->map_id)
        return false;
      
      entry = entry->h_next;
    }
    
    entry->h_next = new_entry(map, key, value, id);
    entry->h_next->h_prev = entry;
    entry = entry->h_next;
  } else {
    entry = entry_dict[id] = new_entry(map, key, value, id);
  }
  
  if (map->start) {
    entry->s_next = map->start;
    map->start = entry;
  } else
    map->start = entry;
  
  return true;
}

void *map_get(const map_t *map, const char *key)
{
  int id = hash_key(key) % MAX_ENTRIES;
  
  entry_t *entry = entry_dict[id];
  
  if (entry) {
    while (entry) {
      if (strcmp(entry->key, key) == 0 && entry->map_id == map->map_id)
        return entry->value;
      
      entry = entry->h_next;
    }
  }
  
  return NULL;
}
