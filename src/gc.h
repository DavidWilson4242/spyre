#ifndef GC_H
#define GC_H

#include "spyre.h"

void spygc_execute(SpyreState_T *S);
void spygc_track_local(SpyreState_T *S, size_t local_index);
void spygc_untrack_locals(SpyreState_T *S, size_t num_locals);
void spygc_untrack_local(SpyreState_T *S, size_t local_index);

#endif
