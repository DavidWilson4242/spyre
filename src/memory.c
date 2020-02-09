#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"

uint8_t *spymem_rawbuf(SpyreState_T *S, size_t seg_id) {
	
	if (S->memory->allocs[seg_id] == NULL) {
		return NULL;
	}

	return &S->memory->allocs[seg_id][sizeof(MemoryDescriptor_T)];

}

size_t spymem_alloc(SpyreState_T *S, MemoryDescriptor_T *memdesc) {

  size_t index;
  size_t total_size;
  uint8_t *rawbuf, *buffer;
  uint8_t **newallocs;
  MemoryDescriptor_T *desc;
  SpyreAddressList_T *addr;
  SpyreInternalType_T *type = hash_get(S->internal_types, memdesc->type_name);

  if (!type) {
    fprintf(stderr, "invalid typename '%s'\n", memdesc->type_name);
    exit(EXIT_FAILURE);
  }

  total_size = (type->nmembers > 0 ? type->nmembers : 1)*8;

#ifdef DEBUG
  printf("allocating %zu bytes for type %s\n", total_size, type->type_name);
#endif
  
  /* TODO account for array dimensionality */
  rawbuf = calloc(1, sizeof(uint8_t)*total_size + sizeof(MemoryDescriptor_T));
  spyre_assert(rawbuf != NULL);
  desc = (MemoryDescriptor_T *)&rawbuf[0];
  buffer = &rawbuf[sizeof(MemoryDescriptor_T)];
  
  /* TODO deepcopy */
  memcpy(desc, memdesc, sizeof(MemoryDescriptor_T));
  desc->mark = false;

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
    spyre_assert(newallocs != NULL);
    memcpy(newallocs, S->memory->allocs, sizeof(uint8_t *) * S->memory->capacity);
    free(S->memory->allocs);
    S->memory->allocs = newallocs;
    S->memory->capacity = S->memory->capacity*2 + 2;
  }

  S->memory->allocs[index] = rawbuf;

  return index;
}

void spymem_free(SpyreState_T *S, size_t seg_id) {
  if (seg_id >= S->memory->capacity || !S->memory->allocs[seg_id]) {
    fprintf(stderr, "invalid free");
    exit(EXIT_FAILURE);
  }
  free(S->memory->allocs[seg_id]);
  S->memory->allocs[seg_id] = NULL;

  /* seg_id is available for reallocation */
  SpyreAddressList_T *addr = malloc(sizeof(SpyreAddressList_T));
  spyre_assert(addr != NULL);
  addr->addr = seg_id;
  addr->next = NULL;
  
  if (!S->memory->avail) {
    S->memory->avail = addr;
  } else {
    addr->next = S->memory->avail;
    S->memory->avail = addr;
  }

}

