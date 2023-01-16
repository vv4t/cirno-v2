#ifndef ZONE_H
#define ZONE_H

#define ZONE_ALLOC(size) zone_alloc(__FILE__, __LINE__, size)
#define ZONE_FREE(block) zone_free(__FILE__, __LINE__, block)

void *zone_alloc(const char *src, int line, int size);
void zone_free(const char *src, int line, void *block);
void zone_log();

#endif
