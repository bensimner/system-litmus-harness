#include <stdint.h>

#include "lib.h"

static void P0(litmus_test_run* data) {
  asm volatile(
      "mov x0, #1\n\t"
      "str x0, [%[x1]]\n\t"
      "dmb sy\n\t"
      "mov x2, #1\n\t"
      "str x2, [%[x3]]\n\t"
      :
      : [x1] "r" (data->var[0]), [x3] "r" (data->var[1])
      :  "cc", "memory", "x0", "x2"
  );
}

static void svc_handler(void) {
  asm volatile(
      "eret\n\t");
}

static void P1(litmus_test_run* data) {
  asm volatile(
      /* load variables into machine registers */
      "mov x1, %[x1]\n\t"
      "mov x3, %[x3]\n\t"
      "ldr x0, [x1]\n\t"
      /* x3 = X */
      "svc #0\n\t"
      "ldr x2, [x3]\n\t"
      /* extract values */
      "str x0, [%[x0]]\n\t"
      "str x2, [%[x2]]\n\t"
      "dmb st\n\t"
      :
      : [x1] "r" (data->var[1]), [x3] "r" (data->var[0]), [x0] "r" (data->out_reg[0]), [x2] "r" (data->out_reg[1])
      : "cc", "memory", "x0", "x1", "x2", "x3", "x4", "x5", "x6",
        "x7" /* dont touch parameter registers */
  );
}


litmus_test_t MP_dmb_RsvceretR = {
  "MP+dmb+R-svc-eret-R",
  2,(th_f*[]){
    (th_f*)P0,
    (th_f*)P1
  },
  2,(const char*[]){"x", "y"},
  2,(const char*[]){"p1:x0", "p1:x2"},
  .thread_sync_handlers =
    (uint32_t**[]){
     (uint32_t*[]){NULL, NULL},
     (uint32_t*[]){(uint32_t*)svc_handler, NULL},
    },
  .interesting_result = (uint64_t[]){
      /* p1:x0 =*/1,
      /* p1:x2 =*/0,
  },
};