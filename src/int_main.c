#include "int_local.h"

static scope_t  scope_global;

void int_init()
{
  scope_new(&scope_global, NULL, &type_none, NULL, NULL, false);
  stack_init();
}

bool int_run(const s_node_t *node)
{
  return int_body(&scope_global, node);
}

void int_stop()
{
  scope_free(&scope_global);
  scope_global.scope_child = NULL;
  
  heap_clean(&scope_global);
  stack_clean();
}

bool int_call(const char *ident, expr_t *arg_list, int num_arg_list)
{
  fn_t *fn = scope_find_fn(&scope_global, ident);
  if (!fn) {
    printf("int_call: error: function '%s' undeclared\n", ident);
    return false;
  }
  
  scope_t new_scope;
  scope_new(&new_scope, NULL, &fn->type, &scope_global, fn->scope_parent, true);
  new_scope.size += scope_global.size;
  
  int arg_num = 0;
  s_node_t *head = fn->param;
  while (head) {
    if (arg_num >= num_arg_list) {
      printf("int_call: error: %s(): too few arguments\n", ident);
      goto err_cleanup;
    }
    
    type_t type;
    if (!int_type(&new_scope, &type, head->param_decl.type))
      goto err_cleanup;
    
    if (!type_cmp(&type, &arg_list[arg_num].type))
      goto err_cleanup;
    
    if (scope_find_var(&new_scope, head->param_decl.ident->data.ident)) {
      c_error(
        head->param_decl.ident,
        "redefinition of param '%s'",
        head->param_decl.ident->data.ident);
      goto err_cleanup;
    }
    
    var_t *var = scope_add_var(&new_scope, &type, head->param_decl.ident->data.ident);
    mem_assign(stack_mem, var->loc, &var->type, &arg_list[arg_num]);
    
    head = head->param_decl.next;
    arg_num++;
  }
  
  if (arg_num < num_arg_list) {
    LOG_ERROR("%s(): too many arguments", ident);
err_cleanup:
    scope_free(&new_scope);
    return false;
  }
  
  if (!int_body(&new_scope, fn->node)) {
    scope_free(&new_scope);
    return false;
  }
  
  scope_free(&new_scope);
  
  return true;
}

void int_bind(const char *ident, xaction_t xaction)
{
  scope_add_fn(
    &scope_global,
    &type_none,
    NULL,
    NULL,
    xaction,
    NULL,
    ident,
    false);
}

bool int_arg_load(const scope_t *scope, expr_t *expr, char *ident)
{
  var_t *var = scope_find_var(scope, ident);
  if (!var)
    return false;
  
  mem_load(stack_mem, var->loc, &var->type, expr);
  return true;
}

bool int_load_ident(const scope_t *scope, heap_block_t *heap_block, expr_t *expr, const lexeme_t *lexeme)
{
  var_t *var = scope_find_var(scope, lexeme->data.ident);
  if (var) {
    mem_load(heap_block, var->loc, &var->type, expr);
    return true;
  }
  
  fn_t *fn = scope_find_fn(scope, lexeme->data.ident);
  if (fn) {
    expr->type.spec = SPEC_FN;
    expr->type.arr = false;
    expr->type.class = NULL;
    expr->fn = fn;
    expr->loc_offset = 0;
    expr->loc_base = heap_block;
    
    return true;
  }
  
  return false;
}
