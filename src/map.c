#include "map.h"

#include "zone.h"
#include <stdio.h>
#include <string.h>

#define MAX_ENTRIES 1021

typedef unsigned int hash_t;
typedef struct entry_s entry_t;

struct entry_s {
  void        *value;
  map_t       map;
  const char  *key;
  entry_t     *next;
};

static int      map_id = 1;
static entry_t  *entry_dict[MAX_ENTRIES] = {0};

map_t map_new()
{
  return map_id++;
}

static entry_t *new_entry(int map, const char *key, void *value)
{
  entry_t *entry = ZONE_ALLOC(sizeof(entry_t));
  entry->map = map;
  entry->key = key;
  entry->value = value;
  entry->next = NULL;
  return entry;
}

static hash_t hash_key(const char *key)
{
  hash_t hash = 5381;
  
  const char *c = key;
  while (*c)
    hash = ((hash << 5) + hash) + (unsigned char) *c++;
  
  return hash;
}

void map_flush(map_t map)
{
  for (int i = 0; i < MAX_ENTRIES; i++) {
    entry_t *entry = entry_dict[i];
    
    entry_t *prev_entry = NULL;
    while (entry) {
      if (entry->map == map) {
        if (prev_entry)
          prev_entry->next = entry->next;
        else
          entry_dict[i] = entry->next;
        
        ZONE_FREE(entry);
        ZONE_FREE(entry->value);
      }
      
      prev_entry = entry;
      entry = entry->next;
    }
  }
}

int map_put(map_t map, const char *key, void *value)
{
  int id = hash_key(key) % MAX_ENTRIES;
  
  entry_t *entry = entry_dict[id];
  
  if (entry) {
    while (entry->next) {
      if (strcmp(entry->key, key) == 0 && entry->map == map)
        return 0;
      
      entry = entry->next;
    }
    
    entry->next = new_entry(map, key, value);
  } else {
    entry_dict[id] = new_entry(map, key, value);
  }
  
  return 1;
}

void *map_get(map_t map, const char *key)
{
  int id = hash_key(key) % MAX_ENTRIES;
  
  entry_t *entry = entry_dict[id];
  
  if (entry) {
    while (entry) {
      if (strcmp(entry->key, key) == 0 && entry->map == map)
        return entry->value;
      
      entry = entry->next;
    }
  }
  
  return NULL;
}
