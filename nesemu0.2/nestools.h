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
static inline void power_reset(int mode), nametable_address (void);

void bitset(uint8_t * inp, uint8_t val, uint8_t b) {
	val ? (*inp = *inp | (1 << b)) : (*inp = *inp & ~(1 << b));
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
	} else if (!strcmp(cart.slot,"vrc2") ||
			!strcmp(cart.slot,"vrc4")) {
		reset_vrc24();
	} else if (!strcmp(cart.slot,"g101")) {
		reset_g101();
	} else
		reset_default();

	pc = (*cpuread(rst + 1) << 8) + *cpuread(rst);
	bitset(&flag, 1, 2); /* set I flag */
	apuStatus = 0; /* silence all channels */
	noiseShift = 1;
	dmcOutput = 0;
	if (!mode) { /* TODO: what is correct behavior? */
		frameInt = 1;
		apuFrameCounter = 0;
	}
}

#endif
