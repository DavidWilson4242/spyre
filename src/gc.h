#ifndef GC_H
#define GC_H

#include "spyre.h"

void gc_execute(SpyreState_T *S);
void gc_track_local(SpyreState_T *S, size_t local_index);
void gc_untrack_locals(SpyreState_T *S, size_t num_locals);

#endif
