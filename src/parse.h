#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

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
    NODE_DECLARATION
} ASTNodeType_T;

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

typedef struct Expression {

} Expression_T;

typedef struct NodeExpression {

} NodeExpression_T;

typedef struct NodeWhile {

} NodeWhile_T;

typedef struct NodeIf {

} NodeIf_T;

typedef struct ASTNode {
    ASTNodeType_T type;
    void *node;
} ASTNode_T;

typedef struct ParseState {
    LexState_T *L;
    LexToken_T *tok;
    BuiltinTypes_T *builtin;
} ParseState_T;

ParseState_T *parse_file(LexState_T *);
void parse_cleanup(ParseState_T **);

#endif
