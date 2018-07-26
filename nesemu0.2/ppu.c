#include "ppu.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include "globals.h"
#include "SDL.h"
#include "nestools.h"
#include "apu.h"



static inline void render_frame(), render_line(), io_handle(), y_scroll();
uint8_t throttle = 1, vblankSuppressed = 0;
int16_t ppudot = -1, scanline = 0;
uint32_t frame = 0, nmiIsTriggered = 0;
int32_t ppucc = -1;
clock_t start, diff;
SDL_Window *win;
SDL_Renderer *renderer;
SDL_Texture *bitmapTex;
SDL_Surface *bitmapSurface, *nameSurface;
SDL_Event event;
SDL_Color colors[64];

					/*       00      |      01      |      02      |      03      |*/
					/*  R  | G  | B  | R  | G  | B  | R  | G  | B  | R  | G  | B  |*/
uint8_t colarray[] = { 124, 124, 124,   0,   0, 252,   0,   0, 188,  68,  40, 188, /* 0x00 */
					   148,   0, 132, 168,   0,  32, 168,  16,   0, 136,  20,   0, /* 0x04 */
					    80,  48,   0,   0, 120,   0,   0, 104,   0,   0,  88,   0, /* 0x08 */
					     0,  64,  88,   0,   0,   0,   0,   0,   0,   0,   0,   0, /* 0x0c */
					   188, 188, 188,   0, 120, 248,   0,  88, 248, 104,  68, 252, /* 0x10 */
					   216,   0, 204, 228,   0,  88, 248,  56,   0, 228,  92,  16, /* 0x14 */
					   172, 124,   0,   0, 184,   0,   0, 168,   0,   0, 168,  68, /* 0x18 */
					     0, 136, 136,   0,   0,   0,   0,   0,   0,   0,   0,   0, /* 0x1c */
					   248, 248, 248,  60, 188, 252, 104, 136, 252, 152, 120, 248, /* 0x20 */
					   248, 120, 248, 248,  88, 152, 248, 120,  88, 252, 160,  68, /* 0x24 */
					   248, 184,   0, 184, 248,  24,  88, 216,  84,  88, 248, 152, /* 0x28 */
					     0, 232, 216, 120, 120, 120,   0,   0,   0,   0,   0,   0, /* 0x2c */
					   252, 252, 252, 164, 228, 252, 184, 184, 248, 216, 184, 248, /* 0x30 */
					   248, 184, 248, 248, 164, 192, 240, 208, 176, 252, 224, 168, /* 0x34 */
					   248, 216, 120, 216, 248, 120, 184, 248, 184, 184, 248, 216, /* 0x38 */
					     0, 252, 252, 248, 216, 248,   0,   0,   0,   0,   0,   0  /* 0x3c */
					};

void init_time () {
	start = clock();
}

void run_ppu (uint16_t ntimes) {

	while (ntimes) {
		ppudot++;
		ppucc++;

		if (ppudot == 341) {
		scanline++;
		ppudot = 0;
	}
	if (scanline == 262) {
		scanline = 0;
		frame++;
		if (frame%2 && (cpu[0x2001] & 0x18))
			ppudot++;
	}

/* VBLANK ONSET */
	if (scanline == 241 && ppudot == 1) {
		if (!vblankSuppressed) {
			ppuStatus_nmiOccurred = 1; /* set vblank */
			if (nmi_output) {
				nmiIsTriggered = ppucc;
			}
		}
		vblank_period = 1;

/* PRERENDER SCANLINE */
	} else if (scanline == 261) {
		if (ppudot == 1) {
			ppuStatus_nmiOccurred = 0; /* clear vblank */
			nmiIsTriggered = 0;
			vblankSuppressed = 0;
			io_handle();
			render_frame();
		/*	if (waitBuffer) */
				output_sound();
			if (throttle) {
				diff = clock() - start;
				usleep(FRAMETIME - (diff % FRAMETIME));
				start = clock();
			}
			spritezero = 0;
			nmiAlreadyDone = 0;
			vblank_period = 0;
		}
		if (cpu[0x2001] & 0x18) {
			if (ppudot == 256) {
				y_scroll();
		  } else if (ppudot >= 257 && ppudot <= 320) {
			  /* (reset OAM) */
			  oamaddr = 0; /* only if rendering active? */
			  if (ppudot == 257)
				  namev = (namev & 0xfbe0) | (namet & 0x41f); /* reset x scroll */
			  else if (ppudot >= 280 && ppudot <= 304)
				  namev = (namev & 0x841f) | (namet & 0x7be0); /* reset Y scroll */
		  }
		}

/* RESET CLOCK COUNTER HERE... */
	} else if (scanline == 240 && ppudot == 0) {
		ppucc = 0;

/* RENDERED LINES */
	} else if (scanline < 240) {
		if (ppudot == 256) {
			render_line();
			if (cpu[0x2001] & 0x18)
				y_scroll();
		} else if (ppudot >= 257 && ppudot <= 320  && (cpu[0x2001] & 0x18)) {
			/* (reset OAM) */
			if (ppudot == 257)
				/* (x position) */
				namev = (namev & 0xfbe0) | (namet & 0x41f); /* only if rendering enabled */
			oamaddr = 0; /* only if rendering active? */
		}
	}
	ntimes--;
	ppu_wait--;
	}
}

void y_scroll() {
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

void init_graphs(int wpx, int wpy, int ww, int wh, int sw, int sh) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	win = SDL_CreateWindow("Nesemu", wpx, wpy, ww, wh, 0);
	renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
	bitmapSurface = SDL_CreateRGBSurface(0, sw, sh, 8, 0, 0, 0, 0);
	SDL_SetPaletteColors(bitmapSurface->format->palette, colors, 0, 64);
	SDL_LockSurface(bitmapSurface);
	for (int i = 0; i < 64; i++) {
		colors[i].r = colarray[i * 3];
		colors[i].g = colarray[i * 3 + 1];
		colors[i].b = colarray[i * 3 + 2];
	}
}

void kill_graphs () {
	SDL_FreeSurface(bitmapSurface);
	SDL_DestroyTexture(bitmapTex);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(win);
	SDL_ClearQueuedAudio(1);
	SDL_CloseAudio();
	SDL_Quit();
	exit(EXIT_SUCCESS);
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
uint8_t	nsprites = 0, *objsrc, spritebuff[8], *tiledest, *tilesrc, *ntilesrc, npal, nnpal, attsrc, nattsrc;
uint8_t pmap[SWIDTH] = {0}, *palette = &vram[0x3f00];
uint16_t pindx = 0;
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
						/*  left column mask  */
		for (int ncol = 0; ncol < 32; ncol++) {
			    tilesrc = bpattern + 16 * vram[0x2000 | (namev&0x3ff) | (mirroring[cart.mirroring][((namev&0xc00)>>10)]<<10)];
				attsrc = vram[0x23c0 | (mirroring[cart.mirroring][((namev&0xc00)>>10)]<<10) | ((namev >>4) & 0x38) | ((namev >> 2) & 0x07)];
				/* X: shift right 1 AND 1 Y: shift right 4 AND 1 * 2*/
				npal = ((attsrc >> (((namev >> 1) & 1) | ((namev >> 5) & 2) ) * 2) & 3);

				if ((namev & 0x1f) == 0x1f) {
					namev &= ~0x1f;
					namev ^= 0x0400; /* switch nametable horizontally */
				} else
					namev++; /* next tile */

			    ntilesrc = bpattern + 16 * vram[0x2000 | (namev&0x3ff) | (mirroring[cart.mirroring][((namev&0xc00)>>10)]<<10)];
				nattsrc = vram[0x23c0 | (mirroring[cart.mirroring][((namev&0xc00)>>10)]<<10) | ((namev >>4) & 0x38) | ((namev >> 2) & 0x07)];
				nnpal = ((nattsrc >> (((namev >> 1) & 1) | ((namev >> 5) & 2) ) * 2) & 3);

				for (int pcol = 0; pcol < 8; pcol++) {
	/* TODO: AND palette with 0x30 in greyscale mode */
					if (ncol || (cpu[0x2001]&2)) {
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
					} else {
						tiledest[pcol + 8*ncol] = palette[0];
						pmap[pcol + 8*ncol] = 0;
					}
				}


		}

	} else {
	/*	if (namev >=0x3f00)
			pindx = namev; */
	/*memcpy(&tiledest[0], &blank_line[0], SWIDTH); */
		for (int i=0;i<SWIDTH;i++) {
			tiledest[i] = palette[pindx];
		}
	}


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
				if ( (objsrc[3]+pcol) > (7 * (1-((cpu[0x2001]>>2)&1))) ) { /* check if outside left column mask */
					uint8_t p = ((((tilesrc[ (yoff&7) + flipy * (7 - 2 * (yoff&7))]
						>>  (7 - pcol - flipx * (7 - 2 * pcol))) & 1) ? 1 : 0)
				   + (((tilesrc[((yoff&7) + flipy * (7 - 2 * (yoff&7))) + 8]
								>> (7 - pcol - flipx * (7 - 2 * pcol))) & 1) ? 2 : 0));	/* is this right edge check correct? */
					if (p && (pmap[pcol + objsrc[3]]) && !(nsprites-i-1) && !spritezero && ((objsrc[3]+pcol) < 255)) /* sprite zero hit */
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
				power_reset(1);
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

/*
static inline void draw_nt() {
	 render background

		for (int ntable = 0; ntable < 4; ntable++) {

			for (int nrow = 0; nrow < 30; nrow++) {


				for (int ncol = 0; ncol < 32; ncol++) {

					tilesrc = bpattern + 16 * vram[0x2000 + ncol + (nrow * 32) + ((ntable&(mirrmode+2-(2*mirrmode))) * 0x400)];
					tiledest = bitmapSurface->pixels + (ncol*8 + ((ntable&1) *SWIDTH) + (nrow*16*SWIDTH) + ((ntable>>1) * 480*SWIDTH));
					attsrc = vram[(0x23c0+0x400*ntable) + (nrow >> 2) * 8 + (ncol >> 2)];
					npal = (attsrc >> (((ncol % 4) - (ncol % 2))
									+ (((nrow % 4) - (nrow % 2)) * 2))) & 3;
					for (int rr = 0; rr < 8; rr++) {
		 TODO: AND palette with 0x30 in greyscale mode
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
*/
