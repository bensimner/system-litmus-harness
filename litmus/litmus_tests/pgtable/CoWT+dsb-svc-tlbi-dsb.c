#include <stdint.h>

#include "lib.h"

static void svc_handler(void) {
  asm volatile (
    "tlbi vaae1is, x4\n\t"
    "dsb sy\n\t"
    "ldr x2, [x3]\n\t"
    "eret\n\t"
  );
}

static uint32_t* old_vtentry;
static void P0_pre(test_ctx_t* ctx, int i, uint64_t** heap_vars, uint64_t** ptes, uint64_t* pas, uint64_t** out_regs) {
  old_vtentry = hotswap_exception(0x400, (uint32_t*)&svc_handler);
}

static void P0(test_ctx_t* ctx, int i, uint64_t** heap_vars, uint64_t** ptes,
               uint64_t* pas, uint64_t** out_regs) {

  uint64_t* x = heap_vars[0];
  uint64_t* y = heap_vars[1];

  uint64_t* x2 = out_regs[0];

  uint64_t* xpte = ptes[0];
  uint64_t* ypte = ptes[1];

  /* assuming x, y initialised to 1, 2 */

  asm volatile (
      /* move from C vars into machine regs */
      "mov x0, %[ydesc]\n\t"
      "mov x1, %[xpte]\n\t"
      "mov x3, %[x]\n\t"
      "mov x4, x3\n\t"
      "lsr x4, x4, #12\n\t"

      /* test */
      "str x0, [x1]\n\t"
      "dsb sy\n\t"
      "svc #0\n\t"

      /* output back to C vars */
      "str x2, [%[x2]]\n\t"
      :
      : [ydesc] "r" (*ypte), [xpte] "r" (xpte), [x] "r" (x), [x2] "r" (x2)
      : "cc", "memory", "x0", "x1", "x2", "x3", "x4");
}

static void P0_post(test_ctx_t* ctx, int i, uint64_t** heap_vars, uint64_t** ptes, uint64_t* pas, uint64_t** out_regs) {
  restore_hotswapped_exception(0x400, old_vtentry);
}

void CoWT_dsbsvctlbidsb(void) {
  run_test("CoWT+dsb-svc-tlbi-dsb",
    1, (th_f** []){
      (th_f* []) {P0_pre, P0, P0_post},
    },
    2, (const char* []){"x", "y"},
    1, (const char* []){"p0:x2",},
    (test_config_t){
        .interesting_result = (uint64_t[]){
            /* p0:x2 =*/1,
        },
        .no_init_states=2,
        .init_states=(init_varstate_t[]){
          (init_varstate_t){"x", TYPE_HEAP, 1},
          (init_varstate_t){"y", TYPE_HEAP, 2},
        }
    });
}