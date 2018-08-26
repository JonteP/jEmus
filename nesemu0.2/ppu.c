#include "ppu.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include "globals.h"
#include "apu.h"
#include "mapper.h"
#include "my_sdl.h"
#include "6502.h"

static inline void draw_line(), draw_nametable(), draw_pattern(), draw_palette(), y_scroll(), check_nmi();

uint8_t *chrSlot[0x8], nameTableA[0x400], nameTableB[0x400], palette[0x20], oam[0x100], frameBuffer[SHEIGHT][SWIDTH], nameBuffer[SHEIGHT][SWIDTH<<1], patternBuffer[SWIDTH>>1][SWIDTH], paletteBuffer[SWIDTH>>4][SWIDTH>>1];
uint8_t ppuOamAddress, vblank_period = 0, ppuStatusNmi = 0, ppuStatusSpriteZero = 0, ppuStatusOverflow = 0, nmiSuppressed = 0;
uint8_t throttle = 1, frameBuffer[SHEIGHT][SWIDTH], nameBuffer[SHEIGHT][SWIDTH<<1], patternBuffer[SWIDTH>>1][SWIDTH], paletteBuffer[SWIDTH>>4][SWIDTH>>1];
int16_t ppudot = 0, scanline = 0;

/* PPU internal registers */
uint8_t ppuW = 0, ppuX = 0;
uint16_t ppuT, ppuV;

/* PPU external registers */
uint8_t ppuController, ppuMask, ppuData;

uint32_t frame = 0, nmiFlipFlop = 0;
int32_t ppucc = 0;
clock_t start, diff;

void init_time () {
	start = clock();
}
uint8_t miniCount = 0;
void run_ppu (uint16_t ntimes) {

	while (ntimes) {
		ppudot++;
		ppucc++;

		check_nmi();

		if (mmc3Int)
			irqPulled = 1;

		if (ppudot == 341) {
		scanline++;
		ppudot = 0;
	}
	if (scanline == 262) {
		scanline = 0;
		frame++;
	}

/* VBLANK ONSET */
	if (scanline == 241 && ppudot == 1) {
		ppuStatusNmi = 1; /* set vblank */

		vblank_period = 1;

/* PRERENDER SCANLINE */
	} else if (scanline == 261) {
		if (ppudot == 1) {
			ppuStatusNmi = 0; /* clear vblank */
			nmiSuppressed = 0;

			io_handle();
			if (nametableActive)
				draw_nametable();
			if (patternActive)
				draw_pattern();
			if (paletteActive)
				draw_palette();
			render_frame();
			if (throttle) {
				diff = clock() - start;
				usleep(FRAMETIME - (diff % FRAMETIME));
				start = clock();
			}

			ppuStatusSpriteZero = 0;
			nmiPulled = 0;
			vblank_period = 0;
		}
		if (ppuMask & 0x18) {
			if (ppudot == 256) {
				y_scroll();
		  } else if (ppudot >= 257 && ppudot <= 320) {
			  /* (reset OAM) */
			  ppuOamAddress = 0; /* only if rendering active? */
			  if (ppudot == 257)
				  ppuV = (ppuV & 0xfbe0) | (ppuT & 0x41f); /* reset x scroll */
			  else if (ppudot >= 280 && ppudot <= 304)
				  ppuV = (ppuV & 0x841f) | (ppuT & 0x7be0); /* reset Y scroll */
		  } else if (ppudot == 339) {
				if (frame%2 && (ppuMask & 0x18))
					ppudot++;
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
		if (!ppudot)
			draw_line();
		if (ppudot == 256) {
			if (ppuMask & 0x18)
				y_scroll();
		} else if (ppudot >= 257 && ppudot <= 320  && (ppuMask & 0x18)) {
			/* (reset OAM) */
			if (ppudot == 257)
				/* (x position) */
				ppuV = (ppuV & 0xfbe0) | (ppuT & 0x41f); /* only if rendering enabled */

			ppuOamAddress = 0; /* only if rendering active? */
		}
	}
	ntimes--;
	ppu_wait--;
	}
}

void y_scroll() {
	if ((ppuV & 0x7000) != 0x7000)
		ppuV += 0x1000;
	else {
		ppuV &= ~0x7000;
		uint8_t coarsey = ((ppuV & 0x3e0) >> 5);
		if (coarsey == 29) {
			coarsey = 0;
			ppuV ^= 0x0800;
	  } else if (coarsey == 31)
			coarsey = 0;
	    else
			coarsey++;
			ppuV = (ppuV & ~0x03e0) | (coarsey << 5);
	}
}

void draw_line() {
uint8_t	nsprites = 0, *objsrc, spritebuff[8], *tilesrc, *ntilesrc, npal, nnpal, attsrc, nattsrc;
uint8_t pmap[SWIDTH] = {0}, smap[SWIDTH] = {0}, *palette = ppuread(0x3f00);
uint16_t pindx = 0, tileOffset, spriteOffset;
/* fill sprite buffer */
	if (ppuMask & 0x10) {
		for (uint8_t i = (ppuOamAddress>>2); i < 64; i++) {
			objsrc = oam + (i<<2);
			if ((objsrc[0]+1) <= scanline && scanline <= (objsrc[0]+8 + (((ppuController>>5)&1)*8))) {
				if (nsprites < 8) {
					spritebuff[nsprites]=(i<<2);
					nsprites++;
				} else
					ppuStatusOverflow = 1; /* TODO: correct sprite overflow */
			}
		}
	}

/* render background */
	if (ppuMask & 0x08) {
						/*  left column mask  */
		for (int ncol = 0; ncol < 32; ncol++) {
				tileOffset = ( (16 * *ppuread(0x2000 | (ppuV&0xfff))) + ((ppuController & 0x10) <<8));
			    tilesrc = &chrSlot[(tileOffset>>10)][tileOffset&0x3ff];
				attsrc = *ppuread(0x2fc0 | ((ppuV >>4) & 0x38) | ((ppuV >> 2) & 0x07));
				/* X: shift right 1 AND 1 Y: shift right 4 AND 1 * 2*/
				npal = ((attsrc >> (((ppuV >> 1) & 1) | ((ppuV >> 5) & 2) ) * 2) & 3);

				if ((ppuV & 0x1f) == 0x1f) {
					ppuV &= ~0x1f;
					ppuV ^= 0x0400; /* switch nametable horizontally */
				} else
					ppuV++; /* next tile */
				tileOffset = ( (16 * *ppuread(0x2000 | (ppuV&0xfff))) + ((ppuController & 0x10) <<8));
			    ntilesrc = &chrSlot[(tileOffset>>10)][tileOffset&0x3ff];
				nattsrc = *ppuread(0x2fc0 | ((ppuV >>4) & 0x38) | ((ppuV >> 2) & 0x07));
				nnpal = ((nattsrc >> (((ppuV >> 1) & 1) | ((ppuV >> 5) & 2) ) * 2) & 3);

				for (int pcol = 0; pcol < 8; pcol++) {
					if (ncol || (ppuMask&2)) {
						if ((pcol + ppuX) < 8) {
							pmap[pcol + 8*ncol] = ((tilesrc[(ppuV >> 12)] & (1<<(7-(pcol+ppuX))) ) ? 1 : 0) + ( (tilesrc[(ppuV >> 12) + 8] & (1<<(7-(pcol+ppuX)))) ? 2 : 0);
							if (pmap[pcol + 8*ncol])
								frameBuffer[scanline][pcol + 8*ncol] = palette[npal * 4 + pmap[pcol + 8*ncol]] & ((ppuMask & 0x01) ? 0x30 : 0xff);
							else
								frameBuffer[scanline][pcol + 8*ncol] = palette[0] & ((ppuMask & 0x01) ? 0x30 : 0xff);
						}
						else {
							pmap[pcol + 8*ncol] = ((ntilesrc[(ppuV >> 12)] & (1<<(7-((pcol+ppuX)-8))) ) ? 1 : 0) + ( (ntilesrc[(ppuV >> 12) + 8] & (1<<(7-((pcol+ppuX)-8)))) ? 2 : 0);
							if (pmap[pcol + 8*ncol])
								frameBuffer[scanline][pcol + 8*ncol] = palette[nnpal * 4 + pmap[pcol + 8*ncol]] & ((ppuMask & 0x01) ? 0x30 : 0xff);
							else
								frameBuffer[scanline][pcol + 8*ncol] = palette[0] & ((ppuMask & 0x01) ? 0x30 : 0xff);
						}
					} else {
						frameBuffer[scanline][pcol + 8*ncol] = palette[0] & ((ppuMask & 0x01) ? 0x30 : 0xff);
						pmap[pcol + 8*ncol] = 0;
					}
				}


		}

	} else {
	/*	if (ppuV >=0x3f00)
			pindx = namev; */
	/*memcpy(&tiledest[0], &blank_line[0], SWIDTH); */
		for (int i=0;i<SWIDTH;i++) {
			frameBuffer[scanline][i] = palette[pindx] & ((ppuMask & 0x01) ? 0x30 : 0xff);
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
					if (p && (pmap[pcol + objsrc[3]]) && (spritebuff[i] == ppuOamAddress) && !ppuStatusSpriteZero && ((objsrc[3]+pcol) < 255)) /* sprite zero hit */
						ppuStatusSpriteZero = 1;
					if (p && !(objsrc[2] & 0x20))
						frameBuffer[scanline][objsrc[3] + pcol] = palette[npal * 4 + p] & ((ppuMask & 0x01) ? 0x30 : 0xff);
					else if (p && (objsrc[2] & 0x20) && !(pmap[pcol + objsrc[3]]))
						frameBuffer[scanline][objsrc[3] + pcol] = palette[npal * 4 + p] & ((ppuMask & 0x01) ? 0x30 : 0xff);
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

uint8_t ppureg = 0, vbuff = 0;
uint8_t read_ppu_register(uint16_t addr) {
	uint8_t tmpval8;
	switch (addr & 0x2007) {
	case 0x2002:
		if (ppucc == 343 || ppucc == 344) {/* suppress if read and set at same time */
			nmiSuppressed = 1;
			nmiFlipFlop = 0;
		} else if (ppucc == 342) {
			ppuStatusNmi = 0;
		}
		tmpval8 = (ppuStatusNmi<<7) | (ppuStatusSpriteZero<<6) | (ppuStatusOverflow<<5) | (ppureg & 0x1f);
		ppuStatusNmi = 0;
		ppuW = 0;
		break;
	case 0x2004:
		tmpval8 = oam[ppuOamAddress];
		break;
	case 0x2007:
		if (ppuV < 0x3f00) {
			tmpval8 = vbuff;
			vbuff = *ppuread(ppuV);
		}
		/* TODO: buffer update when reading palette */
		else if ((ppuV) >= 0x3f00) {
			tmpval8 = *ppuread(ppuV) & ((ppuMask & 0x01) ? 0x30 : 0xff);
		}
		ppuV += (ppuController & 0x04) ? 32 : 1;
		break;
	}
	return tmpval8;
}

void write_ppu_register(uint16_t addr, uint8_t tmpval8) {
	ppureg = tmpval8;
	switch (addr & 0x2007) {
	case 0x2000:
		ppuController = tmpval8;
		ppuT &= 0xf3ff;
		ppuT |= ((ppuController & 3)<<10);
		if (!(ppuController & 0x80)) {
			nmiPulled = 0;
		} else if (ppuController & 0x80) {
			check_nmi();
		}
		break;
	case 0x2001:
		ppuMask = tmpval8;
		break;
	case 0x2002:
		break;
	case 0x2003:
		ppuOamAddress = tmpval8;
		break;
	case 0x2004:
		ppureg = tmpval8;
		/* TODO: writing during rendering */
		if (vblank_period || !(ppuController & 0x18)) {
			oam[ppuOamAddress++] = tmpval8;
		}
		break;
	case 0x2005:
		if (ppuW == 0) {
			ppuT &= 0xffe0;
			ppuT |= ((tmpval8 & 0xf8)>>3); /* coarse X scroll */
			ppuX = (tmpval8 & 0x07); /* fine X scroll  */
			ppuW = 1;
		} else if (ppuW == 1) {
			ppuT &= 0x8c1f;
			ppuT |= ((tmpval8 & 0xf8)<<2); /* coarse Y scroll */
			ppuT |= ((tmpval8 & 0x07)<<12); /* fine Y scroll */
			ppuW = 0;
		}
		break;
	case 0x2006:
		if (ppuW == 0) {
			ppuT &= 0x80ff;
			ppuT |= ((tmpval8 & 0x3f) << 8);
			ppuW = 1;
		} else if (ppuW == 1) {
			ppuT &= 0xff00;
			ppuT |= tmpval8;
			ppuV = ppuT;
			ppuW = 0;
		}
		break;
	case 0x2007:
		ppuData = tmpval8;
		if ((ppuV & 0x3fff) >= 0x3f00)
			*ppuread(ppuV & 0x3fff) = (ppuData & 0x3f);
		else
			*ppuread(ppuV & 0x3fff) = ppuData;
		ppuV += (ppuController & 0x04) ? 32 : 1;
		break;
	}
}

uint8_t * ppuread(uint16_t address) {
	uint8_t * memLocation;
	if (address < 0x2000) /* pattern tables */
		memLocation = &chrSlot[(address>>10)][address&0x3ff];
	else if (address >= 0x2000 && address <0x3f00) /* nametables */
		memLocation = mirroring[cart.mirroring][((ppuV&0xc00)>>10)] ? &nameTableB[address&0x3ff] : &nameTableA[address&0x3ff];
	else if (address >= 0x3f00) { /* palette RAM */
		if (address == 0x3f10)
			address = 0x3f00;
		else if (address == 0x3f14)
			address = 0x3f04;
		else if (address == 0x3f18)
			address = 0x3f08;
		else if (address == 0x3f1c)
			address = 0x3f0c;
		memLocation = &palette[(address&0x1f)];
	}
	return memLocation;
}

void check_nmi() {
	if ((ppuController & 0x80) && ppuStatusNmi && !nmiPulled && !nmiSuppressed) {
		nmiPulled = 1;
		nmiFlipFlop = ppucc;
	}
}
