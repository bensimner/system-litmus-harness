#include <stdint.h>

#include "lib.h"

static void P0(litmus_test_run* data) {
  asm volatile (
    "mov x0, #1\n\t"
    "mov x1, %[x]\n\t"
    /* test */
    "str x0, [x1]\n\t"
  :
  : [x] "r" (data->var[0]), [y] "r" (data->var[1])
  : "cc", "memory", "x0", "x1"
  );
}

static void P1(litmus_test_run* data) {
  asm volatile (
    "mov x1, %[x]\n\t"
    "mov x2, #1\n\t"
    "mov x3, %[y]\n\t"
    /* test */
    "ldr x0, [x1]\n\t"
    "eor x4, x0, x0\n\t"
    "add x4, x4, x3\n\t"
    "str x2, [x4]\n\t"
    "str x0, [%[outp1r0]]\n\t"
  :
  : [x] "r" (data->var[0]), [y] "r" (data->var[1]), [outp1r0] "r" (data->out_reg[0])
  : "cc", "memory", "x0", "x1", "x2", "x3", "x4"
  );
}

static void P2(litmus_test_run* data) {
  asm volatile (
    "mov x1, %[y]\n\t"
    "mov x3, %[x]\n\t"
    /* test */
    "ldr x0, [x1]\n\t"
    "eor x4, x0, x0\n\t"
    "add x4, x4, x3\n\t"
    "ldr x2, [x4]\n\t"
    "str x0, [%[outp2r0]]\n\t"
    "str x2, [%[outp2r2]]\n\t"
  :
  : [x] "r" (data->var[0]), [y] "r" (data->var[1]), [outp2r0] "r" (data->out_reg[1]), [outp2r2] "r" (data->out_reg[2])
  : "cc", "memory", "x0", "x1", "x2", "x3", "x4"
  );
}

static bar_t* bar = NULL;
static void p0_setup(litmus_test_run* data) {
  if (bar != NULL)
    free(bar);

  bar = alloc(sizeof(bar_t));
  *bar = (bar_t){0};
}

static void teardown(litmus_test_run* data) {
  bwait(get_cpu(), data->i % 3, bar, 3);
  if (get_cpu() == 0) {
    *data->PTE[0] = 0;
  }
}

litmus_test_t check1 = {
  "check1",
  3,(th_f*[]){
    (th_f*)P0,
    (th_f*)P1,
    (th_f*)P2
  },
  2,(const char*[]){"x", "y"},
  3,(const char*[]){"p1:x0", "p2:x0", "p2:x2"},
  .start_els=(int[]){1,1},
  .setup_fns = (th_f*[]){
    (th_f*)p0_setup,
    NULL,
    NULL,
  },
  .teardown_fns = (th_f*[]){
    (th_f*)teardown,
    (th_f*)teardown,
    (th_f*)teardown,
  },
  .requires_pgtable = 1,
};