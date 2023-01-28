#include "int_local.h"

bool int_expr(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  switch (node->node_type) {
  case S_BINOP:
    return int_binop(scope, expr, node);
  case S_UNARY:
    return int_unary(scope, expr, node);
  case S_INDEX:
    return int_index(scope, expr, node);
  case S_DIRECT:
    return int_direct(scope, expr, node);
  case S_PROC:
    return int_proc(scope, expr, node);
  case S_CONSTANT:
    return int_constant(scope, expr, node);
  case S_NEW:
    return int_new(scope, expr, node);
  case S_ARRAY_INIT:
    return int_array_init(scope, expr, node);
  case S_POST_OP:
    return int_post_op(scope, expr, node);
  default:
    LOG_ERROR("unknown expr node_type (%i)", node->node_type);
    return false;
  }
}

bool int_proc(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t base;
  if (!int_expr(scope, &base, node->proc.base))
    return false;
  
  if (!type_fn(&base.type)) {
    c_error(
      node->proc.left_bracket,
      "attempt to call non-function");
    return false;
  }
  
  fn_t *fn = (fn_t*) base.block;
  
  if (!fn) {
    c_error(
      node->proc.left_bracket,
      "attempt to call invalid function");
    return false;
  }
  
  if (!fn->node && !fn->xaction) {
    c_error(
      node->proc.left_bracket,
      "attempt to call function without body");
    return false;
  }
  
  scope_t new_scope;
  if (fn->scope_class) {
    scope_new(&new_scope, NULL, &fn->type, scope, fn->scope_parent->scope_find, true);
    new_scope.size += scope->size;
    
    expr_t self_expr;
    self_expr.type.spec = SPEC_CLASS;
    self_expr.type.arr = false;
    self_expr.type.class = fn->scope_class;
    self_expr.block = base.loc_base;
    self_expr.loc_base = NULL;
    self_expr.loc_offset = 0;
    
    var_t *var = scope_add_var(&new_scope, &self_expr.type, "this");
    mem_assign(stack_mem, var->loc, &var->type, &self_expr);
  } else {
    scope_new(&new_scope, NULL, &fn->type, scope, fn->scope_parent, true);
    new_scope.size += scope->size;
  }
  
  new_scope.ret_type = fn->type;
  
  s_node_t *arg = node->proc.arg;
  s_node_t *head = fn->param;
  while (head) {
    if (!arg) {
      c_error(
        node->proc.left_bracket,
        "too few arguments to function '%h'",
        node);
      goto err_cleanup;
    }
    
    type_t type;
    if (!int_type(&new_scope, &type, head->param_decl.type))
      goto err_cleanup;
    
    scope->size += new_scope.size;
    expr_t arg_value;
    if (!int_expr(scope, &arg_value, arg->arg.body))
      goto err_cleanup;
    scope->size -= new_scope.size;
    
    if (!expr_cast(&arg_value, &type)) {
      c_error(
        head->param_decl.ident,
        "expected '%z' but argument is of type '%z'",
        &type,
        &arg_value.type);
      goto err_cleanup;
    }
    
    var_t *var = scope_add_var(&new_scope, &type, head->param_decl.ident->data.ident);
    if (!var) {
      c_error(
        head->param_decl.ident,
        "redefinition of param '%s'",
        head->param_decl.ident->data.ident);
      goto err_cleanup;
    }
    mem_assign(stack_mem, var->loc, &var->type, &arg_value);
    
    head = head->param_decl.next;
    arg = arg->arg.next;
  }
  
  if (arg) {
    c_error(
      node->proc.left_bracket,
      "too many arguments to function '%h'",
      node);
    goto err_cleanup;
  }
  
  if (fn->node) {
    if (!int_body(&new_scope, fn->node)) {
err_cleanup:
      scope_free(&new_scope);
      scope->scope_child = NULL;
      return false;
    }
  } else {
    fn->xaction(&new_scope.ret_value, &new_scope);
  }
  
  if (fn->is_new) {
    expr_t self_expr;
    self_expr.type.spec = SPEC_CLASS;
    self_expr.type.arr = false;
    self_expr.type.class = fn->scope_class;
    self_expr.block = base.loc_base;
    self_expr.loc_base = NULL;
    self_expr.loc_offset = 0;
    
    *expr = self_expr;
  } else {
    *expr = new_scope.ret_value;
  }
  
  scope_free(&new_scope);
  scope->scope_child = NULL;
  
  return true;
}

bool int_unary(scope_t *scope, expr_t *expr, const s_node_t *node)
{  
  expr_t rhs;
  if (!int_expr(scope, &rhs, node->unary.rhs))
    return false;
  
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  
  if (node->unary.op->token == '-') {
    if (type_cmp(&rhs.type, &type_i32)) {
      expr->i32 = -rhs.i32;
      expr->type = type_i32;
    } else if (type_cmp(&rhs.type, &type_f32)) {
      expr->f32 = -rhs.f32;
      expr->type = type_f32;
    } else
      goto err_no_op;
  } else if (node->unary.op->token == '!') {
    if (type_cmp(&rhs.type, &type_i32)) {
      expr->i32 = !rhs.i32;
      expr->type = type_i32;
    } else
      goto err_no_op;
  } else {
err_no_op:
    c_error(
      node->unary.op,
      "unknown operand type for '%t': '%z'",
      node->unary.op->token,
      &rhs.type);
    return false;
  }
  
  return true;
}

static bool binop_i32(expr_t *expr, const expr_t *lhs, token_t op, const expr_t *rhs)
{
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  expr->type = type_i32;
  switch (op) {
  case '+':
    expr->i32 = lhs->i32 + rhs->i32;
    break;
  case '-':
    expr->i32 = lhs->i32 - rhs->i32;
    break;
  case '*':
    expr->i32 = lhs->i32 * rhs->i32;
    break;
  case '/':
    expr->i32 = lhs->i32 / rhs->i32;
    break;
  case '<':
    expr->i32 = lhs->i32 < rhs->i32;
    break;
  case '>':
    expr->i32 = lhs->i32 > rhs->i32;
    break;
  case TK_LE:
    expr->i32 = lhs->i32 <= rhs->i32;
    break;
  case TK_GE:
    expr->i32 = lhs->i32 >= rhs->i32;
    break;
  case TK_EQ:
    expr->i32 = lhs->i32 == rhs->i32;
    break;
  case TK_NE:
    expr->i32 = lhs->i32 != rhs->i32;
    break;
  case TK_AND:
    expr->i32 = lhs->i32 && rhs->i32;
    break;
  case TK_OR:
    expr->i32 = lhs->i32 || rhs->i32;
    break;
  default:
    return false;
  }
  
  return true;
}

static bool binop_f32(expr_t *expr, const expr_t *lhs, token_t op, const expr_t *rhs)
{
  expr->type = type_f32;
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  switch (op) {
  case '+':
    expr->f32 = lhs->f32 + rhs->f32;
    break;
  case '-':
    expr->f32 = lhs->f32 - rhs->f32;
    break;
  case '*':
    expr->f32 = lhs->f32 * rhs->f32;
    break;
  case '/':
    expr->f32 = lhs->f32 / rhs->f32;
    break;
  case '<':
    expr->i32 = lhs->f32 < rhs->f32;
    expr->type = type_i32;
    break;
  case '>':
    expr->i32 = lhs->f32 > rhs->f32;
    expr->type = type_i32;
    break;
  case TK_GE:
    expr->f32 = lhs->f32 >= rhs->f32;
    expr->type = type_i32;
    break;
  case TK_LE:
    expr->f32 = lhs->f32 <= rhs->f32;
    expr->type = type_i32;
    break;
  case TK_EQ:
    expr->f32 = lhs->f32 == rhs->f32;
    expr->type = type_i32;
    break;
  case TK_NE:
    expr->f32 = lhs->f32 != rhs->f32;
    expr->type = type_i32;
    break;
  default:
    return false;
  }
  
  return true;
}

static bool binop_string(expr_t *expr, const expr_t *lhs, token_t op, const expr_t *rhs)
{
  if (op == '+') {
    heap_block_t *str_lhs = lhs->block;
    heap_block_t *str_rhs = rhs->block;
    
    int new_len = str_lhs->size + str_rhs->size - 2;
    
    heap_block_t *concat_str = heap_alloc(new_len + 1);
    
    memcpy(concat_str->block, str_lhs->block, str_lhs->size - 1);
    memcpy(&concat_str->block[str_lhs->size - 1], str_rhs->block, str_rhs->size - 1);
    
    concat_str->block[new_len] = 0;
    
    expr->type = type_string;
    expr->block = concat_str;
    expr->loc_base = NULL;
    expr->loc_offset = 0;
  } else {
    return false;
  }
  
  return true;
}

bool int_binop(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t lhs;
  if (!int_expr(scope, &lhs, node->binop.lhs))
    return false;
  
  expr_t rhs;
  if (!int_expr(scope, &rhs, node->binop.rhs))
    return false;
  
  if (node->binop.op->token == '='
  || node->binop.op->token >= TK_ADD_ASSIGN
  && node->binop.op->token <= TK_DIV_ASSIGN) {
    if (!expr_lvalue(&lhs)) {
      c_error(node->binop.op, "lvalue required as left operand of assignment");
      return false;
    }
    
    if (type_cmp(&lhs.type, &type_i32) && expr_cast(&rhs, &type_i32)) {
      switch (node->binop.op->token) {
      case '=':
        lhs.i32 = rhs.i32;
        break;
      case TK_ADD_ASSIGN:
        lhs.i32 += rhs.i32;
        break;
      case TK_SUB_ASSIGN:
        lhs.i32 -= rhs.i32;
        break;
      case TK_MUL_ASSIGN:
        lhs.i32 *= rhs.i32;
        break;
      case TK_DIV_ASSIGN:
        lhs.i32 /= rhs.i32;
        break;
      default:
        goto err_no_op;
      }
    } else if (type_cmp(&lhs.type, &type_f32) && expr_cast(&rhs, &type_f32)) {
      switch (node->binop.op->token) {
      case '=':
        lhs.f32 = rhs.f32;
        break;
      case TK_ADD_ASSIGN:
        lhs.f32 += rhs.f32;
        break;
      case TK_SUB_ASSIGN:
        lhs.f32 -= rhs.f32;
        break;
      case TK_MUL_ASSIGN:
        lhs.f32 *= rhs.f32;
        break;
      case TK_DIV_ASSIGN:
        lhs.f32 /= rhs.f32;
        break;
      default:
        goto err_no_op;
      }
    } else if (type_cmp(&lhs.type, &type_string) && type_cmp(&rhs.type, &type_string)) {
      switch (node->binop.op->token) {
      case '=':
        lhs.block = rhs.block;
        break;
      case TK_ADD_ASSIGN:
        binop_string(expr, &lhs, '+', &rhs);
        break;
      default:
        goto err_no_op;
      }
      
      expr->loc_base = lhs.loc_base;
      expr->loc_offset = lhs.loc_offset;
      lhs = *expr;
    } else if ((type_class(&lhs.type) && type_class(&rhs.type))
    || (type_array(&lhs.type) && type_array(&rhs.type))
    || (type_cmp(&lhs.type, &type_string) && type_cmp(&rhs.type, &type_string))
    && type_cmp(&lhs.type, &rhs.type)) {
      switch (node->binop.op->token) {
      case '=':
        lhs.block = rhs.block;
        break;
      default:
        goto err_no_op;
      }
    } else
      goto err_no_op;
    
    mem_assign(lhs.loc_base, lhs.loc_offset, &lhs.type, &lhs);
    *expr = lhs;
  } else {
    expr->loc_base = NULL;
    expr->loc_offset = 0;
    if (type_cmp(&lhs.type, &type_i32) && type_cmp(&rhs.type, &type_i32)) {
      if (!binop_i32(expr, &lhs, node->binop.op->token, &rhs))
        goto err_no_op;
    } else if (expr_cast(&lhs, &type_f32) && expr_cast(&rhs, &type_f32)) {
      if (!binop_f32(expr, &lhs, node->binop.op->token, &rhs))
        goto err_no_op;
    } else if (type_cmp(&lhs.type, &type_string) && type_cmp(&rhs.type, &type_string)) {
      if (!binop_string(expr, &lhs, node->binop.op->token, &rhs))
        goto err_no_op;
    } else {
err_no_op:
      c_error(
        node->binop.op,
        "unknown operand type for '%t': '%z' and '%z' '%h'",
        node->binop.op->token,
        &lhs.type, &rhs.type,
        node);
      return false;
    }
  }
  
  return true;
}

static bool array_direct(expr_t *expr, const s_node_t *node, const expr_t *base)
{
  if (strcmp(node->direct.child_ident->data.ident, "length") == 0) {
    expr_i32(expr, base->block->size / type_size_base(&base->type));
  } else {
    c_error(
      node->direct.child_ident,
      "request for unknown member '%s' in array",
      node->direct.child_ident->data.ident);
    return false;
  }
  
  return true;
}

bool int_direct(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t base;
  if (!int_expr(scope, &base, node->direct.base))
    return false;
  
  if (type_array(&base.type))
    return array_direct(expr, node, &base);
  
  if (!type_class(&base.type)) {
    c_error(
      node->direct.child_ident,
      "request for member '%s' in non-class",
      node->direct.child_ident->data.ident);
    return false;
  }
  
  if (!base.block) {
    c_error(
      node->direct.child_ident,
      "request for member '%s' in uninitialised class",
      node->direct.child_ident->data.ident);
    return false;
  }
  
  if (!int_load_ident(base.type.class, base.block, expr, node->direct.child_ident)) {
    c_error(
      node->direct.child_ident,
      "'class %s' has no member named '%s'",
      base.type.class->ident,
      node->direct.child_ident->data.ident);
    return false;
  }
  
  return true;
}

bool int_index(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t base;
  if (!int_expr(scope, &base, node->index.base))
    return false;
  
  if (!type_array(&base.type)) {
    c_error(node->index.left_bracket, "subscripted value is not an array '%h'", node);
    return false;
  }
  
  expr_t index;
  if (!int_expr(scope, &index, node->index.index))
    return false;
  
  if (!type_cmp(&index.type, &type_i32)) {
    c_error(
      node->index.left_bracket,
      "array subscript is of type '%z', not 'i32' '%h'",
      &index.type,
      node);
    return false;
  }
  
  int offset = index.i32 * type_size_base(&base.type);
  
  if (!base.block) {
    c_error(
      node->index.left_bracket,
      "cannot index into uninitialised array '%h'",
      node->index.base);
    return false;
  }
  
  if (offset >= base.block->size) {
    c_error(node->index.left_bracket, "index out of bounds '%h'", node);
    return false;
  }
  
  *expr = base;
  expr->type.arr = false;
  expr->loc_base = base.block;
  expr->loc_offset = offset;
  mem_load(expr->loc_base, expr->loc_offset, &expr->type, expr);
  
  return true;
}

bool int_new(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  scope_t *class = scope_find_class(scope, node->new.class_ident->data.ident);
  
  if (!class) {
    c_error(
      node->new.class_ident,
      "undeclared class '%s'",
      node->new.class_ident->data.ident);
    return false;
  }
  
  fn_t *fn = scope_find_fn(class, "+new");
  if (!fn) {
    LOG_ERROR("class has no constructor");
    return false;
  }
  
  heap_block_t *heap_block = heap_alloc(class->size);
  
  expr->type.spec = SPEC_FN;
  expr->type.arr = false;
  expr->type.class = NULL;
  expr->fn = fn;
  expr->loc_base = heap_block;
  expr->loc_offset = 0;
  
  return true;
}

bool int_array_init(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  type_t type;
  if (!int_type(scope, &type, node->array_init.type))
    return false;
  
  if (node->array_init.init) {
    int size = 0;
    s_node_t *head = node->array_init.init;
    while (head) {
      size++;
      head = head->arg.next;
    }
    
    expr->type = type;
    expr->type.arr = true;
    expr->block = heap_alloc(size * type_size(&type));
    expr->loc_base = NULL;
    expr->loc_offset = 0;
    
    int num_arg = 0;
    head = node->array_init.init;
    while (head) {
      expr_t arg;
      if (!int_expr(scope, &arg, head->arg.body))
        return false;
      
      if (!type_cmp(&arg.type, &type)) {
        c_error(
          node->array_init.array_init,
          "incompatible types when initializing array type '%z' with '%z' at '%h'",
          &type,
          &arg.type,
          head->arg.body);
        return false;
      }
      
      mem_assign(expr->block, num_arg * type_size(&type), &type, &arg);
      
      num_arg++;
      head = head->arg.next;
    }
  } else if (node->array_init.size) {
    expr_t size;
    if (!int_expr(scope, &size, node->array_init.size))
      return false;
    
    if (!type_cmp(&size.type, &type_i32)) {
      c_error(
        node->array_init.array_init,
        "size of array has non-integer type");
      return false;
    }
    
    expr->type = type;
    expr->type.arr = true;
    expr->block = heap_alloc(size.i32 * type_size(&type));
    expr->loc_base = NULL;
    expr->loc_offset = 0;
  } else {
    LOG_ERROR("missing size or init");
    return false;
  }
  
  return true;
}

bool int_post_op(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t lhs;
  if (!int_expr(scope, &lhs, node->post_op.lhs))
    return false;
  
  expr->loc_base = NULL;
  expr->loc_offset = 0;
  
  if (node->post_op.op->token == TK_INC) {
    if (!expr_lvalue(&lhs)) {
      c_error(node->post_op.op, "lvalue required as left operand '%t'", node->post_op.op->token);
      return false;
    }
    
    *expr = lhs;
    expr->loc_base = NULL;
    expr->loc_offset = 0;
    
    if (type_cmp(&lhs.type, &type_i32))
      lhs.i32 = lhs.i32 + 1;
    else if (type_cmp(&lhs.type, &type_f32))
      lhs.f32 = lhs.f32 + 1.0;
    
    mem_assign(lhs.loc_base, lhs.loc_offset, &type_i32, &lhs);
  } else if (node->post_op.op->token == TK_DEC) {
    if (!expr_lvalue(&lhs)) {
      c_error(node->post_op.op, "lvalue required as left operand '%t'", node->post_op.op->token);
      return false;
    }
    
    *expr = lhs;
    expr->loc_base = NULL;
    expr->loc_offset = 0;
    
    if (type_cmp(&lhs.type, &type_i32))
      lhs.i32 = lhs.i32 - 1;
    else if (type_cmp(&lhs.type, &type_f32))
      lhs.f32 = lhs.f32 - 1.0;
    
    mem_assign(lhs.loc_base, lhs.loc_offset, &type_i32, &lhs);
  } else {
    c_error(
      node->post_op.op,
      "unknown postfix operand type for '%t': '%z'",
      node->post_op.op->token,
      &lhs.type);
    return false;
  }
  
  return true;
}

bool int_constant(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  var_t *var;
  switch (node->constant.lexeme->token) {
  case TK_CONST_INTEGER:
    expr_i32(expr, node->constant.lexeme->data.i32);
    break;
  case TK_CONST_FLOAT:
    expr_f32(expr, node->constant.lexeme->data.f32);
    break;
  case TK_STRING_LITERAL:
    expr->type = type_string;
    expr->block = heap_alloc_string(node->constant.lexeme->data.string_literal);
    expr->loc_base = NULL;
    expr->loc_offset = 0;
    break;
  case TK_IDENTIFIER:
    if (!int_load_ident(scope, stack_mem, expr, node->constant.lexeme)) {
      c_error(
        node->constant.lexeme,
        "'%s' undeclared",
        node->constant.lexeme->data.ident);
      return false;
    }
    break;
  default:
    LOG_ERROR("unknown token (%i)", node->constant.lexeme->token);
    break;
  }
  
  return true;
}
