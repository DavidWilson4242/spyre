#ifndef GEN_H
#define GEN_H

#include <stdlib.h>
#include "parse.h"

typedef struct GenerateState {
  ParseState_T *P;
  ASTNode_T *at;
  FILE *outfile;
  size_t lcount;
} GenerateState_T;

void generate_bytecode(ParseState_T *P, char *);

#endif
