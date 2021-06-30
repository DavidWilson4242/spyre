#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "gen.h"

/* syntax generation */
static void generate_function(GenerateState_T *, ASTNode_T **);
static void generate_block(GenerateState_T *, ASTNode_T **);
static void generate_if(GenerateState_T *, ASTNode_T **);

/* expression generation */
static void generate_expression(GenerateState_T *G, NodeExpression_T *);
static void generate_binary_expression(GenerateState_T *G, BinaryOpNode_T *);
static void generate_unary_expression(GenerateState_T *G, UnaryOpNode_T *);
static void generate_integer_expression(GenerateState_T *G, int64_t); 

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

/* attempts to find a local variable in the current generation context */
static Declaration_T *get_local(ASTNode_T *ast, const char *ident) {
  
  switch (ast->type) {
    case NODE_FUNCTION: {
      Declaration_T *arg = ast->nodefunc->args;
      for (; arg != NULL; arg = arg->next) {
	if (!strcmp(arg->name, ident)) {
	  return arg;
	}
      }
      break;
    }
    case NODE_BLOCK: {
      Declaration_T *decl = ast->nodeblock->vars;
      for (; decl != NULL; decl = decl->next) {
	if (!strcmp(decl->name, ident)) {
	  return decl;
	}
      }
      break;
    }
    default:
      break;
  }

  if (ast->next) {
    return get_local(ast->next, ident);
  }

  if (ast->type == NODE_BLOCK) {
    return get_local(ast->nodeblock->children, ident);
  }

  return NULL;

}

static void write_s(GenerateState_T *G, const char *s) {
  fprintf(G->outfile, "%s", s);
}

static void write_int(GenerateState_T *G, size_t n) {
  fprintf(G->outfile, "%zu", n);
}

static void write_label(GenerateState_T *G, size_t label_index) {
  fprintf(G->outfile, "__L%zu:", label_index);
}

static void write_label_ref(GenerateState_T *G, size_t label_index) {
  fprintf(G->outfile, "__L%zu", label_index);
}

static void generate_function(GenerateState_T *G, ASTNode_T **funcp) {
  ASTNode_T *func = *funcp;
  ASTNode_T **next = &func->next;
  write_s(G, func->nodefunc->func_name); 
  write_s(G, ":\nRESL ");
  write_int(G, func->nodefunc->stack_space);
  write_s(G, "\n");
  generate_block(G, next);
  write_s(G, "RET\n");
  *funcp = (*funcp)->next;
}

static void generate_if(GenerateState_T *G, ASTNode_T **ifp) {
  size_t poslbl = G->lcount++;
  size_t neglbl = G->lcount++;
  ASTNode_T *ifnode = *ifp;
  ASTNode_T **next = &ifnode->next;
  generate_expression(G, ifnode->nodeif->cond);
  write_s(G, "ITEST\n");
  write_s(G, "JZ ");
  write_label_ref(G, neglbl);
  write_s(G, "\n");
  generate_block(G, next);
  write_label(G, neglbl);
  write_s(G, "\n");
  *ifp = (*ifp)->next;
}

static void generate_integer_expression(GenerateState_T *G, int64_t value) {
  write_s(G, "IPUSH ");
  write_int(G, value);
  write_s(G, "\n");
}

/* assumes exp is of type EXP_UNARY */
static void generate_unary_expression(GenerateState_T *G, UnaryOpNode_T *exp) {
  generate_expression(G, exp->operand);
}

/* assumes exp is of type EXP_BINARY */
static void generate_binary_expression(GenerateState_T *G, BinaryOpNode_T *exp) {
  generate_expression(G, exp->left_operand);
  generate_expression(G, exp->right_operand);
  switch (exp->optype) {
    case '+':
      write_s(G, "IADD\n");
      break;
    case '-':
      write_s(G, "ISUB\n");
      break;
    case '*':
      write_s(G, "IMUL\n");
      break;
    case '/':
      write_s(G, "IDIV\n");
      break;
    case '=':
      write_s(G, "DUP\n");
      break;
    default:
      break;
  }
}

/* assumes exp is of type EXP_IDENTIFIER */
static void generate_identifier_expression(GenerateState_T *G, NodeExpression_T *exp) {
  ASTNode_T *at = G->at;

  bool is_assign = (exp->parent &&
                    exp->parent->type == EXP_BINARY &&
		    exp->parent->binop->optype == '=');
  bool dont_der = false;

  /* if it's an identifier on the LHS of =, load address */
  dont_der = exp->leaf == LEAF_LEFT && is_assign; 

  /* is it a local? */
  Declaration_T *decl = get_local(G->at, exp->identval);


  if (decl && dont_der) {
    write_s(G, "LEA ");
    write_int(G, decl->local_index); 
  } else if (decl && !dont_der) {
    write_s(G, "LDL "); 
    write_int(G, decl->local_index);
  }

}

static void generate_expression(GenerateState_T *G, NodeExpression_T *exp) {
  
  switch (exp->type) {
    case EXP_UNARY:
      generate_unary_expression(G, exp->unop);
      break;
    case EXP_BINARY:
      generate_binary_expression(G, exp->binop); 
      break;
    case EXP_INTEGER:
      generate_integer_expression(G, exp->ival);
      break;
    case EXP_INDEX:
      break;
    case EXP_CALL:
      break;
    case EXP_FLOAT:
      break;
    case EXP_IDENTIFIER:
      generate_identifier_expression(G, exp); 
      break;
    case EXP_NEW:
      break;
    default:
      break;
  }

}

static void generate_block(GenerateState_T *G, ASTNode_T **blockp) {

  ASTNode_T *block = *blockp;

  for (ASTNode_T *c = block->nodeblock->children; c != NULL; c = c->next) {
    switch (c->type) {
      case NODE_FUNCTION:
        generate_function(G, &c);
        break;
      case NODE_BLOCK:
        generate_block(G, &c);
        break;
      case NODE_IF:
        generate_if(G, &c);
        break;
      case NODE_EXPRESSION:
        generate_expression(G, c->nodeexp);
        break;
      case NODE_RETURN:
	break;
      case NODE_CONTINUE:
	break;
      case NODE_INCLUDE:
	break;
      case NODE_DECLARATION:
	break;
      default:
        break;
    }

    if (c == NULL) {
      break;
    }
  }
}

GenerateState_T *gen_init(ParseState_T *P, char *outfile) {
  GenerateState_T *G = malloc(sizeof(GenerateState_T));
  assert(G);
  G->P = P;
  G->at = P->root;
  G->lcount = 0;
  G->outfile = fopen(outfile, "wb");
  if (G->outfile == NULL) {
    fprintf(stderr, "couldn't open '%s' for reading.\n", outfile);
    exit(EXIT_FAILURE);
  }
  return G;
}

void generate_bytecode(ParseState_T *P, char *outfile) {
  GenerateState_T *G = gen_init(P, outfile);
  determine_local_indices(G);
  
  write_s(G, "JMP __ENTRY__\n");
  generate_block(G, &G->P->root);
  write_s(G, "__ENTRY__:\n");
  write_s(G, "CALL main\n");
  write_s(G, "HALT\n");
}
