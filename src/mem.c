#include "mem.h"

#include "log.h"
#include "zone.h"
#include <string.h>

static heap_block_t *heap_block_list = NULL;

static void heap_mark_R(scope_t *scope);
static void heap_mark_class(heap_block_t *heap_block, const scope_t *class);
static void heap_mark_array_class(heap_block_t *heap_block, const scope_t *class);
static void heap_mark_var(var_t *var);
static void heap_mark_expr(expr_t *expr);

heap_block_t *stack_mem = NULL;

void stack_init()
{
  stack_mem = heap_alloc_static(1024);
}

void stack_clean()
{
  heap_free(stack_mem);
}

void mem_load(heap_block_t *loc_base, int loc_offset, const type_t *type, expr_t *expr)
{
  expr->loc_base = loc_base;
  expr->loc_offset = loc_offset;
  expr->type = *type;
  if (type_array(type)) {
    expr->block = *((heap_block_t**) &loc_base->block[loc_offset]);
  } else if (type_class(type))
    expr->block = *((heap_block_t**) &loc_base->block[loc_offset]);
  else if (type_cmp(type, &type_string))
    expr->block = *((heap_block_t**) &loc_base->block[loc_offset]);
  else if (type_cmp(type, &type_string))
    expr->block = *((heap_block_t**) &loc_base->block[loc_offset]);
  else if (type_cmp(type, &type_i32))
    expr->i32 = *((int*) &loc_base->block[loc_offset]);
  else if (type_cmp(type, &type_f32))
    expr->f32 = *((float*) &loc_base->block[loc_offset]);
  else
    LOG_DEBUG("unknown type");
}

void mem_assign(heap_block_t *loc_base, int loc_offset, const type_t *type, expr_t *expr)
{
  if (type_array(type))
    *((heap_block_t**) &loc_base->block[loc_offset]) = expr->block;
  else if (type_class(type))
    *((heap_block_t**) &loc_base->block[loc_offset]) = expr->block;
  else if (type_cmp(type, &type_string))
    *((heap_block_t**) &loc_base->block[loc_offset]) = expr->block;
  else if (type_cmp(type, &type_string))
    *((heap_block_t**) &loc_base->block[loc_offset]) = expr->block;
  else if (type_cmp(type, &type_none))
    *((heap_block_t**) &loc_base->block[loc_offset]) = expr->block;
  else if (type_cmp(type, &type_i32))
    *((int*) &loc_base->block[loc_offset]) = expr->i32;
  else if (type_cmp(type, &type_f32))
    *((float*) &loc_base->block[loc_offset]) = expr->f32;
  else
    LOG_DEBUG("unknown type");
}

heap_block_t *heap_alloc_static(int size)
{
  heap_block_t *heap_block = ZONE_ALLOC(sizeof(heap_block_t));
  heap_block->block = ZONE_ALLOC(size);
  heap_block->use = true;
  heap_block->size = size;
  heap_block->next = NULL;
  heap_block->prev = NULL;
  memset(heap_block->block, 0, size);
  return heap_block;
}

heap_block_t *heap_alloc(int size)
{
  heap_block_t *heap_block = heap_alloc_static(size);
  
  memset(heap_block->block, 0, size);
  
  if (heap_block_list) {
    heap_block->next = heap_block_list;
    heap_block->prev = NULL;
    heap_block_list->prev = heap_block;
    heap_block_list = heap_block;
  } else {
    heap_block_list = heap_block;
    heap_block_list->prev = NULL;
    heap_block_list->next = NULL;
  }
  
  return heap_block;
}

heap_block_t *heap_alloc_string(const char *string)
{
  int len = strlen(string);
  heap_block_t *heap_block = heap_alloc(len + 1);
  memcpy(heap_block->block, string, len);
  heap_block->block[len] = 0;
  return heap_block;
}

void heap_free(heap_block_t *heap_block)
{
  ZONE_FREE(heap_block->block);
  ZONE_FREE(heap_block);
}

void heap_clean(scope_t *scope_global)
{
  heap_block_t *heap_block = heap_block_list;
  while (heap_block) {
    heap_block->use = false;
    heap_block = heap_block->next;
  }
  
  heap_mark_R(scope_global);
  
  heap_block = heap_block_list;
  while (heap_block) {
    if (heap_block->use) {
      heap_block = heap_block->next;
      continue;
    }
    
    if (heap_block->next)
      heap_block->next->prev = heap_block->prev;
    if (heap_block->prev)
      heap_block->prev->next = heap_block->next;
    
    if (heap_block == heap_block_list)
      heap_block_list = heap_block->next;
    
    heap_free(heap_block);
    
    heap_block = heap_block->next;
  }
}

static void heap_mark_R(scope_t *scope)
{
  if (!scope)
    return;
  
  if (scope->ret_flag)
    heap_mark_expr(&scope->ret_value);
  
  entry_t *entry = scope->map_var.start;
  while (entry) {
    var_t *var = (var_t*) entry->value;
    heap_mark_var(var);
    entry = entry->s_next;
  }
  
  heap_mark_R(scope->scope_child);
}

static void heap_mark_array_class(heap_block_t *heap_block, const scope_t *class)
{
  heap_block_t **array_class = (heap_block_t**) heap_block->block;
  int array_length = heap_block->size / sizeof(size_t);
  
  for (int i = 0; i < array_length; i++) {
    if (array_class[i]) {
      array_class[i]->use = true;
      heap_mark_class(array_class[i], class);
    }
  }
}

static void heap_mark_class(heap_block_t *heap_block, const scope_t *class)
{
  entry_t *entry = class->map_var.start;
  
  while (entry) {
    heap_mark_var((var_t*) entry->value);
    entry = entry->s_next;
  }
}

static void heap_mark_var(var_t *var)
{
  if (var->type.spec == SPEC_CLASS
  || var->type.spec == SPEC_STRING
  || var->type.arr) {
    expr_t expr;
    mem_load(stack_mem, var->loc, &var->type, &expr);
    heap_mark_expr(&expr);
  }
}

static void heap_mark_expr(expr_t *expr)
{
  if (expr->type.spec == SPEC_CLASS
  || expr->type.spec == SPEC_STRING
  || expr->type.arr) {
    heap_block_t *heap_block = (heap_block_t*) expr->block;
    
    if (!heap_block)
      return;
    
    heap_block->use = true;
    if (expr->type.spec == SPEC_CLASS) {
      if (expr->type.arr)
        heap_mark_array_class(heap_block, expr->type.class);
      else
        heap_mark_class(heap_block, expr->type.class);
    }
  }
}
