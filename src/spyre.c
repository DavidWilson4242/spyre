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

/* used as closure argument when loading in struct members */
typedef struct MemberRegistrationHelper {
  SpyreState_T *S;
  SpyreInternalType_T *parent;
  size_t mbr_index;
} MemberRegistrationHelper_T;

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
  type_int->members = NULL;

  type_float = malloc(sizeof(SpyreInternalType_T));
  spyre_assert(type_float);
  type_float->type_name = "float";
  type_float->nmembers = 0;
  type_float->members = NULL;

  type_bool = malloc(sizeof(SpyreInternalType_T));
  spyre_assert(type_bool);
  type_bool->type_name = "bool";
  type_bool->nmembers = 0;
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

static inline size_t spyre_pop_ptr(SpyreState_T *S) {
  S->sp -= sizeof(size_t);
  return *(size_t *)&S->stack[S->sp];
}

static inline void spyre_push_int(SpyreState_T *S, int64_t value) {
  *(int64_t *)&S->stack[S->sp] = value;
  S->sp += sizeof(int64_t);
}

static inline int64_t spyre_top_int(SpyreState_T *S) {
  return *(int64_t *)&S->stack[S->sp - 8];
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
  double f0, f1, f2;
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

      /* arithmetic */
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

      /* misc */
      case INS_DUP:
	spyre_push_int(S, spyre_top_int(S));
	break; 
      case INS_FEQ:
	spyre_push_int(S, S->feq);
	break;
      
      /* debug */
      case INS_IPRINT:
        printf("%lld\n", spyre_pop_int(S));
        break;
      case INS_FPRINT:
        break;
      case INS_PPRINT:
        break;
      case INS_FLAGS:
	printf("****** FLAGS ******\n");
	printf("fz : %d\n", S->fz);
	printf("feq: %d\n", S->feq);
	printf("fgt: %d\n", S->fgt);
	printf("fge: %d\n", S->fge);
	printf("*******************\n");
	break;

      /* memory management and GC */
      case INS_ALLOC:
        v0 = read_u64(S);	
        mdesc.type_name = (char *)&S->code[v0];
        mdesc.arrdim = 0;
        mdesc.arrs = NULL;
        mdesc.ptrdim = 0;
        v1 = spymem_alloc(S, &mdesc);
        spyre_push_ptr(S, v1);
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
      case INS_ARG:
	v0 = read_u64(S);
	v1 = *(uint64_t *)&S->stack[S->bp - 24]; /* number of args passed */
	spyre_push_int(S, *(int64_t *)&S->stack[S->bp - 3*8 - (v1 - v0)*8]);
	break;

      /* local management */
      case INS_LDL:
        v0 = read_u64(S);
        spyre_push_word(S, *(uint64_t *)&S->stack[S->bp + v0*sizeof(uint64_t)]);
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
      case INS_SVLS:
	v0 = spyre_pop_int(S); /* value to save */
	v1 = spyre_pop_int(S); /* local index to save to */
	*(size_t *)&S->stack[S->bp + v1*sizeof(uint64_t)] = v0;
	break;

      /* branching */
      case INS_ITEST:
        v0 = spyre_pop_int(S);
        S->fz = (v0 == 0);
        break;
      case INS_ICMP:
        v1 = spyre_pop_int(S);
        v0 = spyre_pop_int(S);
        S->feq = (v0 == v1);
        S->fgt = (v0 > v1);
        S->fge = (v0 >= v1);
        break;
      case INS_FTEST:
        break;
      case INS_FCMP:
        break;
      case INS_JMP:
        v0 = read_u64(S);
        S->ip = v0;
        break;
      case INS_JZ:
        v0 = read_u64(S);
        if (S->fz) {
          S->ip = v0;
        }
        break;
      case INS_JNZ:
        v0 = read_u64(S);
        if (!S->fz) {
          S->ip = v0;
        }
        break;
      case INS_JGT:
        v0 = read_u64(S);
        if (S->fgt) {
          S->ip = v0;
        }
        break;
      case INS_JGE:
        v0 = read_u64(S);
        if (S->fge) {
          S->ip = v0;
        }
        break;
      case INS_JLT:
        v0 = read_u64(S);
        if (!S->fge) {
          S->ip = v0;
        }
        break;
      case INS_JLE:
        v0 = read_u64(S);
        if (!S->fgt) {
          S->ip = v0;
        }
        break;
      case INS_JEQ:
        v0 = read_u64(S);
        if (S->feq) {
          S->ip = v0;
        }
        break;
      case INS_JNEQ:
        v0 = read_u64(S);
        if (!S->feq) {
          S->ip = v0;
        }
        break;
      case INS_CALL:
	v0 = read_u64(S); /* func addr */
	v1 = read_u64(S); /* num args */	
	spyre_push_int(S, v1);     /* push number args */
	spyre_push_ptr(S, S->bp);  /* push base pointer */
	spyre_push_ptr(S, S->ip);  /* push return address */
	S->bp = S->sp;
	S->ip = v0;
	break;
      case INS_CCALL:

	break;
      case INS_IRET:
	v0 = spyre_pop_int(S); /* return value */
	S->sp = S->bp;
	S->ip = spyre_pop_ptr(S);
	S->bp = spyre_pop_ptr(S);
	S->sp -= spyre_pop_int(S)*8;
	spyre_push_int(S, v0);
	break;

      case INS_RET:
	S->sp = S->bp;
	S->ip = spyre_pop_ptr(S);
	S->bp = spyre_pop_ptr(S);
	S->sp -= spyre_pop_int(S)*8;
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
  
  /* read entire file into buffer */
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

static void map_register_member(const char *key, void *mbr, void *cl) {
  MemberRegistrationHelper_T *helper = cl;
  Declaration_T *member = mbr;
  SpyreInternalMember_T *intmbr = malloc(sizeof(SpyreInternalMember_T));
  spyre_assert(intmbr != NULL);
  intmbr->type = get_type(helper->S, member->dt->type_name);
  intmbr->ptrdim = member->dt->ptrdim;
  intmbr->arrdim = member->dt->arrdim;
  intmbr->byte_offset = sizeof(size_t) * helper->mbr_index;
  helper->parent->members[helper->mbr_index] = intmbr;
  helper->mbr_index++;

  if (intmbr->type == NULL) {
    fprintf(stderr, "critical: couldn't find member '%s''s type (%s)\n", 
                    key, member->dt->type_name);
    exit(EXIT_FAILURE);
  }

  printf("registering member %s\n", intmbr->type->type_name);
}

/* registers all top-level types.  does not register members.
 * register_all_members is called afterwards to process members */
static void map_register_type(const char *key, void *dt, void *cl) {
  SpyreState_T *S = cl;
  Datatype_T *datatype = (Datatype_T *)dt;
  SpyreInternalType_T *type = malloc(sizeof(SpyreInternalType_T));
  spyre_assert(type != NULL);
  type->type_name = malloc(strlen(key) + 1);
  spyre_assert(type->type_name != NULL);
  strcpy(type->type_name, key);
  type->nmembers = datatype->sdesc->members->size;
  type->members = malloc(sizeof(SpyreInternalMember_T) * type->nmembers);
  spyre_assert(type->members);
  register_type(S, type);

  printf("registering type %s\n", key);
}

static void map_register_all_members(const char *key, void *dt, void *cl) {
  SpyreState_T *S = cl;
  Datatype_T *datatype = dt;
  SpyreInternalType_T *type = get_type(S, datatype->type_name);

  printf("registering members of type %s\n", key);

  /* now register each member */
  MemberRegistrationHelper_T helper;
  helper.parent = type;
  helper.mbr_index = 0;
  helper.S = S;
  hash_foreach(datatype->sdesc->members, map_register_member, &helper);
}

/* execute the FNAME bytecode file with context.  The parse state is
 * assumed to have parsed the file FNAME, and extracted relevant information
 * such as user-defined types (structs) */
void spyre_execute_with_context(const char *fname, ParseState_T *P) {
  
  SpyreState_T *S = spyre_init();
  SpyreHash_T *usertypes = P->usertypes;
  hash_foreach(usertypes, map_register_type, S);
  hash_foreach(usertypes, map_register_all_members, S);

}

SpyreState_T *spyre_init() {

  SpyreState_T *S = malloc(sizeof(SpyreState_T));
  spyre_assert(S != NULL);

  init_memory(S);
  init_stack(S);
  init_types(S);

  return S;

}
