#ifndef MEMORY_H
#define MEMORY_H

#include "spyre.h"

size_t spymem_alloc(SpyreState_T *, MemoryDescriptor_T *);
uint8_t *spymem_rawbuf(SpyreState_T *, size_t);
void spymem_free(SpyreState_T *, size_t);

#endif
