#include "lib.h"

void switch_to_vm_stack(void) {
  uint64_t cpu = get_cpu();
  uint64_t sp = read_reg(sp);
  uint64_t new_sp = sp - STACK_PYS_THREAD_BOT_EL1(cpu) + STACK_MMAP_THREAD_BOT_EL1(cpu);
  write_reg(new_sp, sp);
}

void switch_to_pys_stack(void) {
  uint64_t cpu = get_cpu();
  uint64_t sp = read_reg(sp);
  uint64_t new_sp = sp - STACK_MMAP_THREAD_BOT_EL1(cpu) + STACK_PYS_THREAD_BOT_EL1(cpu);
  write_reg(new_sp, sp);
}