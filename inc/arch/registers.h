#ifndef ASM_REGISTERS_H
#define ASM_REGISTERS_H


#define FIELD_STARTS_AT(i) i
#define FIELD(startsat, width, value)



#define SPSR_EL 2
#define SPSR_SP 0

/* register read/write */
#define read_sysreg(r) ({  \
    uint64_t reg;  \
    asm volatile("mrs %[reg], " #r : [reg] "=r" (reg));  \
    reg;  \
})

#define read_sysreg_el1(r) read_sysreg(r##_el1)
#define read_sysreg_el2(r) read_sysreg(r##_el2)

#define __read_sysreg_el(reg, r, el) ({ \
  uint64_t reg; \
  if (el == 1)  \
    reg = read_sysreg_el1(r); \
  else  \
    reg = read_sysreg_el2(r); \
  reg;  \
})

#define read_sysreg_el(r, el) __read_sysreg_el(FRESH_VAR, r, el)

#define write_sysreg(v, r) do {  \
    asm volatile("msr " #r ", %[reg]" : : [reg] "rZ" (v));  \
} while (0)

#define write_sysreg_el1(v, r) write_sysreg(v, r##_el1)
#define write_sysreg_el2(v, r) write_sysreg(v, r##_el2)

#define write_sysreg_el(v, r, el) do { \
  if (el == 1)  \
    write_sysreg_el1(v, r); \
  else  \
    write_sysreg_el2(v, r); \
} while (0)

#define read_reg(r) ({  \
    uint64_t reg;  \
    asm volatile("mov %[reg], " #r : [reg] "=r" (reg));  \
    reg;  \
})

#define write_reg(v, r) do {  \
    asm volatile("mov " #r ", %[reg]" : : [reg] "rZ" (v));  \
} while (0)


#endif /* ASM_REGISTERS_H */
