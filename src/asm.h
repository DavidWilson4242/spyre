#ifndef ASM_H
#define ASM_H

#include <stdlib.h>
#include "lex.h"
#include "hash.h"

typedef struct PendingLabel {
  char *label_name;
  size_t bufaddr;
  struct PendingLabel *next;
} PendingLabel_T;

typedef struct AssembleState {
  LexState_T *L;
  LexToken_T *at;
  SpyreHash_T *labels;
  PendingLabel_T *pending;
  FILE *outfile; 
  uint8_t *writebuf;
  size_t sizebuf;
  size_t bufat;
} AssembleState_T;

void assemble_file(const char *, const char *);

#endif
