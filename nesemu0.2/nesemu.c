#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>	/* malloc; exit */
#include <string.h>	/* memcpy */
#include <time.h>
#include <unistd.h>
#include "globals.h"
#include "nestools.h"
#include "SDL.h"

#define WWIDTH 1024
#define WHEIGHT 768
#define SWIDTH 256
#define SHEIGHT 240
#define WPOSX 100
#define WPOSY 100

extern void opdecode(uint8_t), init_time();
extern void init_graphs(int, int, int, int, int, int);
extern void run_ppu(uint16_t), kill_graphs();


/* debug vars */
uint8_t nmicount = 0;

/* constants */
const uint8_t id[4] = { 0x4e, 0x45, 0x53, 0x1a };
const uint16_t block = 0x2000;

int psize, csize;

int8_t sp_cnt;
uint8_t mm1_shift = 0,  mm1_buff, wram, prg_bank, chr_bank, prg_size, chr_size;
uint8_t oamaddr, mirrmode, p, attsrc, npal, nattsrc, nnpal, vraminc, rw,
		vval, isnmi, mapper, flagbuff, sp_buff[5];
uint8_t vblank_period = 0, isvblank = 0, spritezero = 0, vbuff = 0, ppureg = 0,
		quit = 0, w = 0, s = 0, ctrb = 0, ctrb2 = 0, ctr1 = 0, ctr2 = 0, nmi_allow = 1, nmi_output = 0,
		nmi_disable = 0, a = 0x00, x = 0x00, y = 0x00, vblank_wait = 0, throttle = 1;
uint8_t header[0x10], oam[0x100] = { 0 }, vram[0x4000] = { 0 }, flag = 0x34,
		cpu[0x10000] = { 0 }, dots[256] = {1}, spritebuff[8], spriteof = 0;
uint8_t *prg, *chr, *tilesrc, *ntilesrc, *tiledest, *pmap, *objsrc, *addval, *dest;
uint8_t *bpattern = &vram[0], *spattern = &vram[0x1000], *name = &vram[0x2000], *attrib, *palette = &vram[0x3f00],
		*palette_mirror = &vram[0x3f20];
uint16_t pc, tmp, addr, namet, namev, scrollx = 0, cpu_wait = 0, nmi_wait = 0, ppudot = 0;
uint16_t nmi = 0xfffa, rst = 0xfffc, irq = 0xfffe, scanline = 0, sp = 0x1fd, pcbuff, sp_add = 0x1fd;
uint32_t frame = 0, ppucc = 0, cpucc = 0;

FILE *rom, *logfile;

int main() {
/* parse iNES header ppu_vbl_nmi/rom_singles/05-nmi_timing */

	rom = fopen("/home/jonas/eclipse-workspace/"
			"nrom/ice.nes", "rb");
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
	mapper = ((header[6] >> 4) & 0x0f) | (header[7] & 0xf0);
	switch (mapper) {
	case 0:		/* NROM */
		memcpy(&cpu[0x8000], &prg[0x00], psize);
		if (psize == 0x4000)
			memcpy(&cpu[0xc000], &prg[0x00], psize);
		memcpy(&vram[0], &chr[0], csize);
		break;
	case 1:		/* SxROM, MMC1 */
		memcpy(&cpu[0x8000], &prg[psize-0x8000], 0x8000);
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

	/* initialize memory */

	soft_reset();

	init_graphs(WPOSX, WPOSY, WWIDTH, WHEIGHT, SWIDTH, SHEIGHT);
	/*init_graphs(100, 500, WWIDTH, WHEIGHT, 256*2, 240*2); */

	init_time();

	while (quit == 0) {

		/*	fprintf(logfile,"%04X %02X\t\t A:%02X X=%02X Y:%02X P:%02X SP:%02X CYC:%i\n",pc,cpu[pc],a,x,y,flag,sp,ppudot); */
			if (vblank_wait) {
				vblank_wait = 0;
			}
			opdecode(cpu[pc++]);
			if (sp<0x100 || sp>0x1ff)
				printf("Error: Stack pointer\n");
			run_ppu(cpu_wait);
			/* Interrupt handling */
			if (nmi_output && isvblank && nmi_allow && !vblank_wait) {
				cpu_wait += 7 * 3;
				run_ppu(cpu_wait);
				donmi();
				nmi_allow = 0;
			}

	}
	fclose(logfile);
	kill_graphs();
}
