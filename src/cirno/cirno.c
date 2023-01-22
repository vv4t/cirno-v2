#include "cirno.h"

#include "lex.h"
#include "syntax.h"
#include "interpret.h"
#include <stdio.h>

static lex_t    cirno_lex;
static s_node_t *cirno_node;
static bool     cirno_on = false;

void cirno_init()
{
  int_init();
}

bool cirno_load(const char *file)
{
  if (cirno_on)
    return false;
  
  cirno_on = false;
  cirno_node = NULL;
  
  if (!lex_parse(&cirno_lex, file)) {
    printf("cirno: could not open '%s'\n", file);
    return false;
  }
  
  cirno_node = s_parse(&cirno_lex);
  if (s_error()) {
    s_free(cirno_node);
    lex_free(&cirno_lex);
    
    return false;
  }
  
  cirno_on = true;
  
  int_run(cirno_node);
}

void cirno_unload()
{
  if (cirno_on) {
    int_stop();
    s_free(cirno_node);
    lex_free(&cirno_lex);
    cirno_node = NULL;
    cirno_on = false;
  }
}

void cirno_bind(const char *ident, xaction_t xaction)
{
  int_bind(ident, xaction);
}

bool cirno_call(const char *ident, expr_t *arg_list, int num_arg_list)
{
  int_call(ident, arg_list, num_arg_list);
}

bool cirno_arg_load(const scope_t *scope, expr_t *expr, char *ident)
{
  int_arg_load(scope, expr, ident);
}
