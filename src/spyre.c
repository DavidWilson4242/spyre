#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "spyre.h"

#define MEMORY_INITIAL_CAPACITY 128

/* the first bytes in any spyre allocated buffer contains this struct
 * at its head.  contains information about the size of the buffer,
 * typename (in the future), etc */
typedef struct MemoryDescriptor {
  size_t bufsize;
} MemoryDescriptor_T;

static void spy_assert(bool cond) {
  if (!cond) {
    fprintf(stderr, "SPYRE CRITICAL: out of memory");
    exit(EXIT_FAILURE);
  }
}

int64_t spyre_pop_int(SpyreState_T *S) {
  return 0;  
}

size_t spyre_alloc(SpyreState_T *S, size_t bytes) {

  size_t index;
  uint8_t *rawbuf, *buffer;
  uint8_t **newallocs;
  MemoryDescriptor_T *desc;
  SpyreAvailableAddress_T *addr;

  rawbuf = malloc(sizeof(uint8_t)*bytes + sizeof(MemoryDescriptor_T));
  spy_assert(rawbuf != NULL);
  desc = (MemoryDescriptor_T *)&rawbuf[0];
  buffer = &rawbuf[sizeof(MemoryDescriptor_T)];

  desc->bufsize = bytes;

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
  S->memory->index = 0;
  S->memory->capacity = MEMORY_INITIAL_CAPACITY;
  S->memory->avail = NULL;
  S->memory->allocs = calloc(1, sizeof(uint8_t *) * MEMORY_INITIAL_CAPACITY);
  spy_assert(S->memory->allocs);

  return S;

}
