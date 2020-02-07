#ifndef SPYRE_H
#define SPYRE_H

#include <inttypes.h>
#include <stdlib.h>

typedef struct SpyreAvailableAddress {
  size_t addr;
  struct SpyreAvailableAddress *next;
} SpyreAvailableAddress_T;

typedef struct SpyreMemoryMap {
  uint8_t **allocs;
  size_t capacity;
  size_t index; 
  SpyreAvailableAddress_T *avail;
} SpyreMemoryMap_T;

typedef struct SpyreState {
  SpyreMemoryMap_T *memory;
} SpyreState_T;

SpyreState_T *spyre_init();

#endif
