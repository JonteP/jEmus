#include <stdint.h>
#include <stdlib.h>
#include "globals.h"

static inline void bitset(uint8_t * inp, uint8_t val, uint8_t b);
static inline void donmi();


void bitset(uint8_t * inp, uint8_t val, uint8_t b) {
	val ? (*inp = *inp | (1 << b)) : (*inp = *inp & ~(1 << b));
}

void donmi() {
	sp_cnt = 3;
	sp_buff[0] = ((pc) & 0xff00) >> 8;
	sp_buff[1] = ((pc) & 0xff);
	if (isnmi)
		sp_buff[2] = (flag & 0xef); /* clear b flag */
	else
		sp_buff[2] = (flag | 0x10); /* set b flag */
	pcbuff = (cpu[nmi + 1] << 8) + cpu[nmi];
	bitset(&flagbuff, 1, 2); /* int flag */
	dest = &vval;
	printf("push at nmi\n");
}
