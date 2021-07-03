#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include "lex.h"
#include "parse.h"
#include "hash.h"

/* this file converts a stream of tokens into an abstract syntax tree
 * and performs all necessary validation before the bytecode generation stage */

#define INTTYPE_NAME  "int"
#define FLTTYPE_NAME  "float"
#define CHARTYPE_NAME "char"
#define BOOLTYPE_NAME "bool"

#define INTTYPE_SIZE  8
#define FLTTYPE_SIZE  8
#define BOOLTYPE_SIZE 8
#define CHARTYPE_SIZE 1

/* forward decls */
static void mark_operator(ParseState_T *, uint8_t, uint8_t);
static NodeExpression_T *parse_new_datatype(ParseState_T *, ASTNode_T *);

typedef struct OperatorDescriptor {
  unsigned prec;
  enum {
    ASSOC_LEFT  = 1,
    ASSOC_RIGHT = 2
  } assoc;
  enum {
    OPERAND_UNARY  = 1,
    OPERAND_BINARY = 2
  } optype;
} OperatorDescriptor_T;

typedef struct ParsedFunctionHeader {
  Declaration_T *header;
  Declaration_T *args; 
} ParsedFunctionHeader_T;

static const OperatorDescriptor_T prec_table[255] = {
  [',']				= {1,  ASSOC_LEFT,  OPERAND_BINARY},
  ['=']				= {2,  ASSOC_RIGHT, OPERAND_BINARY},
  [SPECO_INC_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
  [SPECO_DEC_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
  [SPECO_MUL_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
  [SPECO_DIV_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
  [SPECO_MOD_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
  [SPECO_SHL_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
  [SPECO_SHR_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
  [SPECO_AND_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
  [SPECO_OR_BY]			= {2,  ASSOC_RIGHT, OPERAND_BINARY},
  [SPECO_XOR_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
  [SPECO_LOG_AND]		= {3,  ASSOC_LEFT,  OPERAND_BINARY},
  [SPECO_LOG_OR]		= {3,  ASSOC_LEFT,  OPERAND_BINARY},
  [SPECO_EQ]			= {4,  ASSOC_LEFT,  OPERAND_BINARY},
  [SPECO_NEQ]			= {4,  ASSOC_LEFT,  OPERAND_BINARY},
  ['>']				= {6,  ASSOC_LEFT,  OPERAND_BINARY},
  [SPECO_GE]			= {6,  ASSOC_LEFT,  OPERAND_BINARY},
  ['<']				= {6,  ASSOC_LEFT,  OPERAND_BINARY},
  [SPECO_LE]			= {6,  ASSOC_LEFT,  OPERAND_BINARY},
  ['|']				= {7,  ASSOC_LEFT,  OPERAND_BINARY},
  [SPECO_SHL]			= {7,  ASSOC_LEFT,  OPERAND_BINARY},
  [SPECO_SHR]			= {7,  ASSOC_LEFT,  OPERAND_BINARY},
  ['+']				= {8,  ASSOC_LEFT,  OPERAND_BINARY},
  ['-']				= {8,  ASSOC_LEFT,  OPERAND_BINARY},
  ['*']				= {9,  ASSOC_LEFT,  OPERAND_BINARY},
  ['%']				= {9,  ASSOC_LEFT,  OPERAND_BINARY},
  ['/']				= {9,  ASSOC_LEFT,  OPERAND_BINARY},
  ['@']				= {10, ASSOC_RIGHT, OPERAND_UNARY},
  ['$']				= {10, ASSOC_RIGHT, OPERAND_UNARY},
  ['!']				= {10, ASSOC_RIGHT, OPERAND_UNARY},
  [SPECO_TYPENAME]		= {10, ASSOC_RIGHT, OPERAND_UNARY},
  [SPECO_CAST]			= {10, ASSOC_RIGHT, OPERAND_UNARY},
  [SPECO_INC_ONE]		= {11, ASSOC_LEFT,  OPERAND_UNARY},
  [SPECO_DEC_ONE]		= {11, ASSOC_LEFT,  OPERAND_UNARY},
  [SPECO_CALL]			= {11, ASSOC_LEFT,  OPERAND_UNARY},
  [SPECO_INDEX]			= {11, ASSOC_LEFT,  OPERAND_UNARY},
  ['.']				= {11, ASSOC_LEFT,  OPERAND_BINARY}
};

static void indent(size_t n) {
  for (size_t i = 0; i < n; i++) {
    printf("  ");
  }
}

static void parse_err(ParseState_T *P, const char *fmt, ...) {

  va_list varargs;
  va_start(varargs, fmt);

  fprintf(stderr, "Spyre Parse error:\n");
  fprintf(stderr, "\tmessage: ");
  vfprintf(stderr, fmt, varargs);
  fprintf(stderr, "\n\tline:    %u\n", P->tok ? P->tok->lineno : 0);

  va_end(varargs);

  exit(EXIT_FAILURE);
}

/* not to be used for creating structs or functions... just primitives */
/* can, however, be called then rewritten over when creating a struct
 * or a function datatype */
static Datatype_T *make_datatype(const char *type_name, unsigned arrdim, unsigned ptrdim,
    unsigned primsize, bool is_const) {
  Datatype_T *dt = malloc(sizeof(Datatype_T));
  assert(dt);

  dt->type_name = malloc(strlen(type_name) + 1);
  assert(dt->type_name);
  strcpy(dt->type_name, type_name);

  dt->arrdim   = arrdim;
  dt->ptrdim   = ptrdim;
  dt->primsize = primsize;
  dt->is_const = is_const;
  dt->fdesc    = NULL;
  dt->sdesc    = NULL;
  dt->next     = NULL;
  dt->type     = DT_PRIMITIVE;

  return dt;

}

/* creates an empty datatype with empty fields */
static Datatype_T *make_empty_datatype(const char *type_name) {
  Datatype_T *dt = calloc(1, sizeof(Datatype_T));
  assert(dt);

  if (type_name != NULL) {
    dt->type_name = malloc(strlen(type_name) + 1);
    assert(dt->type_name);
    strcpy(dt->type_name, type_name);
  } else {
    dt->type_name = NULL;
  }

  return dt;
}

/* does not clone members.  leaves as NULL.  this is used
 * to clone the builtin primitive types, and is not meant
 * for cloning datatypes with members */
static Datatype_T *clone_datatype(Datatype_T *dt) {
  return make_datatype(dt->type_name, dt->arrdim, dt->ptrdim,
      dt->primsize, dt->is_const);
}

/* TODO support for functions, structs */
static void print_datatype(Datatype_T *dt) {
  printf("%s", dt->type_name);
  for (size_t i = 0; i < dt->ptrdim; i++) {
    printf("^");
  }
  for (size_t i = 0; i < dt->arrdim; i++) {
    printf("[]");
  }
}

static void print_decl(Declaration_T *decl) {
  printf("%s: ", decl->name);
  print_datatype(decl->dt);
}

static Declaration_T *empty_decl() {
  Declaration_T *decl = malloc(sizeof(Declaration_T));
  assert(decl);
  decl->name = NULL;
  decl->dt = NULL;
  decl->next = NULL;
  decl->local_index = 0;
  return decl;
}

static ASTNode_T *empty_node(ASTNodeType_T type) {
  ASTNode_T *node = malloc(sizeof(ASTNode_T));
  assert(node);
  node->type = type;
  node->next = NULL;
  node->prev = NULL;
  node->parent = NULL;
  switch (type) {
    case NODE_IF:
      node->nodeif = calloc(1, sizeof(NodeIf_T));
      break;
    case NODE_WHILE:
      node->nodewhile = calloc(1, sizeof(NodeWhile_T));
      break;
    case NODE_BLOCK:
      node->nodeblock = calloc(1, sizeof(NodeBlock_T));
      break;
    case NODE_EXPRESSION:
      node->nodeexp = calloc(1, sizeof(NodeExpression_T));
      break;
    case NODE_FOR:
      node->nodefor = calloc(1, sizeof(NodeFor_T));
      break;
    case NODE_FUNCTION:
      node->nodefunc = calloc(1, sizeof(NodeFunction_T));
      break;
    case NODE_RETURN:
      node->noderet = calloc(1, sizeof(NodeReturn_T));
      break;
    case NODE_CONTINUE:
    case NODE_INCLUDE:
    case NODE_DECLARATION:
    default:
      break;
  }
  assert(node->nodeif); /* all nodetypes share same address */
  return node;
}

/* verifies that a node can be placed at the next spot.  for example,
 * only blocks and statements may follow an if-statement */
static void verify_node(ParseState_T *P, ASTNode_T *node) {
  if (P->backnode == NULL) {
    return;
  }
  if (P->backnode->type == NODE_IF) {
    if (node->type != NODE_EXPRESSION && node->type != NODE_BLOCK) {
      parse_err(P, "only a statement or block may follow an if-conditional");
    }
  }
  if (P->backnode->type == NODE_WHILE) {
    if (node->type != NODE_EXPRESSION && node->type != NODE_BLOCK) {
      parse_err(P, "only a statement or block may follow a while-loop");
    }
  }
}

static bool is_in_func(ParseState_T *P) {
  ASTNode_T *start = P->backnode != NULL ? P->backnode->parent : P->block->parent;
  while (start != NULL && start != P->root) {
    if (start->type == NODE_BLOCK && start->prev != NULL) {
      if (start->prev->type == NODE_FUNCTION) {
        return true;
      }
    }
    start = start->parent;
  }
  return false;
}

static void append_node(ParseState_T *P, ASTNode_T *node) {
  verify_node(P, node);
  node->next = NULL;
  node->prev = NULL;
  node->parent = P->block;
  if (P->backnode != NULL) {
    node->prev = P->backnode;
    P->backnode->next = node;
  } else {
    P->block->nodeblock->children = node;
  }
  P->backnode = node;
  if (node->type == NODE_BLOCK) {
    P->block = node;
    P->backnode = NULL;
  }
}

static void add_variable_to_block(ParseState_T *P, Declaration_T *decl) {
  if (!P->block) {
    parse_err(P, "variable declarations can only exist inside of a block");
  }

  NodeBlock_T *block = P->block->nodeblock;

  /* add to back, maintaining proper order */
  if (block->vars) {
    block->backvar->next = decl;
    block->backvar = decl;
  } else {
    block->vars = decl;
    block->backvar = decl;
  }
  decl->next = NULL;

}

static inline LexToken_T *at(ParseState_T *P) {
  return P->tok;
}

static inline void advance(ParseState_T *P, int n) {
  for (int i = 0; i < n && P->tok != NULL; i++) {
    P->tok = P->tok->next;
  }
}

static LexToken_T *peek(ParseState_T *P, int n) {
  LexToken_T *token = P->tok;
  for (size_t i = 0; i < n; i++) {
    if (token == NULL) {
      return NULL;
    }
    token = token->next;
  }
  return token;
}

static bool on_string(ParseState_T *P, const char *id, LexToken_T *tok) {
  LexToken_T *t = (tok != NULL) ? tok : P->tok;
  if (t == NULL) {
    return false;
  }
  return !strcmp(t->as_string, id);
}

static bool on_type(ParseState_T *P, LexTokenType_T type, LexToken_T *tok) {
  LexToken_T *t = (tok != NULL) ? tok : P->tok;
  if (t == NULL) {
    return false;
  }
  return t->type == type;
}

static void eat(ParseState_T *P, const char *id) {
  if (!on_string(P, id, NULL)) {
    parse_err(P, "expected token '%s', got '%s'", id, P->tok->as_string);
  }
  advance(P, 1);
}

static void safe_eat(ParseState_T *P) {
  advance(P, 1);
  if (!P->tok) {
    parse_err(P, "unexpected EOF");
  }
}

static void expstack_push(ExpressionStack_T **stack, NodeExpression_T *node) {
  ExpressionStack_T *append = malloc(sizeof(ExpressionStack_T));
  assert(append);
  append->node = node;
  append->next = NULL;
  append->prev = NULL;
  if (!(*stack)) {
    *stack = append;
  } else {
    (*stack)->prev = append;
    append->next = (*stack);
    *stack = append;
  }
}

static NodeExpression_T *expstack_pop(ExpressionStack_T **stack) {
  if (*stack == NULL) {
    return NULL;
  }

  NodeExpression_T *node = (*stack)->node;
  ExpressionStack_T *top = *stack;
  ExpressionStack_T *next = (*stack)->next;
  if (next) {
    next->prev = NULL;
    *stack = next;
  } else {
    *stack = NULL;
  }

  free(top);

  return node;
}

static NodeExpression_T *expstack_top(ExpressionStack_T **stack) {
  if (*stack == NULL) {
    return NULL;
  }
  return (*stack)->node;
}

void expstack_print(ExpressionStack_T **stack) {
  for (ExpressionStack_T *s = *stack; s != NULL; s = s->next) {
    NodeExpression_T *node = s->node;
    switch (node->type) {
      case EXP_INTEGER:
        printf("%ld ", node->ival);
        break;
      case EXP_BINARY:
        printf("%c ", node->binop->optype);
        break;
      default:
        break;
    }
  }
  printf("\n");
}

/* implementation of the shunting yard algorithm */
static void shunting_pops(ExpressionStack_T **postfix, ExpressionStack_T **operators,
    const OperatorDescriptor_T *opdesc) {
  NodeExpression_T *top;
  const OperatorDescriptor_T *top_desc;

  while ((top = expstack_top(operators)) != NULL) {

    switch (top->type) {
      case EXP_BINARY:
        top_desc = &prec_table[top->binop->optype];
        break;
      case EXP_UNARY:
        top_desc = &prec_table[top->unop->optype];
        break;
      case EXP_INDEX:
        top_desc = &prec_table[SPECO_INDEX];
        break;
      case EXP_CALL:
        top_desc = &prec_table[SPECO_CALL];
        break;
      default:
        top_desc = NULL;
    }

    /* found an open parenthesis? quit loop */
    if (top->type == EXP_UNARY && top->unop->optype == '(') {
      break;
    }

    /* use precedence rules to determine break */
    if (opdesc->assoc == ASSOC_LEFT && opdesc->prec > top_desc->prec) {
      break;
    } else if (opdesc->assoc == ASSOC_RIGHT && opdesc->prec >= top_desc->prec) {
      break;
    }

    expstack_push(postfix, expstack_pop(operators));
  }
}

static void expnode_print(NodeExpression_T *node, size_t ind) {
  indent(ind);
  switch (node->type) {
    case EXP_INTEGER:
      printf("%ld\n", node->ival);
      break;
    case EXP_FLOAT:
      printf("%f\n", node->fval);
      break;
    case EXP_IDENTIFIER:
      printf("%s\n", node->identval);
      break;
    case EXP_BINARY:
      printf("%c|%d\n", node->binop->optype, node->binop->optype);
      expnode_print(node->binop->left_operand, ind + 1);
      expnode_print(node->binop->right_operand, ind + 1);
      break;
    case EXP_UNARY:
      printf("%c|%d\n", node->unop->optype, node->unop->optype);
      expnode_print(node->unop->operand, ind + 1);
      break;
    case EXP_INDEX:
      printf("IDX\n");
      expnode_print(node->inop->array, ind + 1);
      expnode_print(node->inop->index, ind + 1);
      break;
    case EXP_CALL:
      printf("CALL\n");
      expnode_print(node->callop->func, ind + 1);
      if (node->callop->args != NULL) {
        expnode_print(node->callop->args, ind + 1);
      }
      break;
    case EXP_NEW:
      printf("NEW\n");
      indent(ind + 1);
      printf("TYPENAME: %s\n", node->newop->dt->type_name);
      indent(ind + 1);
      printf("DIM: %zu\n", node->newop->arrdim);
      indent(ind + 1);
      printf("DIMS: [\n");
      for (NodeExpression_T *n = node->newop->arrsize; n != NULL; n = n->next) {
        expnode_print(n, ind + 2);
      }
      indent(ind + 1);
      printf("]\n");
      break;
    default:
      break;
  }
}

static void astnode_print(ASTNode_T *node, size_t ind) {
  if (node == NULL) {
    return;
  }
  indent(ind);
  switch (node->type) {
    case NODE_IF:
      printf("IF: {\n");
      indent(ind + 1);
      printf("COND: {\n");
      expnode_print(node->nodeif->cond, ind + 2);
      indent(ind + 1);
      printf("}\n");
      indent(ind);
      printf("}\n");
      break;
    case NODE_WHILE:
      printf("WHILE: {\n");
      indent(ind + 1);
      printf("COND: {\n");
      expnode_print(node->nodewhile->cond, ind + 2);
      indent(ind + 1);
      printf("}\n");
      indent(ind);
      printf("}\n");
      break;
    case NODE_FUNCTION:
      printf("FUNCTION: {\n");
      indent(ind + 1);
      printf("NAME: %s\n", node->nodefunc->func_name);
      indent(ind + 1);
      printf("RETURNS: ");
      if (node->nodefunc->rettype != NULL) {
        print_datatype(node->nodefunc->rettype);
      } else {
        printf("void");
      }
      printf("\n");
      indent(ind + 1);
      printf("ARGS: {\n");
      for (Declaration_T *arg = node->nodefunc->args; arg != NULL; arg = arg->next) {
        indent(ind + 2);
        print_decl(arg);
        printf("\n");
      }
      indent(ind + 1);
      printf("}\n");
      if (node->nodefunc->special_ret != NULL) {
        indent(ind + 1);
        printf("SPECIAL_RET: {\n");
        expnode_print(node->nodefunc->special_ret, ind + 2);
        indent(ind + 1);
        printf("}\n");
      }
      indent(ind);
      printf("}\n");
      break;
    case NODE_RETURN:
      printf("RETURN: {\n");
      expnode_print(node->noderet->retval, ind + 1);
      indent(ind);
      printf("}\n");
      break;
    case NODE_BLOCK:
      printf("BLOCK: {\n");
      indent(ind + 1);
      printf("LOCALS: {\n");
      for (Declaration_T *decl = node->nodeblock->vars; decl; decl = decl->next) {
        indent(ind + 2);
        print_decl(decl);
        printf("\n");
      }
      indent(ind + 1);
      printf("}\n");
      indent(ind + 1);
      printf("CHILDREN: {\n");
      for (ASTNode_T *child = node->nodeblock->children; child; child = child->next) {
        astnode_print(child, ind + 2);
      }
      indent(ind + 1);
      printf("}\n");
      indent(ind);
      printf("}\n");
      break;
    case NODE_EXPRESSION:
      printf("STMT: {\n");
      expnode_print(node->nodeexp, ind + 1);
      indent(ind);
      printf("}\n");
      break;
    default:
      break;
  }
}

static NodeExpression_T *empty_expnode(NodeExpressionType_T type, size_t lineno) {
  NodeExpression_T *node = malloc(sizeof(NodeExpression_T));
  assert(node);
  node->parent = NULL;
  node->nodeparent = NULL;
  node->type = type;
  node->lineno = lineno;
  node->next = NULL;
  switch (type) {
    case EXP_INTEGER:
      node->ival = 0;
      break;
    case EXP_FLOAT:
      node->fval = 0.0f;
      break;
    case EXP_BINARY:
      node->binop = malloc(sizeof(BinaryOpNode_T));
      assert(node->binop);
      node->binop->left_operand = NULL;
      node->binop->right_operand = NULL;
      node->binop->me = node;
      break;
    case EXP_UNARY:
      node->unop = malloc(sizeof(UnaryOpNode_T));
      assert(node->unop);
      node->unop->operand = NULL;
      node->unop->me = node;
      break;
    case EXP_INDEX:
      node->inop = malloc(sizeof(IndexNode_T));
      assert(node->inop);
      node->inop->array = NULL;
      node->inop->index = NULL;
      node->inop->me = node;
      break;
    case EXP_CALL:
      node->callop = malloc(sizeof(CallNode_T));
      assert(node->callop);
      node->callop->func = NULL;
      node->callop->args = NULL;
      node->callop->me = node;
      break;
    case EXP_NEW:
      node->newop = malloc(sizeof(NewNode_T));
      node->newop->dt = NULL;
      node->newop->arrdim = 0;
      node->newop->arrsize = NULL;
      node->newop->me = node;
      break;
    default:
      break;
  }
  return node;
}

/* expects a valid mark in P->mark.  then, gathers all tokens until
 * that mark and converts the expression into a postfix tree
 * representation */
static NodeExpression_T *parse_expression(ParseState_T *P, ASTNode_T *nodeparent) {

  ExpressionStack_T *operators = NULL;
  ExpressionStack_T *postfix   = NULL;
  ExpressionStack_T *tree      = NULL;
  NodeExpression_T *node;
  NodeExpression_T *top;
  LexToken_T *oldmark;
  LexToken_T *prev = NULL;
  const OperatorDescriptor_T *opinfo;

  /* ===== PHASE ONE | SHUNTING YARD ===== */

  /* implementation of the shunting yard algorithm.  converts the
   * expression from infix notation to postix notation */
  while (P->tok != P->mark) {
    LexToken_T *t = P->tok;
    switch (t->type) {
      case TOKEN_INTEGER:
        node = empty_expnode(EXP_INTEGER, t->lineno);
        node->ival = t->ival;
        expstack_push(&postfix, node);
        safe_eat(P);
        break;
      case TOKEN_FLOAT:
        node = empty_expnode(EXP_FLOAT, t->lineno);
        node->fval = t->fval;
        expstack_push(&postfix, node);
        safe_eat(P);
        break;
      case TOKEN_IDENTIFIER:

        /* 'new' is a special case identifier */
        if (on_string(P, "new", NULL)) {
          safe_eat(P);
          oldmark = P->mark;
          node = parse_new_datatype(P, nodeparent);
          /* mark gets mutated... */
          P->mark = oldmark;
          expstack_push(&postfix, node);
        } else {
          
          node = empty_expnode(EXP_IDENTIFIER, t->lineno);
          node->identval = malloc(strlen(t->as_string) + 1);
          assert(node->identval);
          strcpy(node->identval, t->as_string);
          expstack_push(&postfix, node);
        safe_eat(P);
        }
        break;

      case TOKEN_OPERATOR:

        /* standard operator? */
        if (prec_table[t->oval].prec) {
          opinfo = &prec_table[t->oval];
          shunting_pops(&postfix, &operators, opinfo);
          if (opinfo->optype) {
            node = empty_expnode(EXP_BINARY, t->lineno);
            node->binop->optype = t->oval;
          } else {
            node = empty_expnode(EXP_UNARY, t->lineno);
            node->unop->optype = t->oval;
          }
          expstack_push(&operators, node);

          /* function call? TODO more cases */
        } else if (t->oval == '(' && prev != NULL && prev->type == TOKEN_IDENTIFIER) {
          node = empty_expnode(EXP_CALL, t->lineno);
          oldmark = P->mark;
          safe_eat(P);
          if (on_string(P, ")", NULL)) {
            node->callop->args = NULL;
          } else {
            mark_operator(P, '(', ')');
            node->callop->args = parse_expression(P, NULL);
            node->callop->args->parent = node;
          }
          node->callop->func = NULL;
          P->mark = oldmark;
          opinfo = &prec_table[SPECO_CALL];
          shunting_pops(&postfix, &operators, opinfo);
          expstack_push(&operators, node);

          /* array index? */
        } else if (t->oval == '[') {
          node = empty_expnode(EXP_INDEX, t->lineno);
          oldmark = P->mark;
          safe_eat(P);
          mark_operator(P, '[', ']');
          node->inop->index = parse_expression(P, NULL);
          node->inop->array = NULL;
          P->mark = oldmark;
          opinfo = &prec_table[SPECO_INDEX];
          shunting_pops(&postfix, &operators, opinfo);
          expstack_push(&operators, node);

          /* standard opening parenthesis? */
        } else if (t->oval == '(') {
          node = empty_expnode(EXP_UNARY, t->lineno);
          node->unop->optype = '(';
          expstack_push(&operators, node);

          /* standard closing parenthesis? */
        } else if (t->oval == ')') {
          while (1) {
            top = expstack_top(&operators);
            if (!top) {
              parse_err(P, "unexpected closing parenthesis ')'");
            }
            if (top->type == EXP_UNARY && top->unop->optype == '(') {
              break;
            }
            expstack_push(&postfix, expstack_pop(&operators));
          }
          expstack_pop(&operators);
        } else {
          parse_err(P, "unknown operator '%s' in expression", t->as_string);
        }
        safe_eat(P);
        break;
      default:
        parse_err(P, "unexpected token '%s' when parsing expression", t->as_string);
    }

    /* if it's a unary or binary operator, write as_string value */
    if (node->type == EXP_UNARY) {
      node->unop->as_string = malloc(strlen(t->as_string) + 1);
      assert(node->unop->as_string);
      strcpy(node->unop->as_string, t->as_string);
    } else if (node->type == EXP_BINARY) {
      node->binop->as_string = malloc(strlen(t->as_string) + 1);
      assert(node->binop->as_string);
      strcpy(node->binop->as_string, t->as_string);
    }

    prev = t;
  }

  /* empty remaining operator stack into postfix */
  while (expstack_top(&operators)) {
    NodeExpression_T *node = expstack_pop(&operators);
    if (node->type == EXP_UNARY && (node->unop->optype == '(' || node->unop->optype == ')')) {
      parse_err(P, "mismatched parenthesis in expression");
    }
    expstack_push(&postfix, node);
  }

  /* reverse the stack onto a temp stack */
  ExpressionStack_T *temp = NULL;
  while (expstack_top(&postfix)) {
    expstack_push(&temp, expstack_pop(&postfix));
  }
  postfix = temp;


  /* ===== PHASE TWO | EXPRESSION TREE ===== */
  const char *malformed_message = "malformed expression";

  for (ExpressionStack_T *s = postfix; s != NULL; s = s->next) {
    node = s->node;
    switch (node->type) {
      case EXP_INTEGER:
      case EXP_FLOAT:
      case EXP_IDENTIFIER:
      case EXP_NEW:
        expstack_push(&tree, node);
        break;
      case EXP_INDEX:
        node->inop->array = expstack_pop(&tree);
        if (node->inop->array == NULL) {
          parse_err(P, malformed_message);
        }
        node->inop->array->parent = node;
        expstack_push(&tree, node);
        break;
      case EXP_CALL:
        node->callop->func = expstack_pop(&tree);
        if (node->callop->func == NULL) {
          parse_err(P, malformed_message);
        }
        node->callop->func->parent = node;
        expstack_push(&tree, node);
        break;
      case EXP_UNARY:
        top = expstack_pop(&tree);
        if (!top) {
          parse_err(P, malformed_message);
        }
        top->parent = node;
        node->unop->operand = top;
        expstack_push(&tree, node);
        break;
      case EXP_BINARY: {
        NodeExpression_T *leaves[2];

        /* binary operator? pop off two operands */
        for (size_t i = 0; i < 2; i++) {
          leaves[i] = expstack_pop(&tree);
          if (!leaves[i]) {
            parse_err(P, malformed_message);
          }
          leaves[i]->parent = node;
          leaves[i]->leaf = (i == 1 ? LEAF_LEFT : LEAF_RIGHT);
        }

        /* swap order */
        node->binop->left_operand = leaves[1];
        node->binop->right_operand = leaves[0];

        expstack_push(&tree, node);
        break;
      }
    }
  }

  NodeExpression_T *final_value = expstack_pop(&tree);

  /* only root-level expnode gets a reference to the parent node */
  final_value->nodeparent = nodeparent;

  if (tree != NULL) {
    parse_err(P, "an expression may only have one result");
  }

  return final_value;
}

/* sometimes, marks are used to denote the end of an expression.  for example,
 * when parsing an if statement, the ')' closing the expression marks where
 * the condition ends.  this function finds a mark. */
static void mark_operator(ParseState_T *P, uint8_t inc, uint8_t end) {
  size_t mark_count = 0;
  for (LexToken_T *t = P->tok; t != NULL; t = t->next) {
    if (t->type != TOKEN_OPERATOR) {
      continue;
    }
    if (inc != SPECO_NULL && t->oval == inc) {
      mark_count++;
    } else if (t->oval == end) {
      if (mark_count == 0) {
        P->mark = t; 
        return;
      }
      mark_count--;
    }
  }
  parse_err(P, "unexpected EOF while parsing expression.");
}

static Datatype_T *datatype_from_name(ParseState_T *P, const char *type_name) {
  Datatype_T *ret;
  Datatype_T *checktypes[] = {
    P->builtin->int_t,
    P->builtin->float_t,
    P->builtin->char_t,
    P->builtin->bool_t
  };
  for (size_t i = 0; i < sizeof(checktypes)/sizeof(Datatype_T *); i++) {
    if (!strcmp(checktypes[i]->type_name, type_name)) {
      return clone_datatype(checktypes[i]);
    }
  }

  return hash_get(P->usertypes, type_name);
}

static Datatype_T *parse_datatype(ParseState_T *P) {
  Datatype_T *dt = datatype_from_name(P, P->tok->as_string); 
  if (!dt) {
    parse_err(P, "unknown typename '%s'", P->tok->as_string);
  }
  safe_eat(P);
  while (on_string(P, "[", NULL)) {
    safe_eat(P);
    eat(P, "]");
    dt->arrdim++;
  }
  return dt;
}

/* this is a special case of parse_datatype. expected to follow
 * the 'new' keyword, where the user can specify array dimensions
 * inside of brackets.  example syntax:
 * new int[10*20][30];
 * */
static NodeExpression_T *parse_new_datatype(ParseState_T *P, ASTNode_T *nodeparent) {
  NodeExpression_T *node = empty_expnode(EXP_NEW, P->tok->lineno);
  NodeExpression_T *dimsize;
  NodeExpression_T *backp = NULL;
  node->newop->dt = datatype_from_name(P, P->tok->as_string);
  node->newop->arrdim = 0;
  safe_eat(P);
  while (on_string(P, "[", NULL)) {
    node->newop->arrdim += 1;
    safe_eat(P);
    if (on_string(P, "]", NULL)) {
      parse_err(P, "expected array length following token '['");
    }
    mark_operator(P, '[', ']');
    dimsize = parse_expression(P, nodeparent);
    if (node->newop->arrsize == NULL) {
      node->newop->arrsize = dimsize;
    } else {
      backp->next = dimsize;
    }
    backp = dimsize;
    eat(P, "]");
  }
  return node;
}

static void parse_expression_node(ParseState_T *P) {
  if (on_string(P, ";", NULL)) {
    eat(P, ";");
    return;
  }
  ASTNode_T *node = empty_node(NODE_EXPRESSION);
  free(node->nodeexp);
  mark_operator(P, SPECO_NULL, ';');
  node->nodeexp = parse_expression(P, node);
  append_node(P, node);
  eat(P, ";");
}

static Declaration_T *parse_declaration(ParseState_T *P) {
  Declaration_T *decl = empty_decl();
  if (!on_type(P, TOKEN_IDENTIFIER, NULL)) {
    parse_err(P, "expected identifier in declaration, got token '%s'\n", P->tok->as_string);
  }
  decl->name = malloc(strlen(P->tok->sval) + 1);
  strcpy(decl->name, P->tok->sval);
  safe_eat(P);
  eat(P, ":");
  decl->dt = parse_datatype(P);
  return decl;
}

static bool should_parse_if(ParseState_T *P) {
  return on_string(P, "if", NULL);
}

/* syntax:
 * if (condition) ...
 */
static void parse_if(ParseState_T *P) {
  ASTNode_T *node = empty_node(NODE_IF);
  eat(P, "if");
  eat(P, "(");
  mark_operator(P, '(', ')');
  node->nodeif->cond = parse_expression(P, node);
  eat(P, ")");
  append_node(P, node);
}

static bool should_parse_return(ParseState_T *P) {
  return on_string(P, "return", NULL);
}

static void parse_return(ParseState_T *P) {
  ASTNode_T *node = empty_node(NODE_RETURN);
  eat(P, "return");
  if (on_string(P, ";", NULL)) {
    node->noderet->retval = NULL;
    safe_eat(P);
  } else {
    mark_operator(P, SPECO_NULL, ';');
    node->noderet->retval = parse_expression(P, node);
  }
  append_node(P, node);
}

static bool should_parse_for(ParseState_T *P) {
  return on_string(P, "for", NULL);
}

static void parse_for(ParseState_T *P) {
  ASTNode_T *node = empty_node(NODE_FOR);
  eat(P, "for");
  eat(P, "(");
  if (on_string(P, ";", NULL)) {
    node->nodefor->init = NULL;
  } else {
    mark_operator(P, SPECO_NULL, ';');
    node->nodefor->init = parse_expression(P, node);
  }
  eat(P, ";");
  mark_operator(P, SPECO_NULL, ';');
  node->nodefor->cond = parse_expression(P, node);
  eat(P, ";");
  if (on_string(P, ")", NULL)) {
    node->nodefor->incr = NULL;
  } else {
    mark_operator(P, '(', ')');
    node->nodefor->incr = parse_expression(P, node);
  }
  eat(P, ")");
  append_node(P, node);
}

static bool should_parse_while(ParseState_T *P) {
  return on_string(P, "while", NULL);
}

/* syntax:
 * while (condition) ...
 */
static void parse_while(ParseState_T *P) {
  ASTNode_T *node = empty_node(NODE_WHILE);
  eat(P, "while");
  eat(P, "(");
  mark_operator(P, '(', ')');
  node->nodewhile->cond = parse_expression(P, node);
  eat(P, ")");
  append_node(P, node);
}

static bool should_parse_function(ParseState_T *P) {
  return on_string(P, "func", NULL);
}

static bool should_parse_cfunction(ParseState_T *P) {
  return on_string(P, "cfunc", NULL);
}

/* expects to be on the token after "func" or "cfunc" */
static ParsedFunctionHeader_T parse_function_header(ParseState_T *P) {
  
  ParsedFunctionHeader_T parsed;

  Declaration_T *arg = NULL, *backarg = NULL;
  Declaration_T *header = empty_decl();

  /* the function gets its own datatype.. without a name */
  Datatype_T *dt = make_empty_datatype(NULL);
  dt->type = DT_FUNCTION;
  dt->fdesc = malloc(sizeof(FunctionDescriptor_T));
  assert(dt->fdesc);
  dt->fdesc->arguments = NULL;
  dt->fdesc->return_type = NULL;
  dt->fdesc->nargs = 0;
  
  if (!on_type(P, TOKEN_IDENTIFIER, NULL)) {
    parse_err(P, "expected function identifier");
  }
  header->name = malloc(strlen(P->tok->as_string) + 1);
  assert(header->name);
  strcpy(header->name, P->tok->as_string);

  safe_eat(P);
  eat(P, "(");

  while (!on_string(P, ")", NULL)) {
    arg = parse_declaration(P);
    if (backarg != NULL) {
      backarg->next = arg;
    }
    if (!on_string(P, ")", NULL) && !on_string(P, ",", NULL)) {
      parse_err(P, "expected ')' or ',' to follow function argument.  Got token '%s'",
          P->tok->as_string);
    }
    if (on_string(P, ",", NULL)) {
      safe_eat(P);
    }

    /* insert into argument list */
    if (dt->fdesc->arguments == NULL) {
      dt->fdesc->arguments = arg;
    } else {
      backarg->next = arg;
    }

    dt->fdesc->nargs++;

    backarg = arg;
  }
  
  /* parse the return value */
  eat(P, ")");
  eat(P, "->");

  /* special case for functions... may mark return type as void.  void will
   * only ever be used in this specific context */
  if (on_string(P, "void", NULL)) {
    dt->fdesc->return_type = NULL;
    safe_eat(P);
  } else {
    dt->fdesc->return_type = parse_datatype(P);
  }

  header->dt = dt;

  parsed.header = header;
  parsed.args = arg;
  
  return parsed;

}

/* syntax:
 * cfunc name(arg0: T, arg1: T, ...) -> T; */
static void parse_cfunction(ParseState_T *P) {

  if (is_in_func(P)) {
    parse_err(P, "c functions must be declared in the global scope");
  }

  eat(P, "cfunc");

  ParsedFunctionHeader_T header = parse_function_header(P);
  Declaration_T *decl = header.header;
  Declaration_T *args = header.args;
  
  /* register C function in context */
  hash_insert(P->cfunctions, decl->name, decl);

}

/* syntax:
 * func name(arg0: T, arg1: T, ...) -> T { ... } */
static void parse_function(ParseState_T *P) {

  if (is_in_func(P)) {
    parse_err(P, "functions within functions are not permitted");
  }

  ASTNode_T *func = empty_node(NODE_FUNCTION);
  NodeFunction_T *fnode = func->nodefunc;

  eat(P, "func");
  
  ParsedFunctionHeader_T header = parse_function_header(P);
  Declaration_T *decl = header.header;
  Declaration_T *args = header.args;

  /* copy details into fnode */
  fnode->func_name = malloc(strlen(decl->name) + 1);
  assert(fnode->func_name);
  strcpy(fnode->func_name, decl->name);
  fnode->dt = decl->dt;
  fnode->args = args;
  fnode->rettype = fnode->dt->fdesc->return_type;

  /* register function in table */
  hash_insert(P->functions, decl->name, decl);

  append_node(P, func);

  /* optionally, user may use the syntax
   * func name(...) -> T = <retval expression>
   * notice that we append the node before this case, so that
   * its arguments can be referenced by the expression */
  if (on_string(P, "=", NULL)) {
    eat(P, "=");
    mark_operator(P, SPECO_NULL, ';');
    fnode->special_ret = parse_expression(P, func); 
  }

}

static bool should_parse_struct(ParseState_T *P) {
  return on_type(P, TOKEN_IDENTIFIER, NULL)
    && on_string(P, ":", peek(P, 1))
    && on_string(P, "struct", peek(P, 2));
}

/* syntax:
 * typename: struct { ... } */
static void parse_struct(ParseState_T *P) {
  Declaration_T *member;
  Datatype_T *dt;

  /* ensure no duplicates */
  if (hash_get(P->usertypes, P->tok->sval)) {
    parse_err(P, "redeclaration of type '%s'", P->tok->sval);
  }

  dt = make_empty_datatype(P->tok->sval);
  safe_eat(P);

  dt->type = DT_STRUCT;
  dt->sdesc = malloc(sizeof(StructDescriptor_T));
  assert(dt->sdesc);
  dt->sdesc->members = hash_init();

  eat(P, ":");
  eat(P, "struct");
  eat(P, "{");

  /* insert new members into the struct's member table.  prevent
   * duplicate entries */
  int index = 0;
  while (!on_string(P, "}", NULL)) {
    member = parse_declaration(P); 
    eat(P, ";");
    if (hash_get(dt->sdesc->members, member->dt->type_name) != NULL) {
      parse_err(P, "duplicate member '%s' in struct '%s'", member->dt->type_name);
    }
    hash_insert(dt->sdesc->members, member->name, member);
    member->struct_index = index++;
  }

  eat(P, "}");

  /* add datatype to table */
  hash_insert(P->usertypes, dt->type_name, dt);
}

static bool should_leave_block(ParseState_T *P) {
  return on_string(P, "}", NULL);
}

static void leave_block(ParseState_T *P) {
  eat(P, "}");
  P->backnode = P->block;
  P->block = P->block->parent;
  if (!P->block) {
    parse_err(P, "unexpected '}' closing a block that doesn't exist");
  }
}

static bool should_parse_block(ParseState_T *P) {
  return on_string(P, "{", NULL);
}

static void parse_block(ParseState_T *P) {
  ASTNode_T *node = empty_node(NODE_BLOCK);
  eat(P, "{");
  append_node(P, node);
  P->block = node;
}

static bool should_parse_declaration(ParseState_T *P) {
  LexToken_T *next = peek(P, 1);
  if (!next || next->type != TOKEN_OPERATOR) {
    return false;
  }
  return on_type(P, TOKEN_IDENTIFIER, NULL) && next->oval == ':';
}

static ParseState_T *init_parsestate(LexState_T *L) {

  ParseState_T *P = malloc(sizeof(ParseState_T));
  assert(P);

  /* init default datatypes */
  P->builtin = malloc(sizeof(BuiltinTypes_T));
  assert(P->builtin);
  P->builtin->int_t   = make_datatype(INTTYPE_NAME,  0, 0, INTTYPE_SIZE,  false);
  P->builtin->float_t = make_datatype(FLTTYPE_NAME,  0, 0, FLTTYPE_SIZE,  false);
  P->builtin->char_t  = make_datatype(CHARTYPE_NAME, 0, 0, CHARTYPE_SIZE, false);
  P->builtin->bool_t  = make_datatype(BOOLTYPE_NAME, 0, 0, BOOLTYPE_SIZE, false);

  /* init default functions */
  P->functions = hash_init();
  P->cfunctions = hash_init();

  /* init abstract syntax tree with just a root.
   * the root is just a block, essentially parsing 
   * the entire file as: { ..file.. } */
  P->root = empty_node(NODE_BLOCK);
  P->block = P->root;
  P->backnode = NULL;

  P->tok = L->tokens;

  P->usertypes = hash_init();

  return P;

}

static void print_registered_functions(const char *key, void *value, void *cl) {
  printf("function: %s\n", key);
}

ParseState_T *parse_file(LexState_T *L) {

  ParseState_T *P = init_parsestate(L);

  while (P->tok) {
    if (should_parse_if(P)) {
      parse_if(P);
    } else if (should_parse_while(P)) {
      parse_while(P);
    } else if (should_parse_for(P)) {
      parse_for(P);
    } else if (should_parse_return(P)) { 
      parse_return(P);
    } else if (should_parse_function(P)) {
      parse_function(P);
    } else if (should_parse_cfunction(P)) {
      parse_cfunction(P);
    } else if (should_parse_block(P)) {
      parse_block(P);
    } else if (should_leave_block(P)) {
      leave_block(P);
    } else if (should_parse_struct(P)) {
      parse_struct(P);
    } else if (should_parse_declaration(P)) {
      add_variable_to_block(P, parse_declaration(P));
      eat(P, ";");
    } else {
      parse_expression_node(P);
    }
  }
  
  printf("===== PHASE TWO: PARSER =====\n");
  astnode_print(P->root, 0);

  printf("registered functions:\n");
  hash_foreach(P->functions, print_registered_functions, NULL);
  printf("===============================\n\n\n");

  return P;

}

void parse_cleanup(ParseState_T **P) {

}

