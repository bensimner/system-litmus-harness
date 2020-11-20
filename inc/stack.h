#ifndef STACK_H
#define STACK_H

/** header for a simple stack walker
 *
 * requires -fno-omit-frame-pointer
 */

#include "lib.h"

typedef struct {
  uint64_t next;
  uint64_t ret;
} stack_frame_t;

typedef struct {
  int no_frames;
  stack_frame_t frames[];
} stack_t;

stack_t* walk_stack_from(uint64_t* fp);
stack_t* walk_stack(void);

#endif /* STACK_H */