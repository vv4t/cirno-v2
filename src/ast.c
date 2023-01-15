#include "ast.h"

#include "log.h"
#include <stdlib.h>

static ast_expr_t *ast_expr(scope_t *scope, const s_node_t *node);
static ast_expr_t *ast_constant(scope_t *scope, const s_node_t *node);
static ast_expr_t *ast_binop(scope_t *scope, const s_node_t *node);

static ast_expr_t *ast_decl(scope_t *scope, const s_node_t *node);
static bool ast_type(scope_t *scope, type_t *type, const s_node_t *node);

static ast_stmt_t *ast_stmt(scope_t *scope, const s_node_t *node);

static ast_expr_t *make_ast_expr(const ast_expr_t *expr);
static ast_stmt_t *make_ast_stmt(const ast_stmt_t *stmt);

ast_stmt_t *ast_parse(const s_node_t *node)
{
  if (!node)
    return NULL;
  
  scope_t scope;
  scope_new(&scope);
  
  ast_stmt_t *stmt_body = ast_stmt(&scope, node);
  ast_stmt_t *stmt_head = stmt_body;
  
  const s_node_t *head = node->stmt.next;
  while (head) {
    ast_stmt_t *stmt = ast_stmt(&scope, head);
    if (stmt) {
      if (stmt_body)
        stmt_head = stmt_head->next = stmt;
      else
        stmt_body = stmt_head = stmt;
    }
    
    head = head->stmt.next;
  }
  
  return stmt_body;
}

static ast_stmt_t *ast_stmt(scope_t *scope, const s_node_t *node)
{
  ast_stmt_t stmt = {0};
  switch (node->stmt.body->node_type) {
  case S_BINOP:
  case S_CONSTANT:
    stmt.ast_stmt_type = AST_STMT_EXPR;
    stmt.expr = ast_expr(scope, node->stmt.body);
    break;
  case S_DECL:
    stmt.ast_stmt_type = AST_STMT_EXPR;
    stmt.expr = ast_decl(scope, node->stmt.body);
    if (!stmt.expr)
      return NULL;
  default:
    LOG_ERROR("unknown");
    return NULL;
  }
  
  return make_ast_stmt(&stmt);
}

static ast_expr_t *ast_decl(scope_t *scope, const s_node_t *node)
{
  if (scope_find_var(scope, node->decl.ident->data.ident)) {
    c_error(node->decl.ident, "redefinition of '%s'", node->decl.ident->data.ident);
    return false;
  }
  
  type_t type = {0};
  if (!ast_type(scope, &type, node->decl.type))
    return false;
  
  scope_add_var(scope, &type, node->decl.ident->data.ident);
  
  return NULL;
}

bool ast_type(scope_t *scope, type_t *type, const s_node_t *node)
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
  
  type->is_ptr = node->type.ptr != NULL;
  
  /*
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
  }*/
  
  return true;
}

static ast_expr_t *ast_expr(scope_t *scope, const s_node_t *node)
{
  switch (node->node_type) {
  case S_BINOP:
    return ast_binop(scope, node);
  case S_CONSTANT:
    return ast_constant(scope, node);
  }
}

static ast_expr_t *ast_binop(scope_t *scope, const s_node_t *node)
{
  ast_expr_t expr = {0};
  expr.ast_expr_type = AST_EXPR_BINOP;
  
  expr.binop.lhs = ast_expr(scope, node->binop.lhs);
  if (!expr.binop.lhs)
    return NULL;
  
  expr.binop.rhs = ast_expr(scope, node->binop.rhs);
  if (!expr.binop.rhs)
    return NULL;
  
  if (type_cmp(&expr.binop.lhs->type, &type_i32) && type_cmp(&expr.binop.rhs->type, &type_i32)) {
    expr.type = type_i32;
    switch (node->binop.op->token) {
    case '+':
      expr.binop.op = AST_OP_ADD_I32;
      break;
    case '-':
      expr.binop.op = AST_OP_SUB_I32;
      break;
    case '*':
      expr.binop.op = AST_OP_MUL_I32;
      break;
    case '/':
      expr.binop.op = AST_OP_DIV_I32;
      break;
    case '=':
      expr.binop.op = AST_OP_STORE_32;
      break;
    default:
      goto err_no_op; // lol lmao
    }
  } else if (type_cmp(&expr.binop.lhs->type, &type_f32) && type_cmp(&expr.binop.rhs->type, &type_f32)) {
    expr.type = type_f32;
    switch (node->binop.op->token) {
    case '+':
      expr.binop.op = AST_OP_ADD_F32;
      break;
    case '-':
      expr.binop.op = AST_OP_SUB_F32;
      break;
    case '*':
      expr.binop.op = AST_OP_MUL_F32;
      break;
    case '/':
      expr.binop.op = AST_OP_DIV_F32;
      break;
    case '=':
      expr.binop.op = AST_OP_STORE_32;
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
      &expr.binop.lhs->type, &expr.binop.rhs->type);
    return false;
  }
  
  return make_ast_expr(&expr);
}

static ast_expr_t *ast_constant(scope_t *scope, const s_node_t *node)
{
  ast_expr_t expr = {0};
  switch (node->constant.lexeme->token) {
  case TK_CONST_INTEGER:
    expr.type = type_i32;
    expr.ast_expr_type = AST_EXPR_I32;
    expr.i32 = node->constant.lexeme->data.i32;
    break;
  case TK_CONST_FLOAT:
    expr.type = type_f32;
    expr.ast_expr_type = AST_EXPR_F32;
    expr.f32 = node->constant.lexeme->data.f32;
    break;
  default:
    return NULL;
  }
  
  return make_ast_expr(&expr);
}

static ast_stmt_t *make_ast_stmt(const ast_stmt_t *stmt)
{
  ast_stmt_t *new_stmt = malloc(sizeof(ast_expr_t));
  *new_stmt = *stmt;
  return new_stmt;
}

static ast_expr_t *make_ast_expr(const ast_expr_t *expr)
{
  ast_expr_t *new_expr = malloc(sizeof(ast_expr_t));
  *new_expr = *expr;
  return new_expr;
}
