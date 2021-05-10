#ifndef ASM_H
#define ASM_H

#include <stdint.h>

/* stores and loads */
void writeb(uint8_t byte, uint64_t addr);
void writew(uint32_t word, uint64_t addr);

uint8_t  readb(uint64_t addr);
uint32_t readw(uint64_t addr);

#define SYS_SPSR spsr
#define SYS_ELR elr
#define SYS_VBAR vbar
#define SYS_FAR far

/** read the clock register */
uint64_t read_clk(void);
uint64_t read_clk_freq(void);

extern uint64_t INIT_CLOCK;
extern uint64_t TICKS_PER_SEC;

#endif /* ASM_H */
