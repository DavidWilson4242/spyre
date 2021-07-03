#ifndef SPYRE_H
#define SPYRE_H

#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include "hash.h"
#include "parse.h"

#define DEBUG

#define INS_HALT    0x00

/* integer arithmetic */
#define INS_IPUSH   0x01
#define INS_IPOP    0x02
#define INS_IADD    0x03
#define INS_ISUB    0x04
#define INS_IMUL    0x05
#define INS_IDIV    0x06

/* misc */
#define INS_DUP     0x20

/* flags */
#define INS_FEQ     0x30
#define INS_FLE     0x31
#define INS_FGE     0x32
#define INS_FLT     0x33
#define INS_FGT     0x34

/* local management */
#define INS_LDL     0x80 /* load local */
#define INS_SVL     0x81 /* save TS to local */
#define INS_DER     0x82 /* get first word of segment on TS */
#define INS_RESL    0x83 /* reserve locals */
#define INS_LDMBR   0x84 /* load member of segment on TS*/
#define INS_SVMBR   0x85 /* save value TS of segment TS-1 member */
#define INS_ARG     0x86 /* loads argument onto TS */
#define INS_SVLS    0x87 /* save TS to local at addr [TS-1] */

/* debug */
#define INS_IPRINT  0x90
#define INS_FPRINT  0x91
#define INS_PPRINT  0x92
#define INS_FLAGS   0x93

/* memory management and GC */
#define INS_ALLOC   0xA0
#define INS_FREE    0xA1
#define INS_TAGL    0xA2
#define INS_UNTAGL  0xA3
#define INS_UNTAGLS 0xA4

/* branching */
#define INS_ITEST   0xC0
#define INS_ICMP    0xC1
#define INS_FTEST   0xC2
#define INS_FCMP    0xC3
#define INS_JMP     0xC4
#define INS_JZ      0xC5
#define INS_JNZ     0xC6
#define INS_JGT     0xC7
#define INS_JGE     0xC8
#define INS_JLT     0xC9
#define INS_JLE     0xCA
#define INS_JEQ     0xCB
#define INS_JNEQ    0xCC
#define INS_CALL    0xCD
#define INS_CCALL   0xCE
#define INS_IRET    0xCF
#define INS_RET     0xD0

struct SpyreState;

/* at the head of every segment allocation */
typedef struct MemoryDescriptor {
  char *type_name;
  size_t arrdim;
  size_t *arrs;
  size_t ptrdim;
  bool mark;
} MemoryDescriptor_T;

typedef struct SpyreAddressList {
  size_t addr;
  struct SpyreAddressList *next;
} SpyreAddressList_T;

typedef struct SpyreMemoryMap {

  /* heap management */
  uint8_t **allocs;
  size_t capacity;
  size_t index; 
  SpyreAddressList_T *avail;

  /* garbage collection */
  SpyreAddressList_T *localtags;
} SpyreMemoryMap_T;

typedef struct SpyreInternalMember {
  struct SpyreInternalType *type;
  size_t ptrdim;
  size_t arrdim;
  size_t byte_offset;
} SpyreInternalMember_T;

typedef struct SpyreInternalType {
  char *type_name;
  size_t nmembers;
  SpyreInternalMember_T **members;
} SpyreInternalType_T;

typedef struct SpyreFunction {
  char *name;
  int (*func)(struct SpyreState *);
} SpyreFunction_T;

typedef struct SpyreState {
  SpyreMemoryMap_T *memory;
  SpyreHash_T *internal_types;
  SpyreHash_T *cfuncs;
  uint8_t *stack;
  uint8_t *code;
  size_t sp;
  size_t bp;
  size_t ip;

  /* flags */
  uint8_t fz;
  uint8_t feq;
  uint8_t fgt;
  uint8_t fge;
} SpyreState_T;

SpyreState_T *spyre_init();
void spyre_execute_file(const char *);
void spyre_execute_with_context(const char *, ParseState_T *);
void spyre_assert(bool);
void spyre_register_cfunc(SpyreState_T *, const char *, int (*)(SpyreState_T *));
size_t spyre_local_asptr(SpyreState_T *, size_t);
int64_t spyre_pop_int(SpyreState_T *S);
SpyreInternalType_T *get_type(SpyreState_T *, const char *);

#endif
