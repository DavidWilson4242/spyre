#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lex.h"
#include "parse.h"

typedef enum CompileMode {
  COMP_NONE = 0,
  COMP_FULL = 1,
  COMP_ASSEMBLE = 2
} CompileMode_T;

void usage() {
  printf("usage: spyre [-c spyre_file] [-a spyre_asm_file]\n");
}

void set_compile_mode(CompileMode_T *compile_mode, int *arg, char **infile, 
    int argc, char **argv, CompileMode_T set_mode) {
  if (*compile_mode != COMP_NONE) {
    fprintf(stderr, "only one compile mode is allowed at once\n");
    exit(EXIT_FAILURE);
  }
  if (*arg >= argc - 1) {
    fprintf(stderr, "expected input file following flag '%s'\n", argv[*arg]);
    exit(EXIT_FAILURE);
  }
  *infile = argv[*arg + 1]; 
  (void)*arg++;
  *compile_mode = set_mode;
}

int main(int argc, char **argv) {

  if (argc <= 1) {
    usage();
    return EXIT_FAILURE;
  }

  CompileMode_T compile_mode = COMP_NONE;
  char *infile = NULL;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-c")) {
      set_compile_mode(&compile_mode, &i, &infile, argc, argv, COMP_FULL); 
    } else if (!strcmp(argv[i], "-a")) {
      set_compile_mode(&compile_mode, &i, &infile, argc, argv, COMP_ASSEMBLE);
    } else if (!strcmp(argv[i], "--help")) {
      usage();
      return EXIT_SUCCESS;
    }
  }

  if (compile_mode == COMP_NONE) {
    fprintf(stderr, "expected a compile mode and input file\n");
    return EXIT_FAILURE;
  }

  if (compile_mode == COMP_FULL) {
    LexState_T *L = lex_file(infile);
    ParseState_T *P = parse_file(L);
    lex_cleanup(&L);
    parse_cleanup(&P);
  }

  return EXIT_SUCCESS;

}
