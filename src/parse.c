#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "parse.h"

#define INTTYPE_NAME  "int"
#define FLTTYPE_NAME  "float"
#define CHARTYPE_NAME "char"

#define INTTYPE_SIZE  8
#define FLTTYPE_SIZE  8
#define CHARTYPE_SIZE 1

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

static inline LexToken_T *at(ParseState_T *P) {
    return P->tok;
}

static inline LexToken_T *advance(ParseState_T *P, int n) {
    for (int i = 0; i < n && P->tok != NULL; i++) {
        P->tok = P->tok->next;
    }
}

static ParseState_T *init_parsestate(LexState_T *L) {

    ParseState_T *P = malloc(sizeof(ParseState_T));
    assert(P);

    /* init default datatypes */
    P->builtin = malloc(sizeof(BuiltinTypes_T));
    assert(P->builtin);
    P->builtin->int_t   = make_datatype(INTTYPE_NAME, 0, 0, INTTYPE_SIZE, false);
    P->builtin->float_t = make_datatype(FLTTYPE_NAME, 0, 0, FLTTYPE_SIZE, false);
    P->builtin->char_t  = make_datatype(CHARTYPE_NAME, 0, 0, CHARTYPE_SIZE, false);
    
    P->tok = L->tokens;

}

ParseState_T *parse_file(LexState_T *L) {
   
}

void parse_cleanup(ParseState_T **P) {

}
