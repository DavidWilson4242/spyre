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

Datatype_T *typecheck_expression(ParseState_T *P, NodeExpression_T *exp) {
  Datatype_T *left, *right, *clone;
  switch (exp->type) {
    case EXP_BINARY:
      /* TODO implicit casting, etc. */
      left  = typecheck_expression(P, exp->binop->left_operand);
      right = typecheck_expression(P, exp->binop->right_operand);
      if (!compare_datatypes_strict(left, right)) {
        const char *op = operator_tostring(exp->binop->optype); 
        typecheck_exp_err(exp, "operands to operator do not match");
      }
      exp->binop->left_operand->resolved = left;
      exp->binop->right_operand->resolved = right;
      return clone;
    case EXP_UNARY:
      break;
    case EXP_INTEGER:
      return make_raw_datatype("int");
    case EXP_FLOAT:
      return make_raw_datatype("float");
    default: 
      return NULL;
      break;
  }
}

void typecheck_block_members(ParseState_T *P, ASTNode_T *block) {
  NodeBlock_T *nodeblock = block->nodeblock;
  for (ASTNode_T *c = nodeblock->children; c != NULL; c = c->next) {
    typecheck_node(P, c);    
  }
}

void typecheck_node(ParseState_T *P, ASTNode_T *node) {
  Datatype_T *ret;
  switch (node->type) {
    case NODE_BLOCK:
      typecheck_block_members(P, node);
      break;
    case NODE_EXPRESSION:
      ret = typecheck_expression(P, node->nodeexp);
      destroy_datatype(&ret); 
      break;
    case NODE_IF:
      ret = typecheck_expression(P, node->nodeif->cond);
      destroy_datatype(&ret);
      break;
    case NODE_WHILE:
      ret = typecheck_expression(P, node->nodewhile->cond);
      destroy_datatype(&ret);
      break;
    default:
      break;
  }
}

void typecheck_syntax_tree(ParseState_T *P) {
  typecheck_node(P, P->root);
}
