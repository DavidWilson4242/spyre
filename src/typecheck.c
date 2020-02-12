#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "typecheck.h"

/* forward decls */
static void destroy_datatype(Datatype_T **);
static Datatype_T *deepcopy_datatype(Datatype_T *);
static Declaration_T *deepcopy_decl(Declaration_T *);
static void typecheck_node(ParseState_T *P, ASTNode_T *);
static void typecheck_expression(ParseState_T *, NodeExpression_T *);

static void typecheck_exp_err(NodeExpression_T *node, const char *fmt, ...) {
  va_list varargs;
  va_start(varargs, fmt);

  fprintf(stderr, "Spyre Typecheck error:\n");
  fprintf(stderr, "\tmessage: ");
  vfprintf(stderr, fmt, varargs);
  fprintf(stderr, "\n\tline:    %zu\n", node ? node->lineno : 0);

  va_end(varargs);

  exit(EXIT_FAILURE);
}

static const char *operator_tostring(uint8_t op) {
  return "OP";
}

static Declaration_T *search_in_decl_list(Declaration_T *decl, const char *name) {
  while (decl != NULL) {
    if (!strcmp(decl->name, name)) {
      return decl;
    }
    decl = decl->next;
  }
  return NULL;
}


/* propogate upwards in the syntax, starting at the nearest block, looking
 * for variables */
static Declaration_T *decl_from_identifier(ParseState_T *P, NodeExpression_T *node) {

  /* find the top level node, so we can find the node we're inside of */
  ASTNode_T *inside;
  NodeExpression_T *parentscan = node;
  while (parentscan->nodeparent == NULL) {
    parentscan = parentscan->parent;
  }
  inside = parentscan->nodeparent;
  
  /* SPECIAL CASE:
   * used for short-return functions */
  /*
  prev = P->backnode;
  if (prev != NULL && prev->type == NODE_FUNCTION) {
    search = search_in_decl_list(prev->nodefunc->args, name);
    if (search != NULL) {
      return search;
    }
  }
  */

  Declaration_T *search = NULL;

  while (inside != NULL) {
    if (inside->type == NODE_BLOCK) {
      search = search_in_decl_list(inside->nodeblock->vars, node->identval);
    } 
    if (search == NULL && inside->prev != NULL && inside->prev->type == NODE_FUNCTION) {
      search = search_in_decl_list(inside->prev->nodefunc->args, node->identval);
    }
    if (search != NULL) {
      return search;
    }
    inside = inside->parent;
  }

  return NULL;
}


/* returns true if and only if the two datatypes are exactly the same, meaning
 * they must have the same typename, arrdim, ptrdim and qualifiers */
static bool compare_datatypes_strict(Datatype_T *a, Datatype_T *b) {
  return (
    (a->type == b->type)
    && (a->ptrdim == b->ptrdim)
    && (a->arrdim == b->ptrdim)
    && (a->is_const == b->is_const)
    && !strcmp(a->type_name, b->type_name)
  );
}

/* expects only primitive types, such as int, float, bool, etc */
static Datatype_T *make_raw_datatype(const char *type_name) {
  Datatype_T *dt = malloc(sizeof(Datatype_T));
  assert(dt);
  dt->type = DT_PRIMITIVE;
  dt->next = NULL;
  dt->arrdim = 0;
  dt->ptrdim = 0;
  dt->primsize = 8;
  dt->is_const = false;
  dt->type_name = malloc(strlen(type_name) + 1);
  assert(dt->type_name);
  strcpy(dt->type_name, type_name);
  return dt;
}

/* map function for destroy_datatype.  used to destroy
 * the hashtable containing a struct's members */
static void destroy_struct_members(const char *key, void *member, void *cl) {
  Declaration_T *decl = member;  
  free(decl->name);
  destroy_datatype(&decl->dt);
  free(decl);
}

static void destroy_datatype(Datatype_T **dtp) {
  Datatype_T *dt = *dtp;
  free(dt->type_name);

  /* recursively destroy children? */
  switch (dt->type) {
    case DT_STRUCT:
      hash_foreach(dt->sdesc->members, destroy_struct_members, NULL);
      hash_free(&dt->sdesc->members);
      free(dt->sdesc);
      break;
    case DT_FUNCTION:
      break;
    default:
      break;
  }

  free(dt);
  *dtp = NULL;
}

/* map function for deepcopy_datatype.  used to clone
 * a struct's members into a newly created hashtable
 * closure argument is the new struct's member table */
static void deepcopy_struct_members(const char *key, void *member, void *cl) {
  Declaration_T *decl = member;
  SpyreHash_T *newtable = cl;
  hash_insert(newtable, key, deepcopy_decl(decl)); 
}

static Declaration_T *deepcopy_decl(Declaration_T *decl) {
  Declaration_T *clone = malloc(sizeof(Declaration_T));
  assert(clone);
  clone->name = malloc(strlen(decl->name) + 1);
  strcpy(clone->name, decl->name);
  clone->dt = deepcopy_datatype(decl->dt);
  clone->local_index = decl->local_index;
  return clone;
}

static Datatype_T *deepcopy_datatype(Datatype_T *dt) {
  Datatype_T *clone = malloc(sizeof(Datatype_T));
  assert(clone);
  clone->type_name = malloc(strlen(dt->type_name) + 1);
  strcpy(clone->type_name, dt->type_name);
  clone->arrdim = dt->arrdim;
  clone->ptrdim = dt->ptrdim;
  clone->primsize = dt->primsize;
  clone->is_const = dt->is_const;
  clone->type = dt->type;
  
  /* struct or function that requires a deeper copy? */
  switch (dt->type) {
    case DT_STRUCT:
      clone->sdesc = malloc(sizeof(StructDescriptor_T));
      assert(clone->sdesc);
      clone->sdesc->members = hash_init();
      hash_foreach(dt->sdesc->members, deepcopy_struct_members, clone->sdesc->members);
      break;
    case DT_FUNCTION:
      break;
    default: 
      clone->sdesc = NULL;
      break;
  }

  return clone;
}

static void typecheck_binary_operator(ParseState_T *P, NodeExpression_T *exp,
                                      NodeExpression_T *left, NodeExpression_T *right) {
  /* TODO implicit casting, etc. */
  Declaration_T *member;

  switch (exp->binop->optype) {
    
    /* struct member operator */
    case '.':
      typecheck_expression(P, left);
      if (left->resolved->type != DT_STRUCT) {
        typecheck_exp_err(exp, "expected struct as left-operand to operator '.' (got type '%s')",
                               left->resolved->type_name);
      }
      if (right->type != EXP_IDENTIFIER) {
        typecheck_exp_err(exp, "expected identifier as right-operand to operator '.'");
      }
      right->resolved = NULL;
      member = hash_get(left->resolved->sdesc->members, right->identval);
      if (member == NULL) {
        typecheck_exp_err(exp, "'%s' is not a valid member of struct '%s'",
                          right->identval, left->resolved->type_name);
      }
      exp->resolved = deepcopy_datatype(member->dt);
      break;
    
    /* comparison operators */
    case SPECO_EQ:
    case SPECO_NEQ:
    case SPECO_GE:
    case SPECO_LE:
    case '<':
    case '>':
      typecheck_expression(P, left);
      typecheck_expression(P, right);
      if (!compare_datatypes_strict(left->resolved, right->resolved)) {
        typecheck_exp_err(exp, "operands to comparison operator '%s' do not match (got types %s and %s)",
                          exp->binop->as_string, left->resolved->type_name, right->resolved->type_name);
      }
      exp->resolved = make_raw_datatype("bool");
      break;

    /* logical operators */
    case SPECO_LOG_AND:
    case SPECO_LOG_OR:
      typecheck_expression(P, left);
      typecheck_expression(P, right);
      if (!compare_datatypes_strict(left->resolved, P->builtin->bool_t)
          || !compare_datatypes_strict(right->resolved, P->builtin->bool_t)) {
        typecheck_exp_err(exp, "operands to logical operator '%s' must be of type 'bool' (got types %s and %s)",
                          exp->binop->as_string, left->resolved->type_name, right->resolved->type_name);
      }
      exp->resolved = make_raw_datatype("bool");
      break;

    default:
      typecheck_expression(P, left);
      typecheck_expression(P, right);
      if (!compare_datatypes_strict(left->resolved, right->resolved)) {
        typecheck_exp_err(exp, "operands to operator '%s' do not match (got types %s and %s)",
                          exp->binop->as_string, left->resolved->type_name, right->resolved->type_name);
      }
      exp->resolved = deepcopy_datatype(left->resolved);
      break;
  }
}

static void typecheck_identifier(ParseState_T *P, NodeExpression_T *exp) {
  Declaration_T *decl = decl_from_identifier(P, exp); 
  if (decl == NULL) {
    typecheck_exp_err(exp, "unknown identifier '%s'", exp->identval);
  }
  exp->resolved = deepcopy_datatype(decl->dt);
}

static void typecheck_expression(ParseState_T *P, NodeExpression_T *exp) {
  Declaration_T *decl;
  switch (exp->type) {
    case EXP_BINARY:
      return typecheck_binary_operator(P, exp, exp->binop->left_operand, exp->binop->right_operand);    
    case EXP_UNARY:
      break;
    case EXP_INTEGER:
      exp->resolved = make_raw_datatype("int");
      break;
    case EXP_FLOAT:
      exp->resolved = make_raw_datatype("float");
      break;
    case EXP_IDENTIFIER:
      typecheck_identifier(P, exp);
      break;
    default: 
      break;
  }
}

void typecheck_block_members(ParseState_T *P, ASTNode_T *block) {
  NodeBlock_T *nodeblock = block->nodeblock;
  for (ASTNode_T *c = nodeblock->children; c != NULL; c = c->next) {
    typecheck_node(P, c);    
  }
}

void typecheck_if(ParseState_T *P, ASTNode_T *node) {
  typecheck_expression(P, node->nodeif->cond);
  if (!compare_datatypes_strict(node->nodeif->cond->resolved, P->builtin->bool_t)) {
    typecheck_exp_err(node->nodeif->cond, "if-condition must evaluate to type 'bool' (got type '%s')", 
                      node->nodeif->cond->resolved->type_name);
  }
}

void typecheck_while(ParseState_T *P, ASTNode_T *node) {
  typecheck_expression(P, node->nodewhile->cond);
  if (!compare_datatypes_strict(node->nodewhile->cond->resolved, P->builtin->bool_t)) {
    typecheck_exp_err(node->nodewhile->cond, "while-condition must evaluate to type 'bool' (got type '%s')", 
                      node->nodewhile->cond->resolved->type_name);
  }
}

void typecheck_node(ParseState_T *P, ASTNode_T *node) {
  switch (node->type) {
    case NODE_BLOCK:
      typecheck_block_members(P, node);
      break;
    case NODE_EXPRESSION:
      typecheck_expression(P, node->nodeexp);
      break;
    case NODE_IF:
      typecheck_if(P, node);
      break;
    case NODE_WHILE:
      typecheck_while(P, node);
      break;
    default:
      break;
  }
}

void typecheck_syntax_tree(ParseState_T *P) {
  typecheck_node(P, P->root);
}
