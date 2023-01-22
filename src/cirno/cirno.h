#ifndef CIRNO_H
#define CIRNO_H

#include "data.h"
#include <stdbool.h>

extern void cirno_init();
extern bool cirno_load(const char *file);
extern void cirno_unload();

extern void cirno_bind(const char *ident, xaction_t xaction);
extern bool cirno_call(const char *ident, expr_t *arg_list, int num_arg_list);
extern bool cirno_arg_load(const scope_t *scope, expr_t *expr, char *ident);

#endif
