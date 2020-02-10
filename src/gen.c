#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "gen.h"

/* helper function for determine_local_indices.  recursively determines the local index
 * of function arguments, as well as local variables inside of blocks. 
 * returns the total number of stack space needed for a give node.  for example, when
 * called on a NODE_FUNCTION, returns the size of the stack that should be allocated
 * when the procedure is called */
static size_t assign_local_indices(GenerateState_T *G, ASTNode_T *node, size_t start) {
  ASTNode_T *next = node->next;
  size_t local_index = start;
  if (node->type == NODE_FUNCTION) {
    for (Declaration_T *arg = node->nodefunc->args; arg != NULL; arg = arg->next) {
      printf("assign %s %zu\n", arg->name, local_index);
      arg->local_index = local_index;
      local_index += 8; /* TODO carefully evaluate */
    }
    if (next != NULL && next->type == NODE_BLOCK) {
      node->nodefunc->stack_space = assign_local_indices(G, next, local_index);
      printf("function %s needs stack space %zu\n",
             node->nodefunc->func_name, node->nodefunc->stack_space);
    }
  } else if (node->type == NODE_BLOCK) {
    for (Declaration_T *var = node->nodeblock->vars; var != NULL; var = var->next) {
      printf("assign %s %zu\n", var->name, local_index);
      var->local_index = local_index;
      local_index += 8;
    }
    for (ASTNode_T *c = node->nodeblock->children; c != NULL; c = c->next) {
      if (c->type == NODE_BLOCK) {
        assign_local_indices(G, c, local_index);
      }
    }
  }
  return local_index;
}

/* assign local index values to each local variable in the syntax tree.
 * the local_index value is a variable's index relative to the base pointer.
 * see: assign_local_indices() */
static void determine_local_indices(GenerateState_T *G) {
  ASTNode_T *root = G->P->root;
  NodeBlock_T *rootb = root->nodeblock;
  for (ASTNode_T *a = rootb->children; a != NULL; a = a->next) {
    if (a->type == NODE_FUNCTION) {
      assign_local_indices(G, a, 0);
    }
  }
}

GenerateState_T *gen_init(ParseState_T *P) {
  GenerateState_T *G = malloc(sizeof(GenerateState_T));
  assert(G);
  G->P = P;
  return G;
}

void generate_bytecode(ParseState_T *P) {
  GenerateState_T *G = gen_init(P);
  determine_local_indices(G);
}
