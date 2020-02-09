#include <stdio.h>
#include <string.h>
#include <math.h>
#include "spyre.h"
#include "hash.h"
#include "gc.h"
#include "memory.h"

/* this file is the meat of the Spyre virtual machine.  It loads a 
 * spyre bytecode file and executes it accordingly. */

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

  //a0 = spymem_alloc(S, &desc);
  //spyre_push_ptr(S, a0);
  //spygc_track_local(S, 0);

  //spygc_execute(S);
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

static inline void spyre_push_word(SpyreState_T *S, uint64_t word) {
  *(uint64_t *)&S->stack[S->sp] = word;
  S->sp += sizeof(uint64_t);
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

static uint8_t read_u8(SpyreState_T *S) {
  uint8_t v = S->code[S->ip];
  S->ip += 1;
  return v;
}

static int64_t read_i64(SpyreState_T *S) {
  int64_t v = *(int64_t *)&S->code[S->ip];
  S->ip += sizeof(int64_t);
  return v;
}

static uint64_t read_u64(SpyreState_T *S) {
  uint64_t v = *(uint64_t *)&S->code[S->ip];
  S->ip += sizeof(uint64_t);
  return v;
}

static void spyre_execute(SpyreState_T *S, uint8_t *bytecode) {

  uint8_t opcode;
  bool running = true;

  /* variables for instructions */
  int64_t v0, v1, v2;
  uint8_t *rawbuf;
  const char *typename;
  MemoryDescriptor_T mdesc;

  S->code = bytecode;
  S->ip = 0;

  while (running && (opcode = read_u8(S))) {
    switch (opcode) {
      case INS_HALT:
        running = false;
        break;
      case INS_IPUSH:
        v0 = read_i64(S);
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
      case INS_IMUL:
        v1 = spyre_pop_int(S);
        v0 = spyre_pop_int(S);
        spyre_push_int(S, v0 * v1);
        break;
      case INS_IDIV:
        v1 = spyre_pop_int(S);
        v0 = spyre_pop_int(S);
        spyre_push_int(S, v0 / v1);
        break;
      case INS_IPRINT:
        printf("%lld\n", spyre_pop_int(S));
        break;
      case INS_ALLOC:
        v0 = read_u64(S);	
        mdesc.type_name = (char *)&S->code[v0];
        mdesc.arrdim = 0;
        mdesc.arrs = NULL;
        mdesc.ptrdim = 0;
        v1 = spymem_alloc(S, &mdesc);
        spyre_push_ptr(S, v1);
        break;
      case INS_LDL:
        v0 = read_u64(S);
        spyre_push_ptr(S, *(size_t *)&S->stack[S->bp + v0*sizeof(uint64_t)]);
        break;
      case INS_SVL:
        v0 = read_u64(S);
        v1 = spyre_pop_int(S);
        *(size_t *)&S->stack[S->bp + v0*sizeof(uint64_t)] = v1;
        break;
      case INS_RESL:
        v0 = read_u64(S);
        S->sp += v0 * sizeof(size_t);
        break;
      case INS_LDMBR:
        v0 = read_u64(S); /* member index */
        v1 = spyre_pop_int(S); /* segment id */
        rawbuf = spymem_rawbuf(S, v1);
        spyre_push_word(S, *(uint64_t *)&rawbuf[v0 * sizeof(uint64_t)]);
        break;
      case INS_SVMBR:
        v0 = read_u64(S);
        v1 = spyre_pop_int(S); /* value to save */
        v2 = spyre_pop_int(S); /* segment id */
        rawbuf = spymem_rawbuf(S, v2);
        *(uint64_t *)&rawbuf[v0 * sizeof(uint64_t)] = v1;
        break;
      case INS_FREE:
        break;
      case INS_TAGL:
        v0 = read_u64(S);
        spygc_track_local(S, v0);
        break;
      case INS_UNTAGL:
        v0 = read_u64(S);
        spygc_untrack_local(S, v0);
        break;
      case INS_UNTAGLS:
        v0 = read_u64(S);
        spygc_untrack_locals(S, v0);
        break;
      default:
        break;
    }
  }

}

void spyre_execute_file(const char *fname) {

  FILE *infile = fopen(fname, "rb");
  if (infile == NULL) {
    fprintf(stderr, "couldn't open '%s' for reading\n", fname);
    exit(EXIT_FAILURE);
  }

  SpyreState_T *S = spyre_init();

  uint8_t *buffer;
  unsigned long long flen;
  fseek(infile, 0, SEEK_END);
  flen = ftell(infile);
  fseek(infile, 0, SEEK_SET);
  buffer = malloc(flen);
  spyre_assert(buffer != NULL);
  fread(buffer, 1, flen, infile);
  spyre_execute(S, buffer);
  free(buffer);

  spygc_execute(S);

}

SpyreState_T *spyre_init() {

  SpyreState_T *S = malloc(sizeof(SpyreState_T));
  spyre_assert(S != NULL);

  init_memory(S);
  init_stack(S);
  init_types(S);

  return S;

}
