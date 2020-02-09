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

/* local management */
#define INS_LDL     0x80 /* load local */
#define INS_SVL     0x81 /* save TS to local */
#define INS_DER     0x82 /* get first word of segment on TS */
#define INS_RESL    0x83 /* reserve locals */
#define INS_LDMBR   0x84 /* load member of segment on TS*/
#define INS_SVMBR   0x85 /* save value TS of segment TS-1 member */

/* debug */
#define INS_IPRINT  0x90
#define INS_FPRINT  0x91
#define INS_PPRINT  0x92

/* memory management and GC */
#define INS_ALLOC   0xA0
#define INS_FREE    0xA1
#define INS_TAGL    0xA2
#define INS_UNTAGL  0xA3
#define INS_UNTAGLS 0xA4

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

typedef struct SpyreState {
  SpyreMemoryMap_T *memory;
  SpyreHash_T *internal_types;
  uint8_t *stack;
  uint8_t *code;
  size_t sp;
  size_t bp;
  size_t ip;
} SpyreState_T;

SpyreState_T *spyre_init();
void spyre_execute_file(const char *);
void spyre_execute_with_context(const char *, ParseState_T *);
void spyre_assert(bool);
size_t spyre_local_asptr(SpyreState_T *, size_t);
SpyreInternalType_T *get_type(SpyreState_T *, const char *);

#endif
