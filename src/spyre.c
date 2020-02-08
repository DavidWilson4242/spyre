#include <stdio.h>
#include <string.h>
#include <math.h>
#include "spyre.h"
#include "hash.h"
#include "gc.h"

#define DEBUG

#define MEMORY_INITIAL_CAPACITY 128
#define STACK_INITIAL_CAPACITY 1024

void spyre_assert(bool cond) {
  if (!cond) {
    fprintf(stderr, "SPYRE CRITICAL: out of memory");
    exit(EXIT_FAILURE);
  }
}

static void register_type(SpyreState_T *S, SpyreInternalType_T *type) {
  hash_insert(S->internal_types, type->type_name, type);
}

SpyreInternalType_T *get_type(SpyreState_T *S, const char *type_name) {
  return hash_get(S->internal_types, type_name);
}

static void init_types(SpyreState_T *S) {

  SpyreInternalType_T *type_int,
                      *type_float,
                      *type_bool;

  S->internal_types = hash_init();
  
  type_int = malloc(sizeof(SpyreInternalType_T));
  spyre_assert(type_int);
  type_int->type_name = "int";
  type_int->nmembers = 0;
  type_int->totmembers = 0;
  type_int->members = NULL;

  type_float = malloc(sizeof(SpyreInternalType_T));
  spyre_assert(type_float);
  type_float->type_name = "float";
  type_float->nmembers = 0;
  type_float->totmembers = 0;
  type_float->members = NULL;

  type_bool = malloc(sizeof(SpyreInternalType_T));
  spyre_assert(type_bool);
  type_bool->type_name = "bool";
  type_bool->nmembers = 0;
  type_bool->totmembers = 0;
  type_bool->members = NULL;

  register_type(S, type_int);
  register_type(S, type_float);
  register_type(S, type_bool);

}

static void init_memory(SpyreState_T *S) {
  S->memory = malloc(sizeof(SpyreMemoryMap_T));
  spyre_assert(S->memory);
  S->memory->index = 1;
  S->memory->capacity = MEMORY_INITIAL_CAPACITY;
  S->memory->avail = NULL;
  S->memory->allocs = calloc(1, sizeof(uint8_t *) * MEMORY_INITIAL_CAPACITY);
  S->memory->localtags = NULL;
  spyre_assert(S->memory->allocs);
}

static void init_stack(SpyreState_T *S) {
  S->stack = malloc(sizeof(uint8_t) * STACK_INITIAL_CAPACITY);
  spyre_assert(S->stack != NULL);
  S->sp = 0;
  S->ip = 0;
  S->bp = 0;
}

static inline int64_t spyre_pop_int(SpyreState_T *S) {
  S->sp -= sizeof(int64_t);
  return *(int64_t *)&S->stack[S->sp];
}

static inline void spyre_push_int(SpyreState_T *S, int64_t value) {
  *(int64_t *)&S->stack[S->sp] = value;
  S->sp += sizeof(int64_t);
}

static inline void spyre_push_ptr(SpyreState_T *S, size_t value) {
  *(size_t *)&S->stack[S->sp] = value;
  S->sp += sizeof(size_t);
}

size_t spyre_local_asptr(SpyreState_T *S, size_t local_index) {
  return *(size_t *)&S->stack[S->bp + local_index*sizeof(size_t)];
}

/* returns pointer to memory descriptor at the head of the buffer
 * located at seg_id */
static MemoryDescriptor_T *spyre_typedata(SpyreState_T *S, size_t seg_id) {
  uint8_t *rawbuf = S->memory->allocs[seg_id];
  if (!rawbuf) {
    return NULL;
  }
  return (MemoryDescriptor_T *)&rawbuf[0];
}

size_t spyre_alloc(SpyreState_T *S, MemoryDescriptor_T *memdesc) {

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

void spyre_free(SpyreState_T *S, size_t seg_id) {
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

static uint8_t spyre_read_u8(SpyreState_T *S) {
  uint8_t v = S->code[S->ip];
  S->ip += 1;
  return v;
}

static int64_t spyre_read_i64(SpyreState_T *S) {
  int64_t v = *(int64_t *)&S->code[S->ip];
  S->ip += sizeof(int64_t);
  return v;
}

static void spyre_execute(SpyreState_T *S, uint8_t *bytecode) {

  uint8_t opcode;
  int64_t v0, v1, v2;
  bool running = true;

  S->code = bytecode;
  S->ip = 0;

  while (running && (opcode = spyre_read_u8(S))) {
    switch (opcode) {
      case INS_HALT:
        running = false;
        break;
      case INS_IPUSH:
        v0 = spyre_read_i64(S);
        spyre_push_int(S, v0);
        break;
      case INS_IPOP:
        spyre_pop_int(S);
        break;
      case INS_IADD:
        v1 = spyre_pop_int(S);
        v0 = spyre_pop_int(S);
        spyre_push_int(S, v0 + v1);
        break;
      case INS_ISUB:
        v1 = spyre_pop_int(S);
        v0 = spyre_pop_int(S);
        spyre_push_int(S, v0 - v1);
        break;
      case INS_IPRINT:
        printf("%lld\n", spyre_pop_int(S)); 
        break;
      default:
        break;
    }
  }

}

SpyreState_T *spyre_init() {

  SpyreState_T *S = malloc(sizeof(SpyreState_T));
  spyre_assert(S != NULL);
  
  init_memory(S);
  init_stack(S);
  init_types(S);

  uint8_t code[] = {
    0x01, 1, 0, 0, 0, 0, 0, 0, 0,
    0x01, 2, 0, 0, 0, 0, 0, 0, 0,
    0x04,
    0x90,
    0x00
  };

  spyre_execute(S, code);

  return S;

}
