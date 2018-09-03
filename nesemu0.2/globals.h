#ifndef GLOBALS_H_
#define GLOBALS_H_
#include <stdint.h>

#define PRG_BANK 0x1000
#define CHR_BANK 0x400

extern uint8_t  ctrb, ctrb2, ctr1, ctr2, nmiPulled,
			    nmiDelayed, vblank_period, openBus;
extern uint8_t  quit;
extern uint16_t ppu_wait, apu_wait;
extern int32_t ppucc;
extern FILE *logfile;

static inline void bitset(uint8_t * inp, uint8_t val, uint8_t b);

void bitset(uint8_t * inp, uint8_t val, uint8_t b) {
	val ? (*inp = *inp | (1 << b)) : (*inp = *inp & ~(1 << b));
}

#endif
