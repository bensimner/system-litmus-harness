#include <stdint.h>

#include "lib.h"

static void P0(litmus_test_run* data) {
  asm volatile (
    /* load registers */
    "mov x1, %[x1]\n\t"
    "mov x3, %[x3]\n\t"
    "mov x0, #1\n\t"
    "str x0, [x1]\n\t"
    "dmb sy\n\t"
    "ldr x2, [x3]\n\t"
    /* collect results */
    "str x2, [%[outp0r2]]\n\t"
  :
  : [x1] "r" (var_va(data, "x")), [x3] "r" (var_va(data, "y")), [outp0r2] "r" (out_reg(data, "p0:x2"))
  : "cc", "memory", "x0", "x1", "x2", "x3"
  );
}

static void P1(litmus_test_run* data) {
  asm volatile (
    /* load registers */
    "mov x1, %[x1]\n\t"
    "mov x3, %[x3]\n\t"
    "mov x0, #1\n\t"
    "str x0, [x1]\n\t"
    "dmb sy\n\t"
    "ldr x2, [x3]\n\t"
    /* collect results */
    "str x2, [%[outp1r2]]\n\t"
  :
  : [x1] "r" (var_va(data, "y")), [x3] "r" (var_va(data, "x")), [outp1r2] "r" (out_reg(data, "p1:x2"))
  : "cc", "memory", "x0", "x1", "x2", "x3"
  );
}


litmus_test_t SB_dmbs = {
  "SB+dmbs",
  2,(th_f*[]){
    (th_f*)P0,
    (th_f*)P1
  },
  2,(const char*[]){"x", "y"},
  2,(const char*[]){"p0:x2", "p1:x2"},
  .interesting_result =
    (uint64_t[]){
      /* p1:x0 =*/ 0,
      /* p1:x2 =*/ 0,
  },
  .no_sc_results = 3,
};