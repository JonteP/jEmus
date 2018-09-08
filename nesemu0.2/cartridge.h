#ifndef CARTRIDGE_H_
#define CARTRIDGE_H_

#include <stdint.h>

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
extern int psize, csize;
extern uint8_t *prg, *chr, *bwram, *wramSource, mirroring[4][4], wramEnable;

void load_rom(), close_rom();

#endif /* CARTRIDGE_H_ */
