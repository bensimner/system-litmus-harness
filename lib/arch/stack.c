#include "lib.h"

stack_t* walk_stack(void) {

  /* have to disable debugging during this alloc
   * otherwise there's a cycle in the alloc debug()s
   */
  uint8_t d = DEBUG;
  DEBUG = 0;
  stack_t* frames = alloc(4096);
  DEBUG = d;
  int i = 0;

  /* with -fno-omit-frame-pointer the frame pointer gets put
   * in register x29 */
  uint64_t init_fp = read_reg(x29);
  uint64_t* fp = (uint64_t*)init_fp;

  while (1) {
    /* each stack frame is
    * ------ fp
    * u64: fp
    * u64: ret
    */
    uint64_t* prev_fp  = (uint64_t*)*fp;
    uint64_t ret       = *(fp+1);
    frames->frames[i++].ret = ret;
    fp = prev_fp;

    /* the frame pointer is initialised to 0
     * for all entry points */
    if (prev_fp == 0) {
      break;
    }
  }

  frames->no_frames = i;
  return frames;
}