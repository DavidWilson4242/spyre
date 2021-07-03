#include "lib_io.h"

int io_print(SpyreState_T *S) {

  printf("printed!\n");
  return 0;

}

void io_init(SpyreState_T *S) {
  
  spyre_register_cfunc(S, "print", io_print);

}
