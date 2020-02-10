#ifndef PARSE_H
#define PARSE_H

#include "lex.h"
#include "hash.h"

struct ASTNode;
struct FunctionDescriptor;
struct StructDescriptor;

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
  EXP_FLOAT,
  EXP_VARIABLE
} NodeExpressionType_T;

typedef enum DatatypeType {
  DT_PRIMITIVE,
  DT_STRUCT,
  DT_FUNCTION
} DatatypeType_T;

typedef enum LeafSide {
  LEAF_NA,
  LEAF_LEFT,
  LEAF_RIGHT
} LeafSide_T;

typedef struct Datatype {
  char *type_name;          /* type_name id */ 
  unsigned arrdim;          /* array dimension */
  unsigned ptrdim;          /* pointer dimension */
  unsigned primsize;        /* size in bytes.  N/A if not primitive */
  bool is_const;
  DatatypeType_T type;
  
  union {
    struct FunctionDescriptor_T *fdesc;
    struct StructDescriptor_T   *sdesc;
  };
  struct Datatype *next;    /* if I'm a member, pointer to next member */
} Datatype_T;

typedef struct Declaration {
  char *name;
  Datatype_T *dt;
  struct Declaration *next;
  size_t local_index;
} Declaration_T;

typedef struct FunctionDescriptor {
  Declaration_T *arguments;
  Datatype_T *return_type;
} FunctionDescriptor_T;

typedef struct StructDescriptor_T {
  SpyreHash_T *members;
} StructDescriptor_T;

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
    Declaration_T *decl;
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
  Declaration_T *vars;
  Declaration_T *backvar;
} NodeBlock_T;

typedef struct NodeReturn {
  NodeExpression_T *retval;
} NodeReturn_T;

typedef struct NodeFunction {
  char *func_name;
  Declaration_T *args;
  NodeExpression_T *special_ret;
  Datatype_T *rettype;
  size_t stack_space;
} NodeFunction_T;

typedef struct ASTNode {
  ASTNodeType_T type;
  struct ASTNode *next;
  struct ASTNode *prev;
  struct ASTNode *parent;
  union {
    NodeIf_T         *nodeif;
    NodeWhile_T      *nodewhile;
    NodeExpression_T *nodeexp;
    NodeBlock_T      *nodeblock;
    NodeReturn_T     *noderet;
    NodeFunction_T   *nodefunc;
  };
} ASTNode_T;

typedef struct ParseState {
  LexState_T *L;
  LexToken_T *tok;
  LexToken_T *mark;
  BuiltinTypes_T *builtin;
  SpyreHash_T *usertypes;
  ASTNode_T *root;
  ASTNode_T *block;
  ASTNode_T *backnode;
} ParseState_T;

ParseState_T *parse_file(LexState_T *);
void parse_cleanup(ParseState_T **);

#endif
