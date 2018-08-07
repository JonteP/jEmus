#include "ppu.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include "globals.h"
#include "nestools.h"
#include "apu.h"
#include "mapper.h"
#include "my_sdl.h"


static inline void draw_line(), draw_nametable(), draw_pattern(), draw_palette(), y_scroll();

uint8_t *chrSlot[0x8], nameTableA[0x400], nameTableB[0x400], palette[0x20], oam[0x100], frameBuffer[SHEIGHT][SWIDTH], nameBuffer[SHEIGHT][SWIDTH<<1], patternBuffer[SWIDTH>>1][SWIDTH], paletteBuffer[SWIDTH>>4][SWIDTH>>1];
uint8_t oamaddr, vblank_period = 0, ppuStatus_nmiOccurred = 0, spritezero = 0;
uint8_t throttle = 1, vblankSuppressed = 0, frameBuffer[SHEIGHT][SWIDTH], nameBuffer[SHEIGHT][SWIDTH<<1], patternBuffer[SWIDTH>>1][SWIDTH], paletteBuffer[SWIDTH>>4][SWIDTH>>1];
int16_t ppudot = -1, scanline = 0;
uint32_t frame = 0, nmiIsTriggered = 0;
int32_t ppucc = -1;
clock_t start, diff;

void init_time () {
	start = clock();
}
uint8_t miniCount = 0;
void run_ppu (uint16_t ntimes) {

	while (ntimes) {
		ppudot++;
		ppucc++;

		if (vrcIrqControl & 0x02) {
			if (vrcIrqCounter == 0xff) {
				vrcIrqInt = 1;
				vrcIrqCounter = vrcIrqLatch;
			}
			else if ((!vrcIrqPrescale && !(vrcIrqControl & 0x04))) {
				vrcIrqCounter++;
				vrcIrqPrescale = 341;
			} else if (vrcIrqControl & 0x04) {
				if (miniCount == 2) {
					miniCount = 0;
					vrcIrqCounter++;
				} else
					miniCount++;
			} else
				vrcIrqPrescale--;
		}
		if ((vrcIrqControl & 0x02) && vrcIrqInt) {
			interrupt_handle(IRQ);
		}

		if (mmc3IrqEnable && mmc3Int)
			interrupt_handle(IRQ);

		if (ppudot == 341) {
		scanline++;
		ppudot = 0;
	}
	if (scanline == 262) {
		scanline = 0;
		frame++;
		if (frame%2 && (ppuMask & 0x18))
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
			if (nametableActive)
				draw_nametable();
			if (patternActive)
				draw_pattern();
			if (paletteActive)
				draw_palette();
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
		if (ppuMask & 0x18) {
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

		if (ppudot == 260 && (ppuMask & 0x18)) {
			if (mmc3IrqCounter == 0) {
				mmc3IrqCounter = mmc3IrqLatch;
				if (mmc3IrqEnable)
					mmc3Int = 1;
			} else
				mmc3IrqCounter--;
		}

		if (ppudot == 256) {
			draw_line();
			if (ppuMask & 0x18)
				y_scroll();
		} else if (ppudot >= 257 && ppudot <= 320  && (ppuMask & 0x18)) {
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

void draw_line() {
uint8_t	nsprites = 0, *objsrc, spritebuff[8], *tilesrc, *ntilesrc, npal, nnpal, attsrc, nattsrc;
uint8_t pmap[SWIDTH] = {0}, smap[SWIDTH] = {0}, *palette = ppuread(0x3f00);
uint16_t pindx = 0, tileOffset, spriteOffset;
/* fill sprite buffer */
	if (ppuMask & 0x10) {
		for (uint8_t i = (oamaddr>>2); i < 64; i++) {
			objsrc = oam + (i<<2);
			if ((objsrc[0]+1) <= scanline && scanline <= (objsrc[0]+8 + (((ppuController>>5)&1)*8))) {
				if (nsprites < 8) {
					spritebuff[nsprites]=(i<<2);
					nsprites++;
				} else
					spriteof = 1; /* TODO: correct sprite overflow */
			}
		}
	}

/* render background */
	if (ppuMask & 0x08) {
						/*  left column mask  */
		for (int ncol = 0; ncol < 32; ncol++) {
				tileOffset = ( (16 * *ppuread(0x2000 | (namev&0xfff))) + ((ppuController & 0x10) <<8));
			    tilesrc = &chrSlot[(tileOffset>>10)][tileOffset&0x3ff];
				attsrc = *ppuread(0x2fc0 | ((namev >>4) & 0x38) | ((namev >> 2) & 0x07));
				/* X: shift right 1 AND 1 Y: shift right 4 AND 1 * 2*/
				npal = ((attsrc >> (((namev >> 1) & 1) | ((namev >> 5) & 2) ) * 2) & 3);

				if ((namev & 0x1f) == 0x1f) {
					namev &= ~0x1f;
					namev ^= 0x0400; /* switch nametable horizontally */
				} else
					namev++; /* next tile */
				tileOffset = ( (16 * *ppuread(0x2000 | (namev&0xfff))) + ((ppuController & 0x10) <<8));
			    ntilesrc = &chrSlot[(tileOffset>>10)][tileOffset&0x3ff];
				nattsrc = *ppuread(0x2fc0 | ((namev >>4) & 0x38) | ((namev >> 2) & 0x07));
				nnpal = ((nattsrc >> (((namev >> 1) & 1) | ((namev >> 5) & 2) ) * 2) & 3);

				for (int pcol = 0; pcol < 8; pcol++) {
	/* TODO: AND palette with 0x30 in greyscale mode */
					if (ncol || (ppuMask&2)) {
						if ((pcol + scrollx) < 8) {
							pmap[pcol + 8*ncol] = ((tilesrc[(namev >> 12)] & (1<<(7-(pcol+scrollx))) ) ? 1 : 0) + ( (tilesrc[(namev >> 12) + 8] & (1<<(7-(pcol+scrollx)))) ? 2 : 0);
							if (pmap[pcol + 8*ncol])
								frameBuffer[scanline][pcol + 8*ncol] = palette[npal * 4 + pmap[pcol + 8*ncol]];
							else
								frameBuffer[scanline][pcol + 8*ncol] = palette[0];
						}
						else {
							pmap[pcol + 8*ncol] = ((ntilesrc[(namev >> 12)] & (1<<(7-((pcol+scrollx)-8))) ) ? 1 : 0) + ( (ntilesrc[(namev >> 12) + 8] & (1<<(7-((pcol+scrollx)-8)))) ? 2 : 0);
							if (pmap[pcol + 8*ncol])
								frameBuffer[scanline][pcol + 8*ncol] = palette[nnpal * 4 + pmap[pcol + 8*ncol]];
							else
								frameBuffer[scanline][pcol + 8*ncol] = palette[0];
						}
					} else {
						frameBuffer[scanline][pcol + 8*ncol] = palette[0];
						pmap[pcol + 8*ncol] = 0;
					}
				}


		}

	} else {
	/*	if (namev >=0x3f00)
			pindx = namev; */
	/*memcpy(&tiledest[0], &blank_line[0], SWIDTH); */
		for (int i=0;i<SWIDTH;i++) {
			frameBuffer[scanline][i] = palette[pindx];
		}
	}

/* render sprites */
	if (ppuMask & 0x10) {
		for (uint8_t i = 0; i < nsprites; i++) {
			objsrc = oam + spritebuff[i];
			uint8_t yoff = scanline - (objsrc[0]+1);
			uint8_t flipx = ((objsrc[2] >> 6) & 1);
			uint8_t flipy = ((objsrc[2] >> 7) & 1);
			if (ppuController & 0x20) /* large sprites */
				spriteOffset = ((objsrc[1] & 0x01) * 0x1000 + ((objsrc[1] & 0xfe) << 4) + (((yoff<<1)^(flipy<<4)) & 0x10));
			else if (!(ppuController & 0x20))
				spriteOffset = (objsrc[1]<<4) + ((ppuController & 0x08) ? 0x1000 : 0);
			tilesrc = &chrSlot[(spriteOffset>>10)][spriteOffset&0x3ff];
			npal = (objsrc[2] & 3) + 4;
			for (int pcol = 0; pcol < 8; pcol++) {
				if ( (objsrc[3]+pcol) > (7 * (1-((ppuMask>>2)&1))) && !smap[pcol + objsrc[3]]) { /* check if outside left column mask */
					uint8_t p = ((((tilesrc[ (yoff&7) + flipy * (7 - 2 * (yoff&7))]
						>>  (7 - pcol - flipx * (7 - 2 * pcol))) & 1) ? 1 : 0)
				   + (((tilesrc[((yoff&7) + flipy * (7 - 2 * (yoff&7))) + 8]
								>> (7 - pcol - flipx * (7 - 2 * pcol))) & 1) ? 2 : 0));	/* is this right edge check correct? */
					smap[pcol + objsrc[3]] = p;
					if (p && (pmap[pcol + objsrc[3]]) && (!spritebuff[i]) && !spritezero && ((objsrc[3]+pcol) < 255)) /* sprite zero hit */
						spritezero = 1;
					if (p && !(objsrc[2] & 0x20))
						frameBuffer[scanline][objsrc[3] + pcol] = palette[npal * 4 + p];
					else if (p && (objsrc[2] & 0x20) && !(pmap[pcol + objsrc[3]]))
						frameBuffer[scanline][objsrc[3] + pcol] = palette[npal * 4 + p];
				}
			}
		}
	}
}


void draw_nametable () {
	uint8_t	*tilesrcA, npalA, attsrcA, paletteIndexA;
	uint16_t tileOffsetA;
	uint8_t	*tilesrcB, npalB, attsrcB, paletteIndexB;
	uint16_t tileOffsetB;
	for (int tileRow = 0; tileRow < 30; tileRow++) {
		for (int tileColumn = 0; tileColumn < 32; tileColumn++) {
			tileOffsetA = 16 * nameTableA[tileRow * 32 + tileColumn] + ((ppuController & 0x10) <<8);
		    tilesrcA = &chrSlot[(tileOffsetA>>10)][tileOffsetA&0x3ff];
		    attsrcA = nameTableA[0x3c0 + (tileRow >> 2) * 8 + (tileColumn >> 2)];
		    npalA = ((attsrcA >> (((tileColumn >> 1) & 1) | (tileRow & 2) ) * 2) & 3);
			tileOffsetB = 16 * nameTableB[tileRow * 32 + tileColumn] + ((ppuController & 0x10) <<8);
		    tilesrcB = &chrSlot[(tileOffsetB>>10)][tileOffsetB&0x3ff];
		    attsrcB = nameTableB[0x3c0 + (tileRow >> 2) * 8 + (tileColumn >> 2)];
		    npalB = ((attsrcB >> (((tileColumn >> 1) & 1) | (tileRow & 2) ) * 2) & 3);
					for (int pixelRow = 0; pixelRow < 8; pixelRow++) {
						for (int pixelColumn = 0; pixelColumn < 8; pixelColumn++) {
							paletteIndexA = (((tilesrcA[pixelRow]     & (1<<(7-pixelColumn))) ? 1 : 0)
										  + ( (tilesrcA[pixelRow + 8] & (1<<(7-pixelColumn))) ? 2 : 0));

							nameBuffer[(tileRow<<3) + pixelRow][         (tileColumn<<3) + pixelColumn] =
									paletteIndexA ? *ppuread(0x3f00 + npalA * 4 + paletteIndexA) : *ppuread(0x3f00);

							paletteIndexB = (((tilesrcB[pixelRow]     & (1<<(7-pixelColumn))) ? 1 : 0)
										  + ( (tilesrcB[pixelRow + 8] & (1<<(7-pixelColumn))) ? 2 : 0));

							nameBuffer[(tileRow<<3) + pixelRow][SWIDTH + (tileColumn<<3) + pixelColumn] =
									paletteIndexB ? *ppuread(0x3f00 + npalB * 4 + paletteIndexB) : *ppuread(0x3f00);
						}
					}
				}
			}
}

void draw_pattern () {
	uint8_t	*tilesrcA, npalA;
	uint16_t tileOffsetA, palSourceA;
	uint8_t	*tilesrcB, npalB;
	uint16_t tileOffsetB, palSourceB;
	for (int tileRow = 0; tileRow < 16; tileRow++) {
		for (int tileColumn = 0; tileColumn < 16; tileColumn++) {
			tileOffsetA = 16 * ((tileRow << 4) + tileColumn);
		    tilesrcA = &chrSlot[(tileOffsetA>>10)][tileOffsetA&0x3ff];
		    npalA = 0;
			tileOffsetB = 16 * ((tileRow << 4) + tileColumn) + 0x1000;
		    tilesrcB = &chrSlot[(tileOffsetB>>10)][tileOffsetB&0x3ff];
		    npalB = 0;
		    palSourceA = (ppuController & 0x10) ? 0x3f10 : 0x3f00;
		    palSourceB = (ppuController & 0x08) ? 0x3f10 : 0x3f00;

			for (int pixelRow = 0; pixelRow < 8; pixelRow++) {
				for (int pixelColumn = 0; pixelColumn < 8; pixelColumn++) {

					patternBuffer[(tileRow<<3) + pixelRow][         (tileColumn<<3) + pixelColumn] =
						*ppuread(palSourceA + npalA * 4
						+ (((tilesrcA[pixelRow]     & (1<<(7-pixelColumn))) ? 1 : 0)
						+ ( (tilesrcA[pixelRow + 8] & (1<<(7-pixelColumn))) ? 2 : 0)));

					patternBuffer[(tileRow<<3) + pixelRow][(SWIDTH>>1) + (tileColumn<<3) + pixelColumn] =
						*ppuread(palSourceB + npalB * 4
						+ (((tilesrcB[pixelRow]     & (1<<(7-pixelColumn))) ? 1 : 0)
						+ ( (tilesrcB[pixelRow + 8] & (1<<(7-pixelColumn))) ? 2 : 0)));
				}
			}
		}
	}
}

void draw_palette () {
	for (int tileRow = 0; tileRow < 2; tileRow++) {
		for (int tileColumn = 0; tileColumn < 16; tileColumn++) {
			for (int pixelRow = 0; pixelRow < 8; pixelRow++) {
				for (int pixelColumn = 0; pixelColumn < 8; pixelColumn++) {
					paletteBuffer[(tileRow<<3) + pixelRow][         (tileColumn<<3) + pixelColumn] =
						*ppuread(0x3f00 + (tileRow << 4) + tileColumn);
				}
			}
		}
	}
}
