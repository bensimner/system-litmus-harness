#ifndef ASM_BARRIERS_H
#define ASM_BARRIERS_H

/* barriers and wrappers */
#if DEBUG_DISABLE_WFE
#define wfe() do { asm volatile ("nop" ::: "memory"); } while (0)
#define sev() do { asm volatile ("nop" ::: "memory"); } while (0)
#else
#define wfe() do { asm volatile ("wfe" ::: "memory"); } while (0)
#define sev() do { asm volatile ("sev" ::: "memory"); } while (0)
#define sevl() do { asm volatile ("sevl" ::: "memory"); } while (0)
#endif
#define dsb() do { asm volatile ("dsb sy" ::: "memory"); } while (0)
#define dmb() do { asm volatile ("dmb sy" ::: "memory"); } while (0)
#define isb() do { asm volatile ("isb" ::: "memory"); } while (0)
#define eret() do { asm volatile ("eret" ::: "memory"); } while (0)

#endif /* ASM_BARRIERS_H */