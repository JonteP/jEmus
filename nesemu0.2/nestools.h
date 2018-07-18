#ifndef NESTOOLS_H_
#define NESTOOLS_H_
#include <stdint.h>
#include <stdlib.h>
#include "globals.h"
#include "apu.h"

static inline void bitset(uint8_t * inp, uint8_t val, uint8_t b);
static inline void donmi(void), power_reset(int mode), nametable_address (void);

void bitset(uint8_t * inp, uint8_t val, uint8_t b) {
	val ? (*inp = *inp | (1 << b)) : (*inp = *inp & ~(1 << b));
}

void donmi() {
	cpu[sp--] = ((pc) & 0xff00) >> 8;
	cpu[sp--] = ((pc) & 0xff);
	if (nmiVblankTriggered)
		cpu[sp--] = (flag & 0xef); /* clear b flag */
	else
		cpu[sp--] = (flag | 0x10); /* set b flag */
	pc = (cpu[nmi + 1] << 8) + cpu[nmi];
	bitset(&flag, 1, 2); /* set I flag */
}

void power_reset (int mode) {
	pc = (cpu[rst + 1] << 8) + cpu[rst];
	bitset(&flag, 1, 2); /* set I flag */
	apuStatus = 0; /* silence all channels */
	noiseShift = 1;
	dmcOutput = 0;
	if (!mode) { /* TODO: what is correct behavior? */
		frameInt = 1;
		apuFrameCounter = 0;
	}
}

void nametable_address () {
	switch (mirrmode) {
		case 0:
			{uint16_t mirroring[4] = { 0, 0, 1, 1 };
			nameadd = (namev&0xf3ff) | (mirroring[(namev>>10)]<<10);
			break;}
		case 1:
			{uint16_t mirroring[4] = { 0, 1, 0, 1 };
			nameadd = (namev&0xf3ff) | (mirroring[(namev>>10)]<<10);
			break;}
		case 2:
			{uint16_t mirroring[4] = { 0, 0, 0, 0 };
			nameadd = (namev&0xf3ff) | (mirroring[(namev>>10)]<<10);
			break;}
		case 3:
			{uint16_t mirroring[4] = { 1, 1, 1, 1 };
			nameadd = (namev&0xf3ff) | (mirroring[(namev>>10)]<<10);
			break;}
		}
}

#endif
