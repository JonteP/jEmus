#include <stdint.h>
#include <stdlib.h>
#include "globals.h"

static inline void bitset(uint8_t * inp, uint8_t val, uint8_t b);
static inline void donmi(), soft_reset();

void bitset(uint8_t * inp, uint8_t val, uint8_t b) {
	val ? (*inp = *inp | (1 << b)) : (*inp = *inp & ~(1 << b));
}

void donmi() {
	cpu[sp--] = ((pc) & 0xff00) >> 8;
	cpu[sp--] = ((pc) & 0xff);
	if (isnmi)
		cpu[sp--] = (flag & 0xef); /* clear b flag */
	else
		cpu[sp--] = (flag | 0x10); /* set b flag */
	pc = (cpu[nmi + 1] << 8) + cpu[nmi];
	bitset(&flag, 1, 2); /* set I flag */
}

void soft_reset () {
	pc = (cpu[rst + 1] << 8) + cpu[rst];
	bitset(&flag, 1, 2); /* set I flag */
}
