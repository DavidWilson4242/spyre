#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

struct ASTNode;

typedef enum ASTNodeType {
  NODE_UNDEFINED = 0,
  NODE_IF,
  NODE_WHILE,
  NODE_FOR,
  NODE_FUNCTION,
  NODE_EXPRESSION,
  NODE_RETURN,
  NODE_CONTINUE,
  NODE_INCLUDE,
  NODE_DECLARATION,
  NODE_BLOCK
} ASTNodeType_T;

typedef enum NodeExpressionType {
  EXP_UNARY,
  EXP_BINARY,
  EXP_INTEGER,
  EXP_FLOAT
} NodeExpressionType_T;

typedef enum LeafSide {
  LEAF_NA,
  LEAF_LEFT,
  LEAF_RIGHT
} LeafSide_T;

typedef struct Datatype {
  char *typename;          /* typename id */ 
  unsigned arrdim;          /* array dimension */
  unsigned ptrdim;          /* pointer dimension */
  unsigned primsize;        /* size in bytes.  N/A if not primitive */
  bool is_const;

  struct Datatype *members; /* struct if valid. primitive if NULL */
  struct Datatype *next;    /* if I'm a member, pointer to next member */
} Datatype_T;

typedef struct Declaration {
  char *name;
  Datatype_T *dt;
} Declaration_T;

typedef struct BuiltinTypes {
  Datatype_T *int_t;
  Datatype_T *float_t;
  Datatype_T *char_t;
  Datatype_T *bool_t;
} BuiltinTypes_T;

typedef struct BinaryOpNode {
  struct NodeExpression *left_operand;
  struct NodeExpression *right_operand;
  uint8_t optype;
} BinaryOpNode_T;

typedef struct UnaryOpNode {
  struct NodeExpression *operand;
  uint8_t optype;
} UnaryOpNode_T;

typedef struct NodeExpression {
  NodeExpressionType_T type;
  struct NodeExpression *parent;
  LeafSide_T leaf;
  union {
    int64_t ival;
    double fval;
    BinaryOpNode_T *binop;
    UnaryOpNode_T *unop;
  };
} NodeExpression_T;

typedef struct ExpressionStack {
  NodeExpression_T *node;
  struct ExpressionStack *next;
  struct ExpressionStack *prev;
} ExpressionStack_T;

typedef struct NodeWhile {
  NodeExpression_T *cond;
} NodeWhile_T;

typedef struct NodeIf {
  NodeExpression_T *cond;
} NodeIf_T;

typedef struct NodeBlock {
  struct ASTNode *children;
} NodeBlock_T;

typedef struct ASTNode {
  ASTNodeType_T type;
  struct ASTNode *next;
  struct ASTNode *parent;
  union {
    NodeIf_T         *nodeif;
    NodeWhile_T      *nodewhile;
    NodeExpression_T *nodeexp;
    NodeBlock_T      *nodeblock;
  };
} ASTNode_T;

typedef struct ParseState {
  LexState_T *L;
  LexToken_T *tok;
  LexToken_T *mark;
  BuiltinTypes_T *builtin;
  ASTNode_T *root;
  ASTNode_T *block;
  ASTNode_T *backnode;
} ParseState_T;

ParseState_T *parse_file(LexState_T *);
void parse_cleanup(ParseState_T **);

#endif
