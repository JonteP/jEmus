#ifndef C6502_H_
#define C6502_H_
#include <stdint.h>

typedef enum {
    IRQ,
    NMI,
	BRK
} interrupt_t;

extern interrupt_t intFlag;
void opdecode(void), interrupt_handle(interrupt_t);
uint8_t * cpuread(uint16_t);
uint8_t * ppuread(uint16_t);

extern uint8_t ppuController, ppuMask, flag;
extern uint8_t *prgSlot[0x8];
extern uint16_t nmi, rst, irq, pc;

#endif
