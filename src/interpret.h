#ifndef INTERPRET_H
#define INTERPRET_H

#include "data.h"
#include "syntax.h"
#include <stdbool.h>

extern void int_start();
extern bool int_run(const s_node_t *node);
extern void int_stop();

extern bool int_call(const char *ident, expr_t *arg_list, int num_arg_list);
extern void int_bind(const char *ident, xaction_t xaction);
extern bool int_var_load(const scope_t *scope, expr_t *expr, char *ident);

#endif
