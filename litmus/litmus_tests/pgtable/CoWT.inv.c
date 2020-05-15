#include <stdint.h>

#include "lib.h"

static void sync_handler(void) {
  asm volatile (
    "mov x2, #0\n\t"
  );
}

static void P0(litmus_test_run* data) {
  asm volatile (
      /* move from C vars into machine regs */
      "mov x0, %[ydesc]\n\t"
      "mov x1, %[xpte]\n\t"
      "mov x3, %[x]\n\t"

      /* test */
      "str x0, [x1]\n\t"
      "ldr x2, [x3]\n\t"

      /* output */
      "str x2, [%[outp0r2]]\n\t"
      :
      : [ydesc] "r" (var_desc(data, "y")), [xpte] "r" (var_pte(data, "x")), [x] "r" (var_va(data, "x")), [outp0r2] "r" (out_reg(data, "p0:x2"))
      :  "cc", "memory", "x0", "x1", "x2", "x3"
  );
}

litmus_test_t CoWTinv = {
  "CoWT.inv",
  1,(th_f*[]){
    (th_f*)P0
  },
  2,(const char*[]){"x", "y"},
  1,(const char*[]){"p0:x2",},
  .interesting_result = (uint64_t[]){
    /* p0:x2 =*/0,
  },
  .no_init_states=2,
  .init_states=(init_varstate_t*[]){
    &(init_varstate_t){"x", TYPE_PTE, 0},
    &(init_varstate_t){"y", TYPE_HEAP, 1},
  },
  .thread_sync_handlers = (uint32_t**[]){
    (uint32_t*[]){NULL, NULL},
    (uint32_t*[]){(uint32_t*)sync_handler, NULL},
  },
  .requires_pgtable = 1,
  .no_sc_results = 1,
};