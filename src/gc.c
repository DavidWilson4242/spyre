#include <stdio.h>
#include <stdlib.h>
#include "gc.h"
#include "memory.h"

/* this file contains all functions related to garbage collection.  It implements
 * the two main stages of garbage collection: mark and sweep */

static void domark(SpyreState_T *S, size_t seg_id) {
	uint8_t *rawbuf;
  uint8_t **allocs = S->memory->allocs;
  MemoryDescriptor_T *mdesc = (MemoryDescriptor_T *)&allocs[seg_id][0];
  SpyreInternalType_T *typeinfo = get_type(S, mdesc->type_name);
  SpyreInternalMember_T *member;
  SpyreInternalType_T *meminfo;
  size_t mem_seg_id;

	/* base case */
	if (mdesc->mark) {
		return;
	}
  
  mdesc->mark = true;
  printf("marked seg_id %zu\n", seg_id);
  
  for (size_t i = 0; i < typeinfo->nmembers; i++) {
		rawbuf = spymem_rawbuf(S, seg_id);
    member = typeinfo->members[i];
    meminfo = member->type;
    if (meminfo->nmembers > 0) {
      mem_seg_id = *(size_t *)&rawbuf[member->byte_offset];
      if (mem_seg_id != 0) {
        domark(S, mem_seg_id);
      }
    }
  }
  
}

/* iterate through all memory segments and unmark */
static void unmark(SpyreState_T *S) {
  size_t capacity = S->memory->capacity;
  uint8_t **allocs = S->memory->allocs;
  MemoryDescriptor_T *typeinfo;
  for (size_t i = 0; i < capacity; i++) {
    if (allocs[i] != NULL) {
      typeinfo = (MemoryDescriptor_T *)&allocs[i][0]; 
      if (typeinfo->mark) {
        printf("unmarked seg_id %zu\n", i);
      }
      typeinfo->mark = false;
    }
  }
}

static void mark(SpyreState_T *S) {
  SpyreAddressList_T *addr;
  for (addr = S->memory->localtags; addr != NULL; addr = addr->next) {
    size_t seg_id = spyre_local_asptr(S, addr->addr); 
    domark(S, seg_id);
  }
}

static void sweep(SpyreState_T *S) {
  size_t capacity = S->memory->capacity;
  uint8_t **allocs = S->memory->allocs;
  MemoryDescriptor_T *typeinfo;
  for (size_t i = 0; i < capacity; i++) {
    if (allocs[i] != NULL) {
      typeinfo = (MemoryDescriptor_T *)&allocs[i][0]; 
      if (!typeinfo->mark) {
        spymem_free(S, i);
        printf("freed seg_id %zu\n", i);
      }
    }
  }
}

void spygc_execute(SpyreState_T *S) {
  printf("******** GC RUNNING ********\n");
  printf("==== UNMARKING ====\n");
  unmark(S);
  printf("===================\n\n");
  printf("==== MARKING ====\n");
  mark(S);
  printf("=================\n\n");
  printf("==== SWEEPING ====\n");
  sweep(S);
  printf("==================\n");
  printf("****************************\n\n");
}

void spygc_track_local(SpyreState_T *S, size_t local_index) {
  size_t stack_addr = S->bp + local_index;
  SpyreAddressList_T *localtag = malloc(sizeof(SpyreAddressList_T));
  spyre_assert(localtag != NULL);
  localtag->addr = stack_addr;

  /* append to root node list */
  if (S->memory->localtags != NULL) {
    localtag->next = S->memory->localtags;
    S->memory->localtags = localtag;
  } else {
    S->memory->localtags = localtag;
    localtag->next = NULL;
  }
}

/* untags one local variable */
void spygc_untrack_local(SpyreState_T *S, size_t local_index) {
	SpyreAddressList_T *prev = NULL;
	for (SpyreAddressList_T *a = S->memory->localtags; a != NULL; a = a->next) {
		if (a->addr == local_index) {
			if (prev != NULL) {
				prev->next = a->next;
			} else {
				S->memory->localtags = a->next;
			}
			free(a);
			break;
		}
		prev = a;
	}
}

/* untags local variables from root node stack */
void spygc_untrack_locals(SpyreState_T *S, size_t num_locals) {
  SpyreAddressList_T *pop;
  for (size_t i = 0; i < num_locals; i++) {
    if (!S->memory->localtags) {
      fprintf(stderr, "garbage collection mismatch\n");
      exit(EXIT_FAILURE);
    }
    pop = S->memory->localtags;
    S->memory->localtags = pop->next;
    free(pop);
  }
}

