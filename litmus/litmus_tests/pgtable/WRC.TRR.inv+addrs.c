#include <stdint.h>

#include "lib.h"

static void P0(test_ctx_t* ctx, int i, uint64_t** heap_vars, uint64_t** ptes, uint64_t* pas, uint64_t** out_regs) {
  uint64_t* x = heap_vars[0];
  uint64_t* y = heap_vars[1];
  uint64_t* z = heap_vars[2];
  
  uint64_t* xpte = ptes[0];
  uint64_t* zpte = ptes[2];
  
  uint64_t zdesc = *zpte;

  asm volatile (
    "mov x0, %[zdesc]\n\t"
    "mov x1, %[xpte]\n\t"

    /* test */
    "str x0, [x1]\n\t"
  :
  : [zdesc] "r" (zdesc), [xpte] "r" (xpte)
  : "cc", "memory", "x0", "x1"
  );
}

static void P1(test_ctx_t* ctx, int i, uint64_t** heap_vars, uint64_t** ptes, uint64_t* pas, uint64_t** out_regs) {
  uint64_t* x = heap_vars[0];
  uint64_t* y = heap_vars[1];

  uint64_t* outp1r0 = out_regs[0];

  asm volatile (
    "mov x1, %[x]\n\t"
    "mov x2, 1\n\t"
    "mov x3, %[y]\n\t"

    /* test */
    "ldr x0, [x1]\n\t"
    "eor x4, x0, x0\n\t"
    "add x4, x4, x3\n\t"
    "str x2, [x4]\n\t"

    /* output */
    "str x0, [%[outp1r0]]\n\t"
  :
  : [x] "r" (x), [y] "r" (y), [outp1r0] "r" (outp1r0)
  : "cc", "memory", "x0", "x1", "x2", "x3", "x4", "x10"
  );
}

static void sync_handler(void) {
  asm volatile (
    "mov x0, #0\n\t"

    "mrs x10, elr_el1\n\t"
    "add x10,x10,#4\n\t"
    "msr elr_el1, x10\n\t"
    "eret\n\t"
  );
}

static void P2(test_ctx_t* ctx, int i, uint64_t** heap_vars, uint64_t** ptes, uint64_t* pas, uint64_t** out_regs) {
  uint64_t* x = heap_vars[0];
  uint64_t* y = heap_vars[1];

  uint64_t* xpte = ptes[0];
  uint64_t* zpte = ptes[2];

  uint64_t zdesc = *zpte;
  
  uint64_t* p2r0 = out_regs[1];
  uint64_t* p2r2 = out_regs[2];

  asm volatile (
    "mov x1, %[y]\n\t"
    "mov x3, %[xpte]\n\t"
    "mov x5, %[zdesc]\n\t"

    "ldr x0, [x1]\n\t"
    "eor x4, x0, x0\n\t"
    "add x4, x4, x3\n\t"
    "ldr x2, [x4]\n\t"

    "eor x2, x2, x5\n\t"
    "cbz x2, .after\n\t"
    "mov x2, #0\n\t"
    ".after:\n\t"
    "eor x2, x2, #1\n\t"

    "str x0, [%[p2r0]]\n\t"
    "str x2, [%[p2r2]]\n\t"
  :
  : [y] "r" (y), [xpte] "r" (xpte), [zdesc] "r" (zdesc), [p2r0] "r" (p2r0), [p2r2] "r" (p2r2)
  : "cc", "memory", "x0", "x1", "x2", "x3", "x4", "x5"
  );
}



litmus_test_t WRCtrrinv_addrs = {
  "WRC.TRR.inv+addrs",
  3,(th_f*[]){
    (th_f*)P0,
    (th_f*)P1,
    (th_f*)P2
  },
  3,(const char*[]){"x", "y", "z"},
  3,(const char*[]){"p1:x0", "p2:x0", "p2:x2"},
  .no_init_states=2,
  .init_states=(init_varstate_t*[]){
      &(init_varstate_t){"x", TYPE_PTE, 0},
      &(init_varstate_t){"z", TYPE_HEAP, 1},
    },
  .thread_sync_handlers =
    (uint32_t**[]){
     (uint32_t*[]){NULL, NULL},
     (uint32_t*[]){(uint32_t*)sync_handler, NULL},
     (uint32_t*[]){NULL, NULL},
    },
  .interesting_result =
    (uint64_t[]){
      /* p1:x0 =*/ 1,
      /* p2:x0 =*/ 1,
      /* p2:x2 =*/ 0,
    },
  .requires_pgtable=1,
};