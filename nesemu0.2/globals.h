#ifndef GLOBALS_H_
#define GLOBALS_H_
#include <stdint.h>

#define PRG_BANK 0x1000
#define CHR_BANK 0x400

typedef struct gameInfos_ {
	char *title;
	char *year;
	char *publisher;
	char *serial;
} gameInfos;
typedef struct gameFeatures_ {
	char slot[20];
	char pcb[20];
	uint8_t mirroring;
	long chrSize;
	long prgSize;
	long wramSize;
	long bwramSize;
	long cramSize;
	long vrcPrg1;
	long vrcPrg0;
	long vrcChr;
	char mmc1_type[20];
	char mmc3_type[20];
} gameFeatures;
extern gameFeatures cart;
extern uint8_t  ctrb, ctrb2, ctr1, ctr2, nmiPulled,
			    nmiDelayed, vblank_period, mirroring[4][4], wramEnable, openBus;
extern uint8_t  *prg, *chr, *bwram, *wramSource, quit;
extern uint16_t ppu_wait, apu_wait;
extern int32_t ppucc;
extern int psize, csize;
extern FILE *logfile;

static inline void bitset(uint8_t * inp, uint8_t val, uint8_t b);

void bitset(uint8_t * inp, uint8_t val, uint8_t b) {
	val ? (*inp = *inp | (1 << b)) : (*inp = *inp & ~(1 << b));
}

#endif
