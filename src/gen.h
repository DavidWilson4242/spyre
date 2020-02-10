#ifndef GEN_H
#define GEN_H

#include <stdlib.h>
#include "parse.h"

typedef struct GenerateState {
  ParseState_T *P;
} GenerateState_T;

void generate_bytecode(ParseState_T *P);

#endif
