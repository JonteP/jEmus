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
extern uint8_t  ctrb, ctrb2, ctr1, ctr2, nmi_output, nmiAlreadyDone,
			    spriteof, nmiDelayed, nmiVblankTriggered, vblank_period, mapper, oneScreen, mirroring[4][4], wramEnable, openBus;
extern uint8_t  *prg, *chr, cpu[0x10000], quit;
extern uint16_t paddr, namev, namet, nameadd, scrollx, ppu_wait, apu_wait;
extern int16_t ppudot;
extern int32_t ppucc, cpucc;
extern int psize, csize;
extern FILE *logfile;

#endif
