#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "spyre.h"
#include "hash.h"

#define MEMORY_INITIAL_CAPACITY 128

/* the first bytes in any spyre allocated buffer contains this struct
 * at its head.  contains information about the size of the buffer,
 * typename (in the future), etc */
typedef struct MemoryDescriptor {
  char *type_name;
  size_t arrdim;
  size_t *arrs;
  size_t ptrdim;
} MemoryDescriptor_T;

static void spy_assert(bool cond) {
  if (!cond) {
    fprintf(stderr, "SPYRE CRITICAL: out of memory");
    exit(EXIT_FAILURE);
  }
}

static void init_types(SpyreState_T *S) {

  SpyreInternalType_T *type_int,
                      *type_float,
                      *type_bool;

  S->internal_types = hash_init();
  
  type_int = malloc(sizeof(SpyreInternalType_T));
  spy_assert(type_int);
  type_int->type_name = "int";
  type_int->size = 8;
  type_int->nmembers = 0;
  type_int->members = NULL;

  type_float = malloc(sizeof(SpyreInternalType_T));
  spy_assert(type_float);
  type_float->type_name = "float";
  type_float->size = 8;
  type_float->nmembers = 0;
  type_float->members = NULL;

  type_bool = malloc(sizeof(SpyreInternalType_T));
  spy_assert(type_bool);
  type_bool->type_name = "bool";
  type_bool->size = 8;
  type_bool->nmembers = 0;
  type_bool->members = NULL;

  hash_insert(S->internal_types, type_int->type_name, type_int);
  hash_insert(S->internal_types, type_float->type_name, type_float);
  hash_insert(S->internal_types, type_bool->type_name, type_bool);
}

int64_t spyre_pop_int(SpyreState_T *S) {
  return 0;  
}

static const MemoryDescriptor_T *spyre_typedata(SpyreState_T *S, size_t addr) {
  uint8_t *rawbuf = S->memory->allocs[addr];
  if (!rawbuf) {
    return NULL;
  }
  return (MemoryDescriptor_T *)&rawbuf[0];
}

size_t spyre_alloc(SpyreState_T *S, MemoryDescriptor_T *memdesc) {

  size_t index;
  uint8_t *rawbuf, *buffer;
  uint8_t **newallocs;
  MemoryDescriptor_T *desc;
  SpyreAvailableAddress_T *addr;
  SpyreInternalType_T *type = hash_get(S->internal_types, memdesc->type_name);

  if (!type) {
    fprintf(stderr, "invalid typename '%s'\n", memdesc->type_name);
    exit(EXIT_FAILURE);
  }

  rawbuf = malloc(sizeof(uint8_t)*type->size + sizeof(MemoryDescriptor_T));
  spy_assert(rawbuf != NULL);
  desc = (MemoryDescriptor_T *)&rawbuf[0];
  buffer = &rawbuf[sizeof(MemoryDescriptor_T)];
  
  memcpy(desc, memdesc, sizeof(MemoryDescriptor_T));

  /* if there's a deallocated index, use that */
  if (S->memory->avail) {
    addr = S->memory->avail;
    index = addr->addr;
    S->memory->avail = addr->next;
    free(addr);
  } else {
    index = S->memory->index++;
  }
  
  /* grow index table if necessary */
  if (index >= S->memory->capacity) {
    newallocs = calloc(1, sizeof(uint8_t *) * (S->memory->capacity*2 + 2)); 
    spy_assert(newallocs != NULL);
    memcpy(newallocs, S->memory->allocs, sizeof(uint8_t *) * S->memory->capacity);
    free(S->memory->allocs);
    S->memory->allocs = newallocs;
    S->memory->capacity = S->memory->capacity*2 + 2;
  }

  S->memory->allocs[index] = rawbuf;

  return index;
}

void spyre_free(SpyreState_T *S, size_t index) {
  if (index >= S->memory->capacity || !S->memory->allocs[index]) {
    fprintf(stderr, "invalid free");
    exit(EXIT_FAILURE);
  }
  free(S->memory->allocs[index]);
  S->memory->allocs[index] = NULL;

  /* index is available for reallocation */
  SpyreAvailableAddress_T *addr = malloc(sizeof(SpyreAvailableAddress_T));
  spy_assert(addr != NULL);
  addr->addr = index;
  addr->next = NULL;
  
  if (!S->memory->avail) {
    S->memory->avail = addr;
  } else {
    addr->next = S->memory->avail;
    S->memory->avail = addr;
  }
}

SpyreState_T *spyre_init() {

  SpyreState_T *S = malloc(sizeof(SpyreState_T));
  spy_assert(S != NULL);
  S->memory = malloc(sizeof(SpyreMemoryMap_T));
  spy_assert(S->memory);
  S->memory->index = 1;
  S->memory->capacity = MEMORY_INITIAL_CAPACITY;
  S->memory->avail = NULL;
  S->memory->allocs = calloc(1, sizeof(uint8_t *) * MEMORY_INITIAL_CAPACITY);
  spy_assert(S->memory->allocs);
  
  init_types(S);
  
  MemoryDescriptor_T desc;
  desc.type_name = "int";
  desc.arrdim = 0;
  desc.arrs = NULL;
  desc.ptrdim = 0;
  size_t index = spyre_alloc(S, &desc);
  
  const MemoryDescriptor_T *got = spyre_typedata(S, index);
  printf("%s\n", got->type_name);

  return S;

}
