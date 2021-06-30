#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "asm.h"

/* this file converts a given spyre assembly file (.spys) to
 * executable spyre bytecode (.spyb) */

#define INITIAL_BUFFER_SIZE 1024
#define INSTRUCTION_COUNT (sizeof(instructions)/sizeof(instructions[0]))

static const struct {
  const char *name;
  size_t opcode;
  size_t operands;
} instructions[] = {
  {"HALT",    0x00, 0},
  {"IPUSH",   0x01, 1},
  {"IPOP",    0x02, 0},
  {"IADD",    0x03, 0},
  {"ISUB",    0x04, 0},
  {"IMUL",    0x05, 0},
  {"IDIV",    0x06, 0},
  {"DUP",     0x20, 0},
  {"LDL",     0x80, 1},
  {"SVL",     0x81, 1},
  {"DER",     0x82, 0},
  {"RESL",    0x83, 1},
  {"LDMBR",   0x84, 1},
  {"SVMBR",   0x85, 1},
  {"ARG",     0x86, 1},
  {"IPRINT",  0x90, 0},
  {"FPRINT",  0x91, 0},
  {"PPRINT",  0x92, 0},
  {"FLAGS",   0x93, 0},
  {"ALLOC",   0xA0, 1},
  {"FREE",    0xA1, 0},
  {"TAGL",    0xA2, 1},
  {"UNTAGL",  0xA3, 1},
  {"UNTAGLS", 0xA4, 1},
  {"ITEST",   0xC0, 0},
  {"ICMP",    0xC1, 0},
  {"FCMP",    0xC2, 0},
  {"FTEST",   0xC3, 0},
  {"JMP",     0xC4, 1},
  {"JZ",      0xC5, 1},
  {"JNZ",     0xC6, 1},
  {"JGT",     0xC7, 1},
  {"JGE",     0xC8, 1},
  {"JLT",     0xC9, 1},
  {"JLE",     0xCA, 1},
  {"JEQ",     0xCB, 1},
  {"JNEQ",    0xCC, 1},
  {"CALL",    0xCD, 2},
  {"CCALL",   0xCE, 2},
  {"IRET",    0xCF, 0},
  {"RET",     0xD0, 0}
};

static void advance(AssembleState_T *A, size_t n) {
  for (size_t i = 0; i < n && A->at != NULL; i++) {
    A->at = A->at->next;
  }
}

static LexToken_T *peek(AssembleState_T *A, size_t n) {
  LexToken_T *t = A->at;
  for (size_t i = 0; i < n && t != NULL; i++) {
    t = t->next;
  }
  return t;
}

static bool is_word(AssembleState_T *A, const char *word, LexToken_T *tok) {
  LexToken_T *check = tok != NULL ? tok : A->at;
  return check != NULL && !strcmp(check->as_string, word);
}

static bool is_type(AssembleState_T *A, LexTokenType_T type, LexToken_T *tok) {
  LexToken_T *check = tok != NULL ? tok : A->at;
  return check != NULL && check->type == type; 
}

static void check_grow(AssembleState_T *A) {

  uint8_t *newbuf;

  if (A->bufat - 8 >= A->sizebuf) {
    newbuf = malloc(sizeof(uint8_t) * (A->sizebuf*2 + 2));
    memcpy(newbuf, A->writebuf, sizeof(uint8_t) * A->sizebuf);
    free(A->writebuf);
    A->writebuf = newbuf;
    A->sizebuf = A->sizebuf*2 + 2;
  }

}

static void write_u8(AssembleState_T *A, uint8_t b) {
  check_grow(A);
  A->writebuf[A->bufat++] = b;
}

static void write_i64(AssembleState_T *A, int64_t v) {
  check_grow(A);
  *(int64_t *)&A->writebuf[A->bufat] = v;
  A->bufat += sizeof(int64_t);
}

static void write_f64(AssembleState_T *A, double v) {
  check_grow(A);
  *(double *)&A->writebuf[A->bufat] = v;
  A->bufat += sizeof(double);
}

static void register_label(AssembleState_T *A) {
  char *key = malloc(strlen(A->at->as_string) + 1);
  size_t *value = malloc(sizeof(size_t));

  assert(key && value);
  
  strcpy(key, A->at->as_string);
  advance(A, 1);
  *value = A->bufat;
  advance(A, 1);

  hash_insert(A->labels, key, value);
}

/* if a label reference is found that hasn't yet been declared, make space
 * for it and add it to the pending list */
static void pend_label(AssembleState_T *A, const char *name) {
  PendingLabel_T *append = malloc(sizeof(PendingLabel_T));
  assert(append);
  append->label_name = malloc(strlen(name) + 1);
  assert(append->label_name);
  strcpy(append->label_name, A->at->as_string);
  append->bufaddr = A->bufat;
  if (A->pending) {
    append->next = A->pending;
    A->pending = append;
  } else {
    append->next = NULL;
    A->pending = append;
  }
  A->bufat += sizeof(size_t);
}

/* writes all pending labels to writebuf.  if a label hasn't yet been
 * declared, it doesn't exist anywhere, so throw an error */
static void write_pending_labels(AssembleState_T *A) {
  PendingLabel_T *pending;
  size_t *label;
  while (A->pending != NULL) {
    pending = A->pending;
    A->pending = pending->next;
    label = hash_get(A->labels, pending->label_name);
    if (label == NULL) {
      fprintf(stderr, "unknown label '%s'\n", pending->label_name);
      exit(EXIT_FAILURE);
    }
    *(size_t *)&A->writebuf[pending->bufaddr] = *label;
    free(pending->label_name);
    free(pending);
  }
}

static void read_instruction(AssembleState_T *A) {
  
  bool found_ins = false;
  size_t *label;
  PendingLabel_T *append;

  if (!is_type(A, TOKEN_IDENTIFIER, NULL)) {
    fprintf(stderr, "expected instruction\n");
    exit(EXIT_FAILURE);
  }

  for (size_t i = 0; i < INSTRUCTION_COUNT; i++) {
    if (!strcmp(instructions[i].name, A->at->as_string)) {
      advance(A, 1);
      write_u8(A, instructions[i].opcode);
      for (size_t j = 0; j < instructions[i].operands; j++) {
        if (!A->at) {
          fprintf(stderr, "expected operand\n");
          exit(EXIT_FAILURE);
        }
        switch (A->at->type) {
          case TOKEN_INTEGER:
            write_i64(A, A->at->ival);
            break;
          case TOKEN_FLOAT:
            write_f64(A, A->at->fval);
            break;
          case TOKEN_IDENTIFIER:
            label = hash_get(A->labels, A->at->as_string);
            if (label != NULL) {
              write_i64(A, *(int64_t *)label);
            } else {
              pend_label(A, A->at->as_string);
            }
            break;
          default:
            fprintf(stderr, "invalid operand '%s'\n", A->at->as_string);
            exit(EXIT_FAILURE);
        }
        advance(A, 1);
      }
      found_ins = true;
      break;
    }
  }

  if (!found_ins) {
    fprintf(stderr, "invalid instruction '%s'\n", A->at->as_string);
    exit(EXIT_FAILURE);
  }
}

static void read_db(AssembleState_T *A) {
  advance(A, 1);
  if (!is_type(A, TOKEN_STRING_LITERAL, NULL)) {
    fprintf(stderr, "expected string to follow 'db'\n");
    exit(EXIT_FAILURE);
  }
  for (const char *c = A->at->as_string; *c; c++) {
    write_u8(A, *c);
  }
  write_u8(A, 0);
  advance(A, 1);
}

static AssembleState_T *asmstate_init(const char *infile, const char *outfile) {
  AssembleState_T *A = malloc(sizeof(AssembleState_T));
  assert(A);
  A->L = lex_file(infile);
  A->outfile = fopen(outfile, "wb");
  if (A->outfile == NULL) {
    fprintf(stderr, "couldn't open '%s' for reading\n", outfile);
  }
  A->labels = hash_init();
  A->at = A->L->tokens;

  A->writebuf = malloc(sizeof(uint8_t) * INITIAL_BUFFER_SIZE);
  A->sizebuf = INITIAL_BUFFER_SIZE;
  A->bufat = 0;

  return A;
}

void assemble_file(const char *infile, const char *outfile) {
  
  AssembleState_T *A = asmstate_init(infile, outfile);
  
  /* make one pass through the token list */
  while (A->at != NULL) {
    if (is_type(A, TOKEN_IDENTIFIER, NULL) && is_word(A, ":", peek(A, 1))) {
      register_label(A);
    } else if (!strcmp(A->at->as_string, "db")) {
      read_db(A);
    } else {
      read_instruction(A);
    }
  }
  
  /* some labels were undetermined during first pass.  fill these labels
   * and report error for labels that are still missing */
  write_pending_labels(A);
  
  /* write bytecode buffer to file */
  FILE *out = fopen(outfile, "wb");
  if (out == NULL) {
    fprintf(stderr, "couldn't open file '%s' for writing\n", outfile);
    exit(EXIT_FAILURE);
  }
  fwrite(A->writebuf, 1, A->bufat, out);
  fclose(out);
  
}
