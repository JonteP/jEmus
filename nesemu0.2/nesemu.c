#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>	/* malloc; exit */
#include <string.h>	/* memcpy */
#include <time.h>
#include <unistd.h>
#include "globals.h"
#include "nestools.h"
#include "SDL.h"
#include "6502.h"

#define WWIDTH 1024
#define WHEIGHT 768
#define SWIDTH 256
#define SHEIGHT 240
#define WPOSX 100
#define WPOSY 100
#define FRAMETIME 16667 /*16667*/

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

SDL_Window *win = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *bitmapTex = NULL;
SDL_Surface *bitmapSurface = NULL;
SDL_Surface *nameSurface = NULL;
SDL_Event event;
SDL_Color colors[64];
uint8_t colarray[] = { 124, 124, 124, 0, 0, 252, 0, 0, 188, 68, 40, 188, 148, 0,
		132, 168, 0, 32, 168, 16, 0, 136, 20, 0, 80, 48, 0, 0, 120, 0, 0, 104,
		0, 0, 88, 0, 0, 64, 88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 188, 188, 188, 0,
		120, 248, 0, 88, 248, 104, 68, 252, 216, 0, 204, 228, 0, 88, 248, 56, 0,
		228, 92, 16, 172, 124, 0, 0, 184, 0, 0, 168, 0, 0, 168, 68, 0, 136, 136,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 248, 248, 248, 60, 188, 252, 104, 136, 252,
		152, 120, 248, 248, 120, 248, 248, 88, 152, 248, 120, 88, 252, 160, 68,
		248, 184, 0, 184, 248, 24, 88, 216, 84, 88, 248, 152, 0, 232, 216, 120,
		120, 120, 0, 0, 0, 0, 0, 0, 252, 252, 252, 164, 228, 252, 184, 184, 248,
		216, 184, 248, 248, 184, 248, 248, 164, 192, 240, 208, 176, 252, 224,
		168, 248, 216, 120, 216, 248, 120, 184, 248, 184, 184, 248, 216, 0, 252,
		252, 248, 216, 248, 0, 0, 0, 0, 0, 0 };

static inline void soft_reset ();
static inline void init_graphs(int, int, int, int, int, int);
static inline void io_handle();
static inline void draw_nt();
static inline void render_line();
static inline void render_frame();

int main() {
/* parse iNES header ppu_vbl_nmi/rom_singles/05-nmi_timing */

	rom = fopen("/home/jonas/eclipse-workspace/"
			"nrom/dk.nes", "rb");
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

	for (int i = 0; i < 64; i++) {
		colors[i].r = colarray[i * 3];
		colors[i].g = colarray[i * 3 + 1];
		colors[i].b = colarray[i * 3 + 2];
	}

	SDL_Init(SDL_INIT_VIDEO);

	init_graphs(WPOSX, WPOSY, WWIDTH, WHEIGHT, SWIDTH, SHEIGHT);
	/*init_graphs(100, 500, WWIDTH, WHEIGHT, 256*2, 240*2); */

	clock_t start = clock(), diff;

	while (quit == 0) {
		if (ppudot == 341) {
			scanline++;
			printf("Scanline is: %i\n",scanline);
			ppudot = 0;
		}
		if (scanline == 262) {
			scanline = 0;
			frame++;
			printf("Frame is %i\n",frame);
			ppucc = 0;
		/*	printf("PPU Cycles: %i\tCPU cycles: %i\n",ppucc,cpucc-(cpu_wait/3));
			cpucc = (cpu_wait/3); */
			if (frame%2 && (cpu[0x2001] & 0x18))
				ppudot++;
		}
		if (scanline == 241 && ppudot == 1) {
			printf("VBLANK at PPU cycle: %i\n",ppucc);
			isvblank = 1; /* set vblank */
			vblank_period = 1;
		} else if (scanline == 261) {
			if (ppudot == 1) {
				isvblank = 0; /* clear vblank */
				io_handle();
				render_frame();
				if (throttle) {
					diff = clock() - start;
					usleep(FRAMETIME - (diff % FRAMETIME));
					start = clock();
				}
				spritezero = 0;
			}
			if ((cpu[0x2001] & 0x18) && ppudot == 256) {
				/* dot 256 (y scroll) */
						if ((namev & 0x7000) != 0x7000)
							namev += 0x1000;
						else {
							namev &= ~0x7000;
							uint8_t coarsey = ((namev & 0x3e0) >> 5);
							if (coarsey == 29) {
								coarsey = 0;
								namev ^= 0x0800;
						  } else if (coarsey == 31)
								coarsey = 0;
							 else
								coarsey++;
								namev = (namev & ~0x03e0) | (coarsey << 5);
						}
			}
			if ((cpu[0x2001] & 0x18) && ppudot == 257)
				namev = (namev & 0xfbe0) | (namet & 0x41f); /* reset x scroll */
			if ((cpu[0x2001] & 0x18) && ppudot >= 280 && ppudot <= 304)
				namev = (namev & 0x841f) | (namet & 0x7be0); /* reset Y scroll */
			if ((cpu[0x2001] & 0x18) && ppudot >= 257 && ppudot <= 320)
				/* (reset OAM) */
				oamaddr = 0; /* only if rendering active? */
			nmi_allow = 1;
			vblank_period = 0;
		} else if (scanline < 240) {
			if ((cpu[0x2001] & 0x18) && ppudot == 256) {
				render_line();
				/* dot 256 (y scroll) */
						if ((namev & 0x7000) != 0x7000)
							namev += 0x1000;
						else {
							namev &= ~0x7000;
							uint8_t coarsey = ((namev & 0x3e0) >> 5);
							if (coarsey == 29) {
								coarsey = 0;
								namev ^= 0x0800;
						  } else if (coarsey == 31)
								coarsey = 0;
							 else
								coarsey++;
								namev = (namev & ~0x03e0) | (coarsey << 5);
						}
		   } if ((cpu[0x2001] & 0x18) && ppudot == 257)
				/* (x position) */
				namev = (namev & 0xfbe0) | (namet & 0x41f); /* only if rendering enabled */
			 if ((cpu[0x2001] & 0x18) && ppudot >= 257 && ppudot <= 320)
						/* (reset OAM) */
						oamaddr = 0; /* only if rendering active? */
		}

		if (cpu_wait == 0) {
			pc = pcbuff;
			sp = sp_add;
			flag = flagbuff;
			if (rw == 1)
				memread();
			else if (rw == 2)
				memwrite();
			*dest = vval;
			if (sp_cnt > 0) {
				for (int i = 0;i<sp_cnt;i++) {
					cpu[sp--] = sp_buff[i];
				}
			} else if (sp_cnt < 0)
				sp -= sp_cnt;

		/*	fprintf(logfile,"%04X %02X\t\t A:%02X X=%02X Y:%02X P:%02X SP:%02X CYC:%i\n",pc,cpu[pc],a,x,y,flag,sp,ppudot); */
			if (vblank_wait) {
				vblank_wait = 0;
				cpu_wait += 7 * 3;
				donmi();
				printf("NMI triggered at PPU cycle: %i, plus: %i\n",ppucc, cpu_wait);
				nmi_allow = 0;
			} else
				opdecode(cpu[pc++]);
			if (sp<0x100 || sp>0x1ff)
				printf("Error: Stack pointer\n");
		}
		/* Interrupt handling */
		if (nmi_output && isvblank && nmi_allow) {
			vblank_wait = 1;
		}
		cpu_wait--;
		ppudot++;
		ppucc++;
	}
	fclose(logfile);
	SDL_FreeSurface(bitmapSurface);
	SDL_DestroyTexture(bitmapTex);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(win);
	atexit(SDL_Quit);
	exit(EXIT_SUCCESS);
}

void init_graphs(int wpx, int wpy, int ww, int wh, int sw, int sh) {
	win = SDL_CreateWindow("Nesemu", wpx, wpy, ww, wh, 0);
	renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"2");
	bitmapSurface = SDL_CreateRGBSurface(0, sw, sh, 8, 0, 0, 0, 0);
	SDL_SetPaletteColors(bitmapSurface->format->palette, colors, 0, 64);
	SDL_LockSurface(bitmapSurface);
}

void render_frame() {
	SDL_UnlockSurface(bitmapSurface);
	bitmapTex = SDL_CreateTextureFromSurface(renderer, bitmapSurface);
	if (bitmapTex == NULL)
		printf("Error %s\n",SDL_GetError());
	SDL_FreeSurface(bitmapSurface);
	SDL_RenderCopy(renderer, bitmapTex, NULL, NULL);
	SDL_DestroyTexture(bitmapTex);
	SDL_RenderPresent(renderer);
	SDL_RenderClear(renderer);
	bitmapSurface = SDL_CreateRGBSurface(0, SWIDTH, SHEIGHT, 8, 0, 0, 0, 0);
	SDL_SetPaletteColors(bitmapSurface->format->palette, colors, 0, 64);
	SDL_LockSurface(bitmapSurface);
}

void render_line() {
uint8_t	nsprites = 0;
uint8_t pmap[SWIDTH] = {0};
uint8_t blank_line[SWIDTH] = {palette[0]};
/* fill sprite buffer */
	if (cpu[0x2001] & 0x10) {
		for (uint8_t i = (oamaddr>>2); i < 64; i++) {
			objsrc = oam + (i<<2);
			if ((objsrc[0]+1) <= scanline && scanline <= (objsrc[0]+8+ (((cpu[0x2000]>>5)&1)*8))) {
				if (nsprites < 8) {
					spritebuff[nsprites]=(i<<2);
					nsprites++;
				} else
					spriteof = 1;
			}
		}
	}

/* render background */
	tiledest = bitmapSurface->pixels + scanline * SWIDTH;
	if (cpu[0x2001] & 0x08) {
		for (int ncol = 1-((cpu[0x2001]&2)>>1); ncol < 32; ncol++) {
			    tilesrc = bpattern + 16 * vram[0x2000 + (namev & (mirrmode ? 0x7ff : 0xbff))];
				attsrc = vram[0x23c0 | (namev & (mirrmode ? 0x400 : 0x800 )) | ((namev >>4) & 0x38) | ((namev >> 2) & 0x07)];
				/* X: shift right 1 AND 1 Y: shift right 4 AND 1 * 2*/
				npal = ((attsrc >> (((namev >> 1) & 1) | ((namev >> 5) & 2) ) * 2) & 3);

				if ((namev & 0x1f) == 0x1f) {
					namev &= ~0x1f;
					namev ^= 0x0400;
				} else
					namev++;
			    ntilesrc = bpattern + 16 * vram[0x2000 + (namev & (mirrmode ? 0x7ff : 0xbff))];
				nattsrc = vram[0x23c0 | (namev & (mirrmode ? 0x400 : 0x800 )) | ((namev >>4) & 0x38) | ((namev >> 2) & 0x07)];
				nnpal = ((nattsrc >> (((namev >> 1) & 1) | ((namev >> 5) & 2) ) * 2) & 3);
				for (int pcol = 0; pcol < 8; pcol++) {
	/* TODO: AND palette with 0x30 in greyscale mode */
					if ((pcol + scrollx) < 8) {
						pmap[pcol + 8*ncol] = ((tilesrc[(namev >> 12)] & (1<<(7-(pcol+scrollx))) ) ? 1 : 0) + ( (tilesrc[(namev >> 12) + 8] & (1<<(7-(pcol+scrollx)))) ? 2 : 0);
						if (pmap[pcol + 8*ncol])
							tiledest[pcol + 8*ncol] = palette[npal * 4 + pmap[pcol + 8*ncol]];
						else
							tiledest[pcol + 8*ncol] = palette[0];
					}
					else {
						pmap[pcol + 8*ncol] = ((ntilesrc[(namev >> 12)] & (1<<(7-((pcol+scrollx)-8))) ) ? 1 : 0) + ( (ntilesrc[(namev >> 12) + 8] & (1<<(7-((pcol+scrollx)-8)))) ? 2 : 0);
						if (pmap[pcol + 8*ncol])
							tiledest[pcol + 8*ncol] = palette[nnpal * 4 + pmap[pcol + 8*ncol]];
						else
							tiledest[pcol + 8*ncol] = palette[0];
					}
				}
		}

	} else
		memcpy(&tiledest[0], &blank_line[0], SWIDTH);

/* render sprites */
	if (cpu[0x2001] & 0x10) {
		for (uint8_t i = 0; i < nsprites; i++) {
			objsrc = oam + spritebuff[nsprites-i-1];
			uint8_t yoff = scanline - (objsrc[0]+1);
			uint8_t flipx = ((objsrc[2] >> 6) & 1);
			uint8_t flipy = ((objsrc[2] >> 7) & 1);
			tilesrc = (cpu[0x2000]&0x20) ? spattern + (objsrc[1]&1) * 0x1000 + ((objsrc[1]&0xfe)<<4) + ( ((yoff<<1)^(flipy<<4)) &0x10) : spattern + (objsrc[1]<<4);
			tiledest = bitmapSurface->pixels + objsrc[3] + scanline * SWIDTH;
			npal = (objsrc[2] & 3) + 4;
			for (int pcol = 0; pcol < 8; pcol++) {
				if ( (objsrc[3]+pcol) > 7 * (1-( (cpu[0x2001]>>2)&1) ) ) { /* check if outside left column mask */
					p = ((((tilesrc[ (yoff&7) + flipy * (7 - 2 * (yoff&7))]
						>>  (7 - pcol - flipx * (7 - 2 * pcol))) & 1) ? 1 : 0)
				   + (((tilesrc[((yoff&7) + flipy * (7 - 2 * (yoff&7))) + 8]
								>> (7 - pcol - flipx * (7 - 2 * pcol))) & 1) ? 2 : 0));
					if (p && (pmap[pcol + objsrc[3]]) && !(nsprites-i-1) && !spritezero && ((objsrc[3]+pcol) != 255)) /* sprite zero hit */
						spritezero = 1;
					if (p && !(objsrc[2] & 0x20))
						tiledest[pcol] = palette[npal * 4 + p];
					else if (p && (objsrc[2] & 0x20) && !(pmap[pcol + objsrc[3]]))
						tiledest[pcol] = palette[npal * 4 + p];
				}
			}
		}
	}
}

static inline void draw_nt() {
	/* render background */

		for (int ntable = 0; ntable < 4; ntable++) {

			for (int nrow = 0; nrow < 30; nrow++) {


				for (int ncol = 0; ncol < 32; ncol++) {

					tilesrc = bpattern + 16 * vram[0x2000 + ncol + (nrow * 32) + ((ntable&(mirrmode+2-(2*mirrmode))) * 0x400)];
					tiledest = bitmapSurface->pixels + (ncol*8 + ((ntable&1) *SWIDTH) + (nrow*16*SWIDTH) + ((ntable>>1) * 480*SWIDTH));
					attsrc = vram[(0x23c0+0x400*ntable) + (nrow >> 2) * 8 + (ncol >> 2)];
					npal = (attsrc >> (((ncol % 4) - (ncol % 2))
									+ (((nrow % 4) - (nrow % 2)) * 2))) & 3;
					for (int rr = 0; rr < 8; rr++) {
		/* TODO: AND palette with 0x30 in greyscale mode */
						tiledest[SWIDTH*2 * rr] = palette[npal * 4
								+ (((tilesrc[rr] & 0x80) ? 1 : 0)
										+ ((tilesrc[rr + 8] & 0x80) ? 2 : 0))];
						tiledest[SWIDTH*2 * rr + 1] = palette[npal * 4
								+ (((tilesrc[rr] & 0x40) ? 1 : 0)
										+ ((tilesrc[rr + 8] & 0x40) ? 2 : 0))];
						tiledest[SWIDTH*2 * rr + 2] = palette[npal * 4
								+ (((tilesrc[rr] & 0x20) ? 1 : 0)
										+ ((tilesrc[rr + 8] & 0x20) ? 2 : 0))];
						tiledest[SWIDTH*2 * rr + 3] = palette[npal * 4
								+ (((tilesrc[rr] & 0x10) ? 1 : 0)
										+ ((tilesrc[rr + 8] & 0x10) ? 2 : 0))];
						tiledest[SWIDTH*2 * rr + 4] = palette[npal * 4
								+ (((tilesrc[rr] & 0x08) ? 1 : 0)
										+ ((tilesrc[rr + 8] & 0x08) ? 2 : 0))];
						tiledest[SWIDTH*2 * rr + 5] = palette[npal * 4
								+ (((tilesrc[rr] & 0x04) ? 1 : 0)
										+ ((tilesrc[rr + 8] & 0x04) ? 2 : 0))];
						tiledest[SWIDTH*2 * rr + 6] = palette[npal * 4
								+ (((tilesrc[rr] & 0x02) ? 1 : 0)
										+ ((tilesrc[rr + 8] & 0x02) ? 2 : 0))];
						tiledest[SWIDTH*2 * rr + 7] = palette[npal * 4
								+ (((tilesrc[rr] & 0x01) ? 1 : 0)
										+ ((tilesrc[rr + 8] & 0x01) ? 2 : 0))];
					}
				}
			}
		}

		SDL_UnlockSurface(bitmapSurface);
		bitmapTex = SDL_CreateTextureFromSurface(renderer, bitmapSurface);
		SDL_FreeSurface(bitmapSurface);
		SDL_RenderCopy(renderer, bitmapTex, NULL, NULL);
		SDL_DestroyTexture(bitmapTex);
		SDL_RenderPresent(renderer);
		bitmapSurface = SDL_CreateRGBSurface(0, SWIDTH*2, SHEIGHT*2, 8, 0, 0, 0, 0);
		SDL_SetPaletteColors(bitmapSurface->format->palette, colors, 0, 64);
		SDL_LockSurface(bitmapSurface);

}

void io_handle() {
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		/* Keyboard event */
		/* Pass the event data onto PrintKeyInfo() */
		case SDL_KEYDOWN:
			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_ESCAPE:
				printf("Quitting\n");
				quit = 1;
				break;
			case SDL_SCANCODE_F3:
				printf("Resetting\n");
				soft_reset();
				break;
			case SDL_SCANCODE_F10:
				throttle ^= 1;
				break;
			case SDL_SCANCODE_UP:
				bitset(&ctr1, 1, 4);
				break;
			case SDL_SCANCODE_DOWN:
				bitset(&ctr1, 1, 5);
				break;
			case SDL_SCANCODE_LEFT:
				bitset(&ctr1, 1, 6);
				break;
			case SDL_SCANCODE_RIGHT:
				bitset(&ctr1, 1, 7);
				break;
			case SDL_SCANCODE_RETURN:
				bitset(&ctr1, 1, 3);
				break;
			case SDL_SCANCODE_BACKSPACE:
				bitset(&ctr1, 1, 2);
				break;
			case SDL_SCANCODE_Z:
				bitset(&ctr1, 1, 1);
				break;
			case SDL_SCANCODE_X:
				bitset(&ctr1, 1, 0);
				break;
			case SDL_SCANCODE_I:
				bitset(&ctr2, 1, 4);
				break;
			case SDL_SCANCODE_K:
				bitset(&ctr2, 1, 5);
				break;
			case SDL_SCANCODE_J:
				bitset(&ctr2, 1, 6);
				break;
			case SDL_SCANCODE_L:
				bitset(&ctr2, 1, 7);
				break;
			case SDL_SCANCODE_1:
				bitset(&ctr2, 1, 3);
				break;
			case SDL_SCANCODE_2:
				bitset(&ctr2, 1, 2);
				break;
			case SDL_SCANCODE_A:
				bitset(&ctr2, 1, 1);
				break;
			case SDL_SCANCODE_S:
				bitset(&ctr2, 1, 0);
				break;
			default:
				break;
			}
			break;
		case SDL_KEYUP:
			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_UP:
				bitset(&ctr1, 0, 4);
				break;
			case SDL_SCANCODE_DOWN:
				bitset(&ctr1, 0, 5);
				break;
			case SDL_SCANCODE_LEFT:
				bitset(&ctr1, 0, 6);
				break;
			case SDL_SCANCODE_RIGHT:
				bitset(&ctr1, 0, 7);
				break;
			case SDL_SCANCODE_RETURN:
				bitset(&ctr1, 0, 3);
				break;
			case SDL_SCANCODE_BACKSPACE:
				bitset(&ctr1, 0, 2);
				break;
			case SDL_SCANCODE_Z:
				bitset(&ctr1, 0, 1);
				break;
			case SDL_SCANCODE_X:
				bitset(&ctr1, 0, 0);
				break;
			case SDL_SCANCODE_I:
				bitset(&ctr2, 0, 4);
				break;
			case SDL_SCANCODE_K:
				bitset(&ctr2, 0, 5);
				break;
			case SDL_SCANCODE_J:
				bitset(&ctr2, 0, 6);
				break;
			case SDL_SCANCODE_L:
				bitset(&ctr2, 0, 7);
				break;
			case SDL_SCANCODE_1:
				bitset(&ctr2, 0, 3);
				break;
			case SDL_SCANCODE_2:
				bitset(&ctr2, 0, 2);
				break;
			case SDL_SCANCODE_A:
				bitset(&ctr2, 0, 1);
				break;
			case SDL_SCANCODE_S:
				bitset(&ctr2, 0, 0);
				break;
			default:
				break;
			}
			break;
			/* SDL_QUIT event (window close) */
		case SDL_QUIT:
			break;
		default:
			break;
		}
	}
}

void soft_reset () {
	pc = (cpu[rst + 1] << 8) + cpu[rst];
	dest = &vval; /* dummy pointer */
	pcbuff = pc;
	sp_add = sp;
	flagbuff = flag;
	bitset(&flag, 1, 2); /* set I flag */
}
