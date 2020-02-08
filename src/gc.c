#include <stdio.h>
#include <stdlib.h>
#include "gc.h"

static void domark(SpyreState_T *S, size_t seg_id) {
  uint8_t **allocs = S->memory->allocs;
  MemoryDescriptor_T *mdesc = (MemoryDescriptor_T *)&allocs[seg_id][0];
  SpyreInternalType_T *typeinfo = get_type(S, mdesc->type_name);
  SpyreInternalMember_T *member;
  SpyreInternalType_T *meminfo;
  size_t mem_seg_id;
  
  mdesc->mark = true;
  printf("marked seg_id %zu\n", seg_id);
  
  for (size_t i = 0; i < typeinfo->nmembers; i++) {
    member = typeinfo->members[i];
    meminfo = member->type;
    if (meminfo->nmembers > 0) {
      printf("checking member...\n");
      mem_seg_id = *(size_t *)&allocs[seg_id][sizeof(MemoryDescriptor_T) + member->byte_offset];
      if (mem_seg_id != 0) {
        domark(S, mem_seg_id);
      } else printf("seg id was 0\n");
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
        spyre_free(S, i);
        printf("freed seg_id %zu\n", i);
      }
    }
  }
}

void gc_execute(SpyreState_T *S) {
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

void gc_track_local(SpyreState_T *S, size_t local_index) {
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

/* untags local variables from root node stack */
void gc_untrack_locals(SpyreState_T *S, size_t num_locals) {
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

