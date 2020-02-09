#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lex.h"
#include "asm.h"
#include "spyre.h"
#include "parse.h"

typedef enum CompileMode {
  COMP_NONE = 0,
  COMP_FULL,
  COMP_ASSEMBLE,
	COMP_EXECUTE
} CompileMode_T;

void usage() {
  printf("usage: spyre [-c spyre_file] [-a spyre_asm_file]\n"
	       "             [-r spyre_bytecode_file]\n");
}

void set_compile_mode(CompileMode_T *compile_mode, int *argn, char **infile, 
    int argc, char **argv, CompileMode_T set_mode) {
  if (*compile_mode != COMP_NONE) {
    fprintf(stderr, "only one compile mode is allowed at once\n");
    exit(EXIT_FAILURE);
  }
  if (*argn >= argc - 1) {
    fprintf(stderr, "expected input file following flag '%s'\n", argv[*argn]);
    exit(EXIT_FAILURE);
  }
  *infile = argv[*argn + 1]; 
  (void)*argn++;
  *compile_mode = set_mode;
}

void set_output_file(int *argn, char **outfile, int argc, char **argv) {
	if (*argn >= argc - 1) {
		fprintf(stderr, "expected output file following flag -o\n");
		exit(EXIT_FAILURE);
	}
	*outfile = argv[*argn + 1];
	(void)*argn++;
}

int main(int argc, char **argv) {

  if (argc <= 1) {
    usage();
    return EXIT_FAILURE;
  }

  CompileMode_T compile_mode = COMP_NONE;
  char *infile = NULL;
	char *outfile = NULL;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-c")) {
      set_compile_mode(&compile_mode, &i, &infile, argc, argv, COMP_FULL); 
    } else if (!strcmp(argv[i], "-a")) {
      set_compile_mode(&compile_mode, &i, &infile, argc, argv, COMP_ASSEMBLE);
		} else if (!strcmp(argv[i], "-r")) {
      set_compile_mode(&compile_mode, &i, &infile, argc, argv, COMP_EXECUTE);
		} else if (!strcmp(argv[i], "-o")) {
			set_output_file(&i, &outfile, argc, argv);
    } else if (!strcmp(argv[i], "--help")) {
      usage();
      return EXIT_SUCCESS;
    }
  }

	if (compile_mode == COMP_ASSEMBLE && outfile == NULL) {
		fprintf(stderr, "expected an output file\n");
		exit(EXIT_FAILURE);
	}

	LexState_T *L;
	ParseState_T *P;

	switch (compile_mode) {
		case COMP_NONE:
			fprintf(stderr, "expected a compile mode and input file\n");
			return EXIT_FAILURE;
		case COMP_FULL:
			L = lex_file(infile);
			P = parse_file(L);
      spyre_execute_with_context(NULL, P);
			lex_cleanup(&L);
			parse_cleanup(&P);
			break;
		case COMP_ASSEMBLE:
			assemble_file(infile, outfile);			
			break;
		case COMP_EXECUTE:
			spyre_execute_file(infile);		
			break;
		default:
			break;
  }

  return EXIT_SUCCESS;

}
