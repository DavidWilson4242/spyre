#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include "lex.h"
#include "parse.h"

#define INTTYPE_NAME  "int"
#define FLTTYPE_NAME  "float"
#define CHARTYPE_NAME "char"

#define INTTYPE_SIZE  8
#define FLTTYPE_SIZE  8
#define CHARTYPE_SIZE 1

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

static const OperatorDescriptor_T prec_table[255] = {
	[',']				      = {1,  ASSOC_LEFT,  OPERAND_BINARY},
	['=']				      = {2,  ASSOC_RIGHT, OPERAND_BINARY},
	[SPECO_INC_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
	[SPECO_DEC_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
	[SPECO_MUL_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
	[SPECO_DIV_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
	[SPECO_MOD_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
	[SPECO_SHL_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
	[SPECO_SHR_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
	[SPECO_AND_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
	[SPECO_OR_BY]		  = {2,  ASSOC_RIGHT, OPERAND_BINARY},
	[SPECO_XOR_BY]		= {2,  ASSOC_RIGHT, OPERAND_BINARY},
	[SPECO_LOG_AND]	  = {3,  ASSOC_LEFT,  OPERAND_BINARY},
	[SPECO_LOG_OR]		= {3,  ASSOC_LEFT,  OPERAND_BINARY},
	[SPECO_EQ]			  = {4,  ASSOC_LEFT,  OPERAND_BINARY},
	[SPECO_NEQ]			  = {4,  ASSOC_LEFT,  OPERAND_BINARY},
	['>']				      = {6,  ASSOC_LEFT,  OPERAND_BINARY},
	[SPECO_GE]			  = {6,  ASSOC_LEFT,  OPERAND_BINARY},
	['<']				      = {6,  ASSOC_LEFT,  OPERAND_BINARY},
	[SPECO_LE]			  = {6,  ASSOC_LEFT,  OPERAND_BINARY},
	['|']				      = {7,  ASSOC_LEFT,  OPERAND_BINARY},
	[SPECO_SHL]			  = {7,  ASSOC_LEFT,  OPERAND_BINARY},
	[SPECO_SHR]			  = {7,  ASSOC_LEFT,  OPERAND_BINARY},
	['+']				      = {8,  ASSOC_LEFT,  OPERAND_BINARY},
	['-']				      = {8,  ASSOC_LEFT,  OPERAND_BINARY},
	['*']				      = {9,  ASSOC_LEFT,  OPERAND_BINARY},
	['%']				      = {9,  ASSOC_LEFT,  OPERAND_BINARY},
	['/']				      = {9,  ASSOC_LEFT,  OPERAND_BINARY},
	['@']				      = {10, ASSOC_RIGHT, OPERAND_UNARY},
	['$']				      = {10, ASSOC_RIGHT, OPERAND_UNARY},
	['!']				      = {10, ASSOC_RIGHT, OPERAND_UNARY},
	[SPECO_TYPENAME]	= {10, ASSOC_RIGHT, OPERAND_UNARY},
	[SPECO_CAST]			= {10, ASSOC_RIGHT, OPERAND_UNARY},
	['.']				      = {11, ASSOC_LEFT,  OPERAND_BINARY},
	[SPECO_INC_ONE]	  = {11, ASSOC_LEFT,  OPERAND_UNARY},
	[SPECO_DEC_ONE]	  = {11, ASSOC_LEFT,  OPERAND_UNARY},
	[SPECO_CALL]			= {11, ASSOC_LEFT,  OPERAND_UNARY},
	[SPECO_INDEX]		  = {11, ASSOC_LEFT,  OPERAND_UNARY}
};

static void indent(size_t n) {
  for (size_t i = 0; i < n; i++) {
    printf("\t");
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
  dt->members  = NULL;
  dt->next     = NULL;

  return dt;

}

/* does not clone members.  leaves as NULL.  this is used
 * to clone the builtin primitive types, and is not meant
 * for cloning datatypes with members */
static Datatype_T *clone_datatype(Datatype_T *dt) {
  return make_datatype(dt->type_name, dt->arrdim, dt->ptrdim,
                       dt->primsize, dt->is_const);
}

static ASTNode_T *empty_node(ASTNodeType_T type) {
  ASTNode_T *node = malloc(sizeof(ASTNode_T));
  assert(node);
  node->type = type;
  node->next = NULL;
  switch (type) {
    case NODE_IF:
      node->node = malloc(sizeof(NodeIf_T));
      break;
    case NODE_WHILE:
      node->node = malloc(sizeof(NodeWhile_T));
      break;
    case NODE_FOR:
    case NODE_FUNCTION:
    case NODE_EXPRESSION:
    case NODE_RETURN:
    case NODE_CONTINUE:
    case NODE_INCLUDE:
    case NODE_DECLARATION:
    default:
      node->node = NULL;
      break;
  }
  return node;
}

static inline LexToken_T *at(ParseState_T *P) {
  return P->tok;
}

static inline LexToken_T *advance(ParseState_T *P, int n) {
  for (int i = 0; i < n && P->tok != NULL; i++) {
    P->tok = P->tok->next;
  }
}

static LexToken_T *peek(ParseState_T *P, int n) {
  LexToken_T *token = P->tok;
  for (size_t i = 0; i < n; i++) {
    if (!token) {
      return NULL;
    }
    token = token->next;
  }
  return token;
}

static bool on_identifier(ParseState_T *P, const char *id) {
  if (!P->tok) {
    return false;
  }
  return !strcmp(P->tok->as_string, id);
}

static void eat(ParseState_T *P, const char *id) {
  if (!on_identifier(P, id)) {
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

static void expstack_push(ExpressionStack_T **stack, ExpressionNode_T *node) {
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

static ExpressionNode_T *expstack_pop(ExpressionStack_T **stack) {
  if (*stack == NULL) {
    return NULL;
  }
  
  ExpressionNode_T *node = (*stack)->node;
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

static ExpressionNode_T *expstack_top(ExpressionStack_T **stack) {
  if (*stack == NULL) {
    return NULL;
  }
  return (*stack)->node;
}

void expstack_print(ExpressionStack_T **stack) {
  for (ExpressionStack_T *s = *stack; s != NULL; s = s->next) {
    ExpressionNode_T *node = s->node;
    switch (node->type) {
      case EXP_INTEGER:
        printf("%ld ", node->ival);
        break;
      case EXP_BINARY:
        printf("%c ", node->binop->optype);
        break;
    }
  }
  printf("\n");
}

/* implementation of the shunting yard algorithm */
static void shunting_pops(ExpressionStack_T **postfix, ExpressionStack_T **operators,
                          const OperatorDescriptor_T *opdesc) {
  ExpressionNode_T *top;
  const OperatorDescriptor_T *top_desc;

  while (1) {
    top = expstack_top(operators);
    if (!top) {
      break;
    }

    switch (top->type) {
      case EXP_BINARY:
        top_desc = &prec_table[top->binop->optype];
        break;
      case EXP_UNARY:
        top_desc = &prec_table[top->unop->optype];
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

static void expnode_print(ExpressionNode_T *node, size_t ind) {
  indent(ind);
  switch (node->type) {
    case EXP_INTEGER:
      break;
  }
}

static ExpressionNode_T *empty_expnode(ExpressionNodeType_T type) {
  ExpressionNode_T *node = malloc(sizeof(ExpressionNode_T));
  assert(node);
  node->parent = NULL;
  node->type = type;
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
      break;
    case EXP_UNARY:
      node->unop = malloc(sizeof(UnaryOpNode_T));
      assert(node->unop);
      node->unop->operand = NULL;
      break;
    default:
      break;
  }
  return node;
}

static ExpressionNode_T *parse_expression(ParseState_T *P) {

  ExpressionStack_T *operators = NULL;
  ExpressionStack_T *postfix   = NULL;
  ExpressionStack_T *tree      = NULL;
  ExpressionNode_T *node;
  ExpressionNode_T *top;
  const OperatorDescriptor_T *opinfo;
 
  /* ===== PHASE ONE | SHUNTING YARD ===== */

  /* implementation of the shunting yard algorithm.  converts the
   * expression from infix notation to postix notation */
  while (P->tok != P->mark) {
    LexToken_T *t = P->tok;
    switch (t->type) {
      case TOKEN_INTEGER:
        node = empty_expnode(EXP_INTEGER);
        node->ival = t->ival;
        expstack_push(&postfix, node);
        break;
      case TOKEN_FLOAT:
        node = empty_expnode(EXP_FLOAT);
        node->fval = t->fval;
        expstack_push(&postfix, node);
        break;
      case TOKEN_OPERATOR:
        if (prec_table[t->oval].prec) {
          opinfo = &prec_table[t->oval];
          shunting_pops(&postfix, &operators, opinfo);
          if (opinfo->optype) {
            node = empty_expnode(EXP_BINARY);
            node->binop->optype = t->oval;
          } else {
            node = empty_expnode(EXP_UNARY);
            node->unop->optype = t->oval;
          }
          expstack_push(&operators, node);
        } else if (t->oval == '(') {
          node = empty_expnode(EXP_UNARY);
          node->unop->optype = '(';
          expstack_push(&operators, node);
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
        break;
      default:
        parse_err(P, "unexpected token '%s' when parsing expression", t->as_string);
    }
    safe_eat(P);
  }
  
  /* empty remaining operator stack into postfix */
  while (expstack_top(&operators)) {
    ExpressionNode_T *node = expstack_pop(&operators);
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
  
  /* print the stack for debug */
  expstack_print(&postfix);

  const char *malformed_message = "malformed expression";

  /* ===== PHASE TWO | EXPRESSION TREE ===== */
  for (ExpressionStack_T *s = postfix; s != NULL; s = s->next) {
    node = s->node;
    switch (node->type) {
      case EXP_INTEGER:
      case EXP_FLOAT:
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
        ExpressionNode_T *leaves[2];

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

  ExpressionNode_T *final_value = expstack_pop(&tree);

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

static bool should_parse_if(ParseState_T *P) {
  return on_identifier(P, "if");
}

static void parse_if(ParseState_T *P) {
  eat(P, "if");
  eat(P, "(");
  mark_operator(P, '(', ')');
  ExpressionNode_T *ex = parse_expression(P);
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

  /* init abstract syntax tree with just a root.
   * the root is just a block, essentially parsing 
   * the entire file as: { ..file.. } */
  P->root = empty_node(NODE_BLOCK);

  P->tok = L->tokens;

  return P;

}

ParseState_T *parse_file(LexState_T *L) {

  ParseState_T *P = init_parsestate(L);

  while (P->tok) {
    if (should_parse_if(P)) {
      parse_if(P);
    }
    advance(P, 1);
  }

  printf("parsing complete\n");

  return P;

}

void parse_cleanup(ParseState_T **P) {

}
