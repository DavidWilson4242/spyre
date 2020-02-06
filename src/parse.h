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

typedef enum ExpressionNodeType {
  EXP_UNARY,
  EXP_BINARY,
  EXP_INTEGER,
  EXP_FLOAT
} ExpressionNodeType_T;

typedef enum LeafSide {
  LEAF_NA,
  LEAF_LEFT,
  LEAF_RIGHT
} LeafSide_T;

typedef struct Datatype {
  char *type_name;          /* typename id */ 
  unsigned arrdim;          /* array dimension */
  unsigned ptrdim;          /* pointer dimension */
  unsigned primsize;        /* size in bytes.  N/A if not primitive */
  bool is_const;

  struct Datatype *members; /* struct if valid. primitive if NULL */
  struct Datatype *next;    /* if I'm a member, pointer to next member */
} Datatype_T;

typedef struct BuiltinTypes {
  Datatype_T *int_t;
  Datatype_T *float_t;
  Datatype_T *char_t;
} BuiltinTypes_T;

typedef struct BinaryOpNode {
  struct ExpressionNode *left_operand;
  struct ExpressionNode *right_operand;
  uint8_t optype;
} BinaryOpNode_T;

typedef struct UnaryOpNode {
  struct ExpressionNode *operand;
  uint8_t optype;
} UnaryOpNode_T;

typedef struct ExpressionNode {
  ExpressionNodeType_T type;
  struct ExpressionNode *parent;
  LeafSide_T leaf;
  union {
    int64_t ival;
    double fval;
    BinaryOpNode_T *binop;
    UnaryOpNode_T *unop;
  };
} ExpressionNode_T;

typedef struct ExpressionStack {
  ExpressionNode_T *node;
  struct ExpressionStack *next;
  struct ExpressionStack *prev;
} ExpressionStack_T;

typedef struct Expression {

} Expression_T;

typedef struct NodeWhile {
  Expression_T *cond;
} NodeWhile_T;

typedef struct NodeIf {
  Expression_T *cond;
} NodeIf_T;

typedef struct ASTNode {
  ASTNodeType_T type;
  void *node;
  struct ASTNode *next;
} ASTNode_T;

typedef struct ParseState {
  LexState_T *L;
  LexToken_T *tok;
  LexToken_T *mark;
  BuiltinTypes_T *builtin;
  ASTNode_T *root;
} ParseState_T;

ParseState_T *parse_file(LexState_T *);
void parse_cleanup(ParseState_T **);

#endif
