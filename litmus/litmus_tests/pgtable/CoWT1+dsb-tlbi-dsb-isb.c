#include <stdint.h>

#include "lib.h"

static void P0(litmus_test_run* data) {
  /* assuming x, y initialised to 1, 2 */
  asm volatile (
    /* move from C vars into machine regs */
    "mov x0, %[ydesc]\n\t"
    "mov x1, %[xpte]\n\t"
    "mov x2, %[xpage]\n\t"
    "mov x4, %[x]\n\t"

    /* test */
    "str x0, [x1]\n\t"
    "dsb sy\n\t"
    "tlbi vae1, x2\n\t"
    "dsb sy\n\t"
    "isb\n\t"
    "ldr x3, [x4]\n\t"

    /* output back to C vars */
    "str x3, [%[outp0r3]]\n\t"
    :
    : [ydesc] "r" (var_desc(data, "y")), [xpte] "r" (var_pte(data, "x")), [x] "r" (var_va(data, "x")), [outp0r3] "r" (out_reg(data, "p0:x3")), [xpage] "r" (var_page(data, "x"))
    :  "cc", "memory", "x0", "x1", "x2", "x3", "x4"
  );
}

litmus_test_t CoWT1_dsbtlbidsbisb = {
  "CoWT1+dsb-tlbi-dsb-isb",
  1,(th_f*[]){
    (th_f*)P0
  },
  2,(const char*[]){"x", "y"},
  1,(const char*[]){"p0:x3",},
  .interesting_result = (uint64_t[]){
      /* p0:x3 =*/0,
  },
  .start_els=(int[]){1,},
  .no_init_states=1,
  .init_states=(init_varstate_t*[]){
    &(init_varstate_t){"y", TYPE_HEAP, 1},
  },
  .requires_pgtable=1,
  .no_sc_results = 1,
};