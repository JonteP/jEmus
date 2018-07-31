#ifndef NESTOOLS_H_
#define NESTOOLS_H_
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "globals.h"
#include "apu.h"
#include "ppu.h"
#include "6502.h"
#include "mapper.h"

static inline void bitset(uint8_t * inp, uint8_t val, uint8_t b);
static inline void donmi(void), power_reset(int mode), nametable_address (void);

void bitset(uint8_t * inp, uint8_t val, uint8_t b) {
	val ? (*inp = *inp | (1 << b)) : (*inp = *inp & ~(1 << b));
}

void donmi() {
	*cpuread(sp--) = ((pc) & 0xff00) >> 8;
	*cpuread(sp--) = ((pc) & 0xff);
	if (nmiVblankTriggered)
		*cpuread(sp--) = (flag & 0xef); /* clear b flag */
	else
		*cpuread(sp--) = (flag | 0x10); /* set b flag */
	pc = (*cpuread(nmi + 1) << 8) + *cpuread(nmi);
	bitset(&flag, 1, 2); /* set I flag */
}

void power_reset (int mode) {
	if(!strcmp(cart.slot,"nrom")) {
		reset_nrom();
	} else if(!strcmp(cart.slot,"sxrom") ||
				!strcmp(cart.slot,"sxrom_a") ||
					!strcmp(cart.slot,"sorom") ||
						!strcmp(cart.slot,"sorom_a")) {
		reset_mmc1();
	} else if(!strcmp(cart.slot,"uxrom") ||
				!strcmp(cart.slot,"un1rom") ||
					!strcmp(cart.slot,"unrom_cc")) {
		reset_uxrom();
	} else if (!strcmp(cart.slot,"cnrom")) {
		reset_cnrom();
	} else if (!strcmp(cart.slot,"axrom")) {
		reset_axrom();
	} else if (!strcmp(cart.slot,"txrom")) {
		reset_mmc3();
	}

	/*	case 4:		 TxROM */
	/*		memcpy(&cpu[0xc000], &prg[psize-0x4000], 0x4000);
			break;

		case 23:		/* VRC2/4 */
	/*		memcpy(&cpu[0xc000], &prg[psize-0x4000], 0x4000);
			break;
		default:
			printf("Error: unsupported Mapper: %i\n", mapper);
			exit(EXIT_FAILURE);
			break;
		} */
	pc = (*cpuread(rst + 1) << 8) + *cpuread(rst);
	bitset(&flag, 1, 2); /* set I flag */
	bpattern = &chrSlot[0];
	spattern = &chrSlot[1];
	apuStatus = 0; /* silence all channels */
	noiseShift = 1;
	dmcOutput = 0;
	if (!mode) { /* TODO: what is correct behavior? */
		frameInt = 1;
		apuFrameCounter = 0;
	}
}

void nametable_address () {
	switch (cart.mirroring) {
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
