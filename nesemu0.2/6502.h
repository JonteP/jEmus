#ifndef C6502_H_
#define C6502_H_
#include <stdint.h>

extern inline void opdecode(uint8_t);
extern inline uint8_t * cpuread(uint16_t);
extern inline uint8_t * ppuread(uint16_t);

extern uint8_t ppuController, ppuMask;
extern uint8_t *prgSlot[0x8];

#endif
