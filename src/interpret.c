#include "interpret.h"

#include "log.h"

static bool int_stmt(scope_t *scope, const s_node_t *node);
static bool int_print(scope_t *scope, const s_node_t *node);
static bool int_if_stmt(scope_t *scope, const s_node_t *node);
static bool int_while_stmt(scope_t *scope, const s_node_t *node);
static bool int_decl(scope_t *scope, const s_node_t *node);
static bool int_class_def(scope_t *scope, const s_node_t *node);

static bool int_type(scope_t *scope, type_t *type, const s_node_t *node);

static bool int_expr(scope_t *scope, expr_t *expr, const s_node_t *node);
static bool int_index(scope_t *scope, expr_t *expr, const s_node_t *node);
static bool int_child(scope_t *scope, expr_t *expr, const s_node_t *node);
static bool int_binop(scope_t *scope, expr_t *expr, const s_node_t *node);
static bool int_constant(scope_t *scope, expr_t *expr, const s_node_t *node);

static char mem_stack[512];
static int  mem_ptr = 0;
static void mem_load(int loc, const type_t *type, expr_t *expr);
static void mem_assign(int loc, const type_t *type, expr_t *expr);

bool interpret(const s_node_t *node)
{
  scope_t scope;
  scope_new(&scope);
  scope.size += 4;
  
  int_stmt(&scope, node);
  
  return true;
}

bool int_stmt(scope_t *scope, const s_node_t *node)
{
  expr_t expr;
  const s_node_t *head = node;
  while (head) {
    switch (head->stmt.body->node_type) {
    case S_BINOP:
    case S_CONSTANT:
    case S_INDEX:
    case S_CHILD:
      if (!int_expr(scope, &expr, head->stmt.body))
        return false;
      break;
    case S_DECL:
      if (!int_decl(scope, head->stmt.body))
        return false;
      break;
    case S_CLASS_DEF:
      if (!int_class_def(scope, head->stmt.body))
        return false;
      break;
    case S_PRINT:
      if (!int_print(scope, head->stmt.body))
        return false;
      break;
    case S_IF_STMT:
      if (!int_if_stmt(scope, head->stmt.body))
        return false;
      break;
    case S_WHILE_STMT:
      if (!int_while_stmt(scope, head->stmt.body))
        return false;
      break;
    }
    head = head->stmt.next;
  }
  
  return true;
}

bool int_while_stmt(scope_t *scope, const s_node_t *node)
{
  expr_t cond;
  if (!int_expr(scope, &cond, node->while_stmt.cond))
    return false;
  
  while (cond.i32 != 0) {
    if (!int_stmt(scope, node->while_stmt.body))
      return false;
    
    if (!int_expr(scope, &cond, node->while_stmt.cond))
      return false;
  }
  
  return true;
}

bool int_if_stmt(scope_t *scope, const s_node_t *node)
{
  expr_t cond;
  if (!int_expr(scope, &cond, node->if_stmt.cond))
    return false;
  
  if (cond.i32 != 0) {
    if (!int_stmt(scope, node->if_stmt.body))
      return false;
  }
  
  return true;
}

bool int_print(scope_t *scope, const s_node_t *node)
{
  expr_t expr;
  if (!int_expr(scope, &expr, node->print.body))
    return false;
  
  c_debug("out: %w", &expr);
  
  return true;
}

bool int_class_def(scope_t *scope, const s_node_t *node)
{
  if (scope_find_class(scope, node->class_def.ident->data.ident)) {
    c_error(node->decl.ident, "redefinition of class '%s'", node->class_def.ident->data.ident);
    return false;
  }
  
  scope_t class_scope;
  scope_new(&class_scope);
  
  s_node_t *head = node->class_def.class_decl;
  
  while (head) {
    type_t type;
    if (!int_type(scope, &type, head->class_decl.type))
      return false;
    
    if (scope_find_var(&class_scope, head->class_decl.ident->data.ident)) {
      c_error(
        node->class_def.ident,
        "redefinition of '%s' in class '%s'",
        head->class_decl.ident->data.ident,
        node->class_def.ident->data.ident);
      return false;
    }
    
    scope_add_var(&class_scope, &type, head->class_decl.ident->data.ident);
    head = head->class_decl.next;
  }
  
  scope_add_class(scope, node->class_def.ident->data.ident, &class_scope);
  
  return true;
}

bool int_decl(scope_t *scope, const s_node_t *node)
{
  if (scope_find_var(scope, node->decl.ident->data.ident)) {
    c_error(node->decl.ident, "redefinition of '%s'", node->decl.ident->data.ident);
    return false;
  }
  
  type_t type;
  if (!int_type(scope, &type, node->decl.type))
    return false;
  
  var_t *var = scope_add_var(scope, &type, node->decl.ident->data.ident);
  if (node->decl.init) {
    expr_t expr;
    
    if (!int_expr(scope, &expr, node->decl.init))
      return false;
    
    if (!type_cmp(&type, &expr.type)) {
      c_error(
        node->decl.type->type.spec,
        "incompatible types when initializing '%z' with '%z'",
        &type, &expr.type);
      return false;
    }
    
    mem_assign(var->loc, &var->type, &expr);
  }
  
  
  return true;
}

bool int_type(scope_t *scope, type_t *type, const s_node_t *node)
{
  type->class = NULL;
  switch (node->type.spec->token) {
  case TK_I32:
    type->spec = SPEC_I32;
    break;
  case TK_F32:
    type->spec = SPEC_F32;
    break;
  case TK_CLASS:
    type->spec = SPEC_CLASS;
    
    type->class = scope_find_class(scope, node->type.class_ident->data.ident);
    if (!type->class) {
      c_error(
        node->type.class_ident,
        "use of undefined class '%s'",
        node->type.class_ident->data.ident);
      return false;
    }
    
    break;
  }
  
  if (node->type.size) {
    expr_t size;
    
    if (!int_expr(scope, &size, node->type.size))
      return false;
    
    if (!type_cmp(&size.type, &type_i32)) {
      c_error(node->type.spec, "size of array has non-integer type");
      return false;
    }
    
    type->size = size.i32;
  } else {
    type->size = 0;
  }
  
  return true;
}

bool int_expr(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  switch (node->node_type) {
  case S_BINOP:
    return int_binop(scope, expr, node);
  case S_INDEX:
    return int_index(scope, expr, node);
  case S_CHILD:
    return int_child(scope, expr, node);
  case S_CONSTANT:
    return int_constant(scope, expr, node);
  }
}

bool int_binop(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t lhs;
  if (!int_expr(scope, &lhs, node->binop.lhs))
    return false;
  
  expr_t rhs;
  if (!int_expr(scope, &rhs, node->binop.rhs))
    return false;
  
  expr->loc = 0;
  if (type_cmp(&lhs.type, &type_i32) && type_cmp(&rhs.type, &type_i32)) {
    expr->type = type_i32;
    switch (node->binop.op->token) {
    case '+':
      expr->i32 = lhs.i32 + rhs.i32;
      break;
    case '-':
      expr->i32 = lhs.i32 - rhs.i32;
      break;
    case '*':
      expr->i32 = lhs.i32 * rhs.i32;
      break;
    case '/':
      expr->i32 = lhs.i32 / rhs.i32;
      break;
    case '<':
      expr->i32 = lhs.i32 < rhs.i32;
      break;
    case '>':
      expr->i32 = lhs.i32 > rhs.i32;
      break;
    case '=':
      if (!lhs.loc) {
        c_error(node->binop.op, "lvalue required as left operand of assignment");
        return false;
      }
      
      mem_assign(lhs.loc, &type_i32, &rhs);
      *expr = lhs;
      expr->i32 = rhs.i32;
      
      break;
    default:
      goto err_no_op; // lol lmao
    }
  } else if (type_cmp(&lhs.type, &type_f32) && type_cmp(&rhs.type, &type_f32)) {
    expr->type = type_f32;
    switch (node->binop.op->token) {
    case '+':
      expr->f32 = lhs.f32 + rhs.f32;
      break;
    case '-':
      expr->f32 = lhs.f32 - rhs.f32;
      break;
    case '*':
      expr->f32 = lhs.f32 * rhs.f32;
      break;
    case '/':
      expr->f32 = lhs.f32 / rhs.f32;
      break;
    case '<':
      expr->type = type_i32;
      expr->i32 = lhs.f32 < rhs.f32;
      break;
    case '>':
      expr->type = type_i32;
      expr->i32 = lhs.f32 > rhs.f32;
      break;
    case '=':
      if (!lhs.loc) {
        c_error(node->binop.op, "lvalue required as left operand of assignment");
        return false;
      }
      
      mem_assign(lhs.loc, &type_f32, &rhs);
      *expr = lhs;
      expr->f32 = rhs.f32;
      
      break;
    default:
      goto err_no_op; // lol lmao
    }
  } else {
err_no_op: // i actually used a goto lol
    c_error(
      node->binop.op,
      "unknown operand type for '%t': '%z' and '%z'",
      node->binop.op->token,
      &lhs.type, &rhs.type);
    return false;
  }
  
  return true;
}

bool int_child(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t base;
  if (!int_expr(scope, &base, node->child.base))
    return false;
  
  if (base.type.spec != SPEC_CLASS || base.type.size > 0) {
    c_error(
      node->child.child_ident,
      "request for member '%s' in something non-class",
      node->child.child_ident->data.ident);
    return false;
  }
  
  var_t *var = scope_find_var(&base.type.class->scope, node->child.child_ident->data.ident);
  if (!var) {
    c_error(
      node->child.child_ident,
      "'class %s' has no member named '%s'",
      base.type.class->ident,
      node->child.child_ident->data.ident);
    return false;
  }
  
  expr->type = var->type;
  expr->loc = base.loc + var->loc;
  mem_load(expr->loc, &expr->type, expr);
  
  return true;
}

bool int_index(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  expr_t base;
  if (!int_expr(scope, &base, node->index.base))
    return false;
  
  if (!type_array(&base.type)) {
    c_error(node->index.left_bracket, "subscripted value is not an array");
    return false;
  }
  
  expr_t index;
  if (!int_expr(scope, &index, node->index.index))
    return false;
  
  if (!type_cmp(&index.type, &type_i32)) {
    c_error(node->index.left_bracket, "array subscript is not an integer");
    return false;
  }
  
  *expr = base;
  expr->type.size = 0;
  expr->loc += index.i32 * type_size_base(&base.type);
  mem_load(expr->loc, &expr->type, expr);
  
  return true;
}

bool int_constant(scope_t *scope, expr_t *expr, const s_node_t *node)
{
  var_t *var;
  switch (node->constant.lexeme->token) {
  case TK_CONST_INTEGER:
    expr->type = type_i32;
    expr->i32 = node->constant.lexeme->data.i32;
    expr->loc = 0;
    break;
  case TK_CONST_FLOAT:
    expr->type = type_f32;
    expr->f32 = node->constant.lexeme->data.f32;
    expr->loc = 0;
    break;
  case TK_IDENTIFIER:
    var = scope_find_var(scope, node->constant.lexeme->data.ident);
    
    if (!var) {
      c_error(
        node->constant.lexeme,
        "'%s' undeclared",
        node->constant.lexeme->data.ident);
      return false;
    }
    
    mem_load(var->loc, &var->type, expr);
    break;
  }
  
  return true;
}

static void mem_load(int loc, const type_t *type, expr_t *expr)
{
  expr->loc = loc;
  expr->type = *type;
  
  if (type_cmp(type, &type_i32))
    expr->i32 = *((int*) &mem_stack[loc]);
  else if (type_cmp(type, &type_f32))
    expr->f32 = *((float*) &mem_stack[loc]);
}

static void mem_assign(int loc, const type_t *type, expr_t *expr)
{
  if (type_cmp(type, &type_i32))
    *((int*) &mem_stack[loc]) = expr->i32;
  else if (type_cmp(type, &type_f32))
    *((float*) &mem_stack[loc]) = expr->f32;
}
