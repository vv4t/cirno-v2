#ifndef HEAP_H
#define HEAP_H

#include "data.h"

extern void mem_load(heap_block_t *loc_base, int loc_offset, const type_t *type, expr_t *expr);
extern void mem_assign(heap_block_t *loc_base, int loc_offset, const type_t *type, expr_t *expr);

extern heap_block_t *heap_alloc_static(int size);
extern heap_block_t *heap_alloc(int size);
extern heap_block_t *heap_alloc_string(const char *string);
extern void         heap_free(heap_block_t *heap_block);
extern void         heap_clean(scope_t *scope_global);

extern heap_block_t *stack_mem;
extern void         stack_init();
extern void         stack_clean();

#endif
