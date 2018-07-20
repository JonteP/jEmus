#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>	/* malloc; exit */
#include <string.h>	/* memcpy */
#include <time.h>
#include <unistd.h>
#include "globals.h"
#include "nestools.h"
#include "SDL.h"
#include "ppu.h"
#include "apu.h"
#include "6502.h"

/* constants */
const uint8_t id[4] = { 0x4e, 0x45, 0x53, 0x1a };
const uint16_t block = 0x2000;

int psize, csize;

uint8_t oamaddr, mirrmode, nmiVblankTriggered, mapper;
uint8_t vblank_period = 0, ppuStatus_nmiOccurred = 0, spritezero = 0,
		quit = 0, ctrb = 0, ctrb2 = 0, ctr1 = 0, ctr2 = 0, nmiAlreadyDone = 0, nmi_output = 0,
		a = 0x00, x = 0x00, y = 0x00, nmiDelayed = 0;
uint8_t header[0x10], oam[0x100] = { 0 }, vram[0x4000] = { 0 }, flag = 0x34,
		cpu[0x10000] = { 0 }, spriteof = 0;
uint8_t *prg, *chr, *bpattern = &vram[0], *spattern = &vram[0x1000];
uint16_t pc, namet, namev, nameadd, scrollx = 0, ppu_wait = 0, apu_wait = 0, nmi_wait = 0;
uint16_t nmi = 0xfffa, rst = 0xfffc, irq = 0xfffe, sp = 0x1fd;
uint8_t mirroring[4][4] = { { 0, 0, 1, 1 },
							{ 0, 1, 0, 1 },
							{ 0, 0, 0, 0 },
							{ 1, 1, 1, 1 } };
int32_t cpucc = 0;

FILE *rom, *logfile;

int main() {
	rom = fopen("/home/jonas/eclipse-workspace/"
			"mmc3/alien3.nes", "rb");
	if (rom == NULL) {
		printf("Error: No such file\n");
		exit(EXIT_FAILURE);
	}
	fread(header, sizeof(header), 1, rom);
	for (int i = 0; i < sizeof(id); i++) {
		if (header[i] != id[i]) {
			printf("Error: Invalid iNES header!\n");
			exit(EXIT_FAILURE);
		}
	}

	/* read data from ROM */
	psize = block * header[4] * 2;
	csize = block * header[5];
	prg = malloc(psize);
	chr = malloc(csize);
	fread(prg, psize, 1, rom);
	fread(chr, csize, 1, rom);
	fclose(rom);

	mirrmode = (header[6] & 1);
	/* 0 = horizontal mirroring
	 * 1 = vertical mirroring
	 * 2 = one screen, low page
	 * 3 = one screen, high page
	 * 4 = 4 screen
	 */
	mapper = ((header[6] >> 4) & 0x0f) | (header[7] & 0xf0);
	switch (mapper) {
	case 0:		/* NROM */
		memcpy(&cpu[0x8000], &prg[0x00], psize);
		if (psize == 0x4000)
			memcpy(&cpu[0xc000], &prg[0x00], psize);
		memcpy(&vram[0], &chr[0], csize);
		break;
	case 1:		/* SxROM, MMC1 - see also mappers 105, 155 */
		memcpy(&cpu[0x8000], &prg[psize-0x8000], 0x8000);
		memcpy(&vram[0], &chr[0], 0x2000);
		break;
	case 2:		/* UxROM - see also mappers 94, 180 */
		memcpy(&cpu[0x8000], &prg[psize-0x8000], 0x8000);
		memcpy(&vram[0], &chr[0], 0x2000);
		break;
	case 3:		/* NROM */
		memcpy(&cpu[0x8000], &prg[0x00], psize);
		if (psize == 0x4000)
			memcpy(&cpu[0xc000], &prg[0x00], psize);
		break;
	case 4:		/* TxROM */
		memcpy(&cpu[0xc000], &prg[psize-0x4000], 0x4000);
		break;
	case 7:		/* AxROM */
		memcpy(&cpu[0x8000], &prg[0], 0x8000);
		memcpy(&vram[0], &chr[0], 0x2000);
		break;
	default:
		printf("Error: unsupported Mapper: %i\n", mapper);
		exit(EXIT_FAILURE);
		break;
	}

	logfile = fopen("/home/jonas/eclipse-workspace/logfile.txt","w");
	if (logfile==NULL)
		printf("Error: Could not create logfile\n");

	power_reset(0);
	init_graphs(WPOSX, WPOSY, WWIDTH, WHEIGHT, SWIDTH, SHEIGHT);
	init_sounds();
	init_time();

	while (quit == 0) {
		/*	fprintf(logfile,"%04X %02X\t\t A:%02X X=%02X Y:%02X P:%02X SP:%02X CYC:%i\n",pc,cpu[pc],a,x,y,flag,sp,ppudot); */
			if (nmiDelayed) {
				nmiDelayed = 0;
			}
			run_ppu(ppu_wait);
			run_apu(apu_wait);
			opdecode(cpu[pc++]);
		/*	if (sp<0x100 || sp>0x1ff)
				printf("Error: Stack pointer\n"); */

			/* Interrupt handling */
			if (nmiIsTriggered >= ppucc-1) /* correct behavior? Probably depends on opcode */
				nmiDelayed = 1;
			if (nmi_output && nmiIsTriggered && !nmiAlreadyDone && !nmiDelayed) {
				apu_wait += 7 * 2;
				ppu_wait += 7 * 3;
				donmi();
				nmiAlreadyDone = 1;
				nmiIsTriggered = 0;
			}

	}
	fclose(logfile);
	kill_graphs();
}
