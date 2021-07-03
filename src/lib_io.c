#include <stdio.h>
#include <stdlib.h>
#include "lib_io.h"
#include "spyre.h"

int io_print(SpyreState_T *S) {
  
  int x = spyre_pop_int(S);
  printf("%d\n", x);

  return 0;

}

void io_init(SpyreState_T *S) {
  
  spyre_register_cfunc(S, "print", io_print);

}
