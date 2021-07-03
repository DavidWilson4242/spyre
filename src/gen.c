#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "gen.h"

/* syntax generation */
static void generate_function(GenerateState_T *, ASTNode_T **);
static void generate_block(GenerateState_T *, ASTNode_T **);
static void generate_if(GenerateState_T *, ASTNode_T **);
static void generate_while(GenerateState_T *, ASTNode_T **);
static void generate_return(GenerateState_T *, ASTNode_T **);

/* expression generation */
static void generate_type_db(GenerateState_T *G, const Datatype_T *);
static void generate_expression(GenerateState_T *G, NodeExpression_T *);
static void generate_binary_expression(GenerateState_T *G, BinaryOpNode_T *);
static void generate_unary_expression(GenerateState_T *G, UnaryOpNode_T *);
static void generate_integer_expression(GenerateState_T *G, int64_t); 
static void generate_assignment(GenerateState_T *G, BinaryOpNode_T *);
static void generate_member_index(GenerateState_T *G, BinaryOpNode_T *);
static void generate_call(GenerateState_T *G, CallNode_T *);

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

static NodeFunction_T *get_function(GenerateState_T *G, const char *ident) {
  return hash_get(G->P->functions, ident);
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

static void generate_type_db(GenerateState_T *G, const Datatype_T *dt) {
  write_s(G, dt->type_name);
  write_s(G, ": db");
  write_s(G, "\"");
  write_s(G, dt->type_name);
  write_s(G, "\"\n");
}

static void generate_type_db_map(const char *key, void *value, void *cl) {
  generate_type_db((GenerateState_T *)cl, (const Datatype_T *)value);
}

static void generate_cfunc_db(GenerateState_T *G, const Declaration_T *decl) {
  write_s(G, decl->name);
  write_s(G, ": db");
  write_s(G, "\"");
  write_s(G, decl->name);
  write_s(G, "\"\n");
}

static void generate_cfunc_db_map(const char *key, void *value, void *cl) {
  generate_cfunc_db((GenerateState_T *)cl, (const Declaration_T *)value);
}

static void generate_function(GenerateState_T *G, ASTNode_T **funcp) {

  ASTNode_T *func = *funcp;
  ASTNode_T **next = &func->next;
  NodeFunction_T *funcnode = func->nodefunc;
  
  /* write function label */
  size_t retlabel = G->lcount++;
  G->funclabel = retlabel;
  write_s(G, func->nodefunc->func_name); 
  write_s(G, ":\n");

  /* reserve local space */
  write_s(G, "RESL ");
  write_int(G, func->nodefunc->stack_space/8);
  write_s(G, "\n");

  /* load arguments onto stack and save as locals */
  for (Declaration_T *arg = funcnode->args; arg != NULL; arg = arg->next) {
    write_s(G, "ARG ");
    write_int(G, arg->local_index);
    write_s(G, "\nSVL ");
    write_int(G, arg->local_index);
    write_s(G, "\n");
  }

  generate_block(G, next);
  write_label(G, retlabel);
  write_s(G, "\n");

  /* RET vs. IRET? */
  if (funcnode->dt->fdesc->return_type) {
    write_s(G, "I");
  }

  write_s(G, "RET\n");
  *funcp = (*funcp)->next;
}

static void generate_while(GenerateState_T *G, ASTNode_T **whilep) {
  size_t top_label = G->lcount++;
  size_t bot_label = G->lcount++;
  ASTNode_T *node = *whilep;
  ASTNode_T **next = &node->next;
  write_label(G, top_label);
  write_s(G, "\n");
  generate_expression(G, node->nodewhile->cond);
  write_s(G, "ITEST\n");
  write_s(G, "JZ ");
  write_label_ref(G, bot_label);
  write_s(G, "\n");
  generate_block(G, next);
  write_s(G, "JMP ");
  write_label_ref(G, top_label);
  write_s(G, "\n");
  write_label(G, bot_label);
  write_s(G, "\n");
  *whilep = (*whilep)->next;
} 

static void generate_for(GenerateState_T *G, ASTNode_T **forp) {
  size_t top_label = G->lcount++;
  size_t bot_label = G->lcount++;
  ASTNode_T *node = *forp;
  ASTNode_T **next = &node->next;
  if (node->nodefor->init) {
    generate_expression(G, node->nodefor->init);
  }
  write_label(G, top_label);
  write_s(G, "\n");
  generate_expression(G, node->nodefor->cond);
  write_s(G, "ITEST\n");
  write_s(G, "JZ ");
  write_label_ref(G, bot_label);
  write_s(G, "\n");
  generate_block(G, next);
  if (node->nodefor->incr) {
    generate_expression(G, node->nodefor->incr);
  }
  write_s(G, "JMP ");
  write_label_ref(G, top_label);
  write_s(G, "\n");
  write_label(G, bot_label);
  write_s(G, "\n");
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

static void generate_return(GenerateState_T *G, ASTNode_T **retp) {
  NodeReturn_T *retnode = (*retp)->noderet;

  if (retnode->retval) {
    generate_expression(G, retnode->retval); 
  }

  write_s(G, "JMP ");
  write_label_ref(G, G->funclabel);
  write_s(G, "\n");
}

static void generate_integer_expression(GenerateState_T *G, int64_t value) {
  write_s(G, "IPUSH ");
  write_int(G, value);
  write_s(G, "\n");
}

/* handles the binary operator '=' */
static void generate_assignment(GenerateState_T *G, BinaryOpNode_T *exp) {
  
  bool is_struct_lhs = exp->left_operand->type == EXP_BINARY &&
                       exp->left_operand->binop->optype == '.';

  if (exp->left_operand->binop->optype == '.') {
    
    const BinaryOpNode_T *memberacc = exp->left_operand->binop; 
    const Datatype_T *struct_type = memberacc->left_operand->resolved;
    const char *member_name       = memberacc->right_operand->identval;
    
    /* sanity check it's a valid struct member format */
    assert(struct_type->type == DT_STRUCT);
    assert(memberacc->right_operand->type == EXP_IDENTIFIER);

    Declaration_T *struct_member = hash_get(struct_type->sdesc->members, member_name);
    assert(struct_member != NULL);

    /* write! */
    write_s(G, "SVMBR ");
    write_int(G, struct_member->struct_index);
    write_s(G, "\n");

  } else { 
    write_s(G, "SVLS\n");
  }

}

/* handles the binary operator '.' */
static void generate_member_index(GenerateState_T *G, BinaryOpNode_T *exp) {
  
  const NodeExpression_T *me = exp->me;
  bool is_assign = (me->parent &&
                    me->parent->type == EXP_BINARY &&
		    me->parent->binop->optype == '=');
  
  const Datatype_T *struct_type = exp->left_operand->resolved;
  const char *member_name       = exp->right_operand->identval;
  
  /* sanity check it's a valid struct member format */
  assert(exp->left_operand->resolved->type == DT_STRUCT);
  assert(exp->right_operand->type == EXP_IDENTIFIER);

  Declaration_T *struct_member = hash_get(struct_type->sdesc->members, member_name);
  assert(struct_member != NULL);
  
  /* dont dereference! */ 
  if (is_assign && me->leaf == LEAF_LEFT) {
    return;
  }

  write_s(G, "LDMBR ");
  write_int(G, struct_member->struct_index);
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
    case SPECO_EQ:
      write_s(G, "ICMP\n");
      write_s(G, "FEQ\n");
      break;
    case SPECO_LE:
      write_s(G, "ICMP\n");
      write_s(G, "FLE\n");
      break;
    case SPECO_GE:
      write_s(G, "ICMP\n");
      write_s(G, "FGE\n");
      break;
    case '<':
      write_s(G, "ICMP\n");
      write_s(G, "FLT\n");
      break;
    case '>':
      write_s(G, "ICMP\n");
      write_s(G, "FGT\n");
      break; 
    case '=':
      generate_assignment(G, exp);
      break;
    case '.':
      generate_member_index(G, exp);
      break;
    default:
      break;
  }
}

/* assumes exp is of type EXP_IDENTIFIER */
static void generate_identifier_expression(GenerateState_T *G, NodeExpression_T *exp) {
  ASTNode_T *at = G->at;
  
  bool is_member = (exp->parent &&
                    exp->parent->type == EXP_BINARY &&
		    exp->parent->binop->optype == '.' &&
		    exp->leaf == LEAF_RIGHT);
  bool is_assign = (exp->parent &&
                    exp->parent->type == EXP_BINARY &&
		    exp->parent->binop->optype == '=');
  bool dont_der = false;
  
  /* if it's a member lookup, do nothing.  it's the parent operator's job
   * to take care of this */
  if (is_member) {
    return;
  }

  /* if it's an identifier on the LHS of =, load address */
  dont_der = exp->leaf == LEAF_LEFT && is_assign; 

  /* is it a local? */
  Declaration_T *decl = get_local(G->at, exp->identval);
  
  if (decl) { 

    /* if don't dereference and it's a local, we're going to
     * push the variable's local index onto the stack */
    write_s(G, dont_der ? "IPUSH " : "LDL ");
    write_int(G, decl->local_index);
    write_s(G, "\n");
  }

}

static void generate_new_expression(GenerateState_T *G, NewNode_T *new) {
  write_s(G, "ALLOC ");
  write_s(G, new->dt->type_name);
  write_s(G, "\n"); 
}

static void generate_call(GenerateState_T *G, CallNode_T *call) {

  generate_expression(G, call->func); 
  generate_expression(G, call->args);
  
  /* TODO function pointers */
  if (call->func->type == EXP_IDENTIFIER) {
    bool is_cfunc = false;
    NodeFunction_T *func = get_function(G, call->func->identval);
    if (func) {
      assert(func != NULL);
      write_s(G, "CALL ");
      write_s(G, func->func_name);
      write_s(G, " ");
      write_int(G, func->dt->fdesc->nargs);
      write_s(G, "\n");
    } else {
      Declaration_T *cfunc = hash_get(G->P->cfunctions, call->func->identval);
      assert(cfunc != NULL);
      write_s(G, "CCALL ");
      write_s(G, cfunc->name);
      write_s(G, " ");
      write_int(G, cfunc->dt->fdesc->nargs);
      write_s(G, "\n");
    }
  }

}

static void generate_expression(GenerateState_T *G, NodeExpression_T *exp) {

  if (exp == NULL) {
    return;
  }
  
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
      generate_call(G, exp->callop);
      break;
    case EXP_FLOAT:
      break;
    case EXP_IDENTIFIER:
      generate_identifier_expression(G, exp); 
      break;
    case EXP_NEW:
      generate_new_expression(G, exp->newop);
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
      case NODE_FOR:
        generate_for(G, &c);
        break;
      case NODE_WHILE:
        generate_while(G, &c);
        break;
      case NODE_EXPRESSION:
        generate_expression(G, c->nodeexp);
        break;
      case NODE_RETURN:
        generate_return(G, &c);
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

  hash_foreach(P->usertypes, generate_type_db_map, G);
  hash_foreach(P->cfunctions, generate_cfunc_db_map, G);
  generate_block(G, &G->P->root);

  write_s(G, "__ENTRY__:\n");
  write_s(G, "CALL main 0\n");
  write_s(G, "HALT\n");
  fclose(G->outfile);
}
