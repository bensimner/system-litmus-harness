#include "lib.h"

extern void MP_pos(void);
extern void MP_dmb_svc(void);
extern void MP_dmb_eret(void);

uint64_t NUMBER_OF_RUNS = 1000UL;
uint8_t ENABLE_PGTABLE = 0;

int main(void) {
  /** warning:
   * ensure ENABLE_PGTABLE is set to 0 for exceptions tests
   */
  MP_pos();
  MP_dmb_svc();
  MP_dmb_eret();
  return 0;
}
