#ifndef SPYRE_H
#define SPYRE_H

#include <inttypes.h>
#include <stdlib.h>
#include "hash.h"

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

typedef struct SpyreInternalType {
  char *type_name;
  size_t size;
  size_t nmembers;
  char **members;
} SpyreInternalType_T;

typedef struct SpyreState {
  SpyreMemoryMap_T *memory;
  SpyreHash_T *internal_types;
} SpyreState_T;

SpyreState_T *spyre_init();

#endif
