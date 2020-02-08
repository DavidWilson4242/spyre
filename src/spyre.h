#ifndef SPYRE_H
#define SPYRE_H

#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include "hash.h"

#define INS_IPUSH 0x01
#define INS_IPOP  0x02

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
  size_t totmembers;
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
void spyre_assert(bool);
size_t spyre_local_asptr(SpyreState_T *, size_t);
size_t spyre_alloc(SpyreState_T *, MemoryDescriptor_T *);
void spyre_free(SpyreState_T *, size_t);
SpyreInternalType_T *get_type(SpyreState_T *, const char *);

#endif

/* 
 size_t a0, a1;

 SpyreInternalMember_T *int0 = malloc(sizeof(SpyreInternalMember_T));
 int0->type = get_type(S, "int");
 int0->ptrdim = 0;
 int0->arrdim = 0;
 int0->byte_offset = 0;

 SpyreInternalMember_T *int1 = malloc(sizeof(SpyreInternalMember_T));
 int1->type = get_type(S, "int");
 int1->ptrdim = 0;
 int1->arrdim = 0;
 int1->byte_offset = 8;

 SpyreInternalType_T *vec2 = malloc(sizeof(SpyreInternalType_T));
 vec2->type_name = "Vector2";
 vec2->nmembers = 2;
 vec2->members = malloc(sizeof(SpyreInternalMember_T) * 2);
 vec2->members[0] = int0;
 vec2->members[1] = int1;
 register_type(S, vec2);

 SpyreInternalMember_T *v0 = malloc(sizeof(SpyreInternalMember_T));
 v0->type = get_type(S, "Vector2");
 v0->ptrdim = 0;
 v0->arrdim = 0;
 v0->byte_offset = 0;

 SpyreInternalMember_T *v1 = malloc(sizeof(SpyreInternalMember_T));
 v1->type = get_type(S, "Vector2");
 v1->ptrdim = 0;
 v1->arrdim = 0;
 v1->byte_offset = 8;

 SpyreInternalType_T *matrix = malloc(sizeof(SpyreInternalType_T));
 matrix->type_name = "Matrix";
 matrix->nmembers = 2;
 matrix->members = malloc(sizeof(SpyreInternalMember_T) * 2);
 matrix->members[0] = v0;
 matrix->members[1] = v1;
 register_type(S, matrix);

 MemoryDescriptor_T desc;
 desc.type_name = "Matrix";
 desc.arrdim = 0;
 desc.arrs = NULL;
 desc.ptrdim = 0;

 a0 = spyre_alloc(S, &desc);
 spyre_push_ptr(S, a0);
 gc_track_local(S, 0);

 gc_execute(S);
*/
