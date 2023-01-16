#include "zone.h"

#include "log.h"
#include <stdlib.h>

typedef struct zone_block_s {
  const char          *src;
  int                 line;
  int                 size;
  struct zone_block_s *prev;
  struct zone_block_s *next;
} zone_block_t;

static zone_block_t *zone_block_list = NULL;
static int          num_zone_block = 0;

void *zone_alloc(const char *src, int line, int size)
{
  char *block = malloc(sizeof(zone_block_t) + size);
  zone_block_t *zone_block = (zone_block_t*) block;
  zone_block->src = src;
  zone_block->line = line;
  zone_block->size = size;
  
  if (zone_block_list) {
    zone_block->next = zone_block_list;
    zone_block->prev = NULL;
    zone_block_list->prev = zone_block;
    zone_block_list = zone_block;
  } else {
    zone_block_list = zone_block;
    zone_block_list->prev = NULL;
    zone_block_list->next = NULL;
  }
  
  num_zone_block++;
  
  char *mem_block = block + sizeof(zone_block_t);
  return mem_block;
}

void zone_free(const char *src, int line, void *block)
{
  zone_block_t *zone_block = (zone_block_t*) ((char*) block - sizeof(zone_block_t));
  
  if (zone_block->next)
    zone_block->next->prev = zone_block->prev;
  if (zone_block->prev)
    zone_block->prev->next = zone_block->next;
  
  if (zone_block == zone_block_list)
    zone_block_list = zone_block->next;
  
  num_zone_block--;
  
  free(zone_block);
}

void zone_log()
{
  LOG_DEBUG("num_zone_block: %i", num_zone_block);
  
  zone_block_t *zone_block = zone_block_list;
  while (zone_block) {
    LOG_DEBUG(
      "%s:%i %ib",
      zone_block->src,
      zone_block->line,
      zone_block->size);
    
    zone_block = zone_block->next;
  }
}
