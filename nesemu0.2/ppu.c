#include "ppu.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h> 	/* clock */
#include <unistd.h> /* usleep */
#include "globals.h"
#include "apu.h"
#include "mapper.h"
#include "my_sdl.h"
#include "6502.h"
#include "cartridge.h"

static inline void draw_nametable(), draw_pattern(), draw_palette(), vertical_increase(), horizontal_increase(), check_nmi(), sprite_evaluation(), sprite_fetch(uint8_t),
				   horizontal_t_to_v(), vertical_t_to_v(), ppu_render(), reload_tile_shifter(), read_tile_data(uint8_t);

uint8_t *chrSlot[0x8], nameTableA[0x400], nameTableB[0x400], palette[0x20], oam[0x100], frameBuffer[SHEIGHT][SWIDTH], nameBuffer[SHEIGHT][SWIDTH<<1], patternBuffer[SWIDTH>>1][SWIDTH], paletteBuffer[SWIDTH>>4][SWIDTH>>1];
uint8_t ppuOamAddress, vblank_period = 0, ppuStatusNmi = 0, ppuStatusSpriteZero = 0, ppuStatusOverflow = 0, nmiSuppressed = 0;
uint8_t throttle = 1, frameBuffer[SHEIGHT][SWIDTH], nameBuffer[SHEIGHT][SWIDTH<<1], patternBuffer[SWIDTH>>1][SWIDTH], paletteBuffer[SWIDTH>>4][SWIDTH>>1];
int16_t ppudot = 0, scanline = 0;

uint8_t primOam[0x100], secOam[0x20];

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
	if (mapperInt)
		irqPulled = 1;

	while (ntimes) {
		ppudot++;
		ppucc++;

		check_nmi();


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
		ppu_render();
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
		if (ppudot == 256) {
			vertical_increase();
		  } else if (ppudot >= 257 && ppudot <= 320) {
			  /* (reset OAM) */
			  ppuOamAddress = 0; /* only if rendering active? */
			  if (ppudot == 257)
				  horizontal_t_to_v();
			  else if (ppudot >= 280 && ppudot <= 304)
				  vertical_t_to_v();
		  } else if (ppudot == 339) {
				if (frame%2 && (ppuMask & 0x18))
					ppudot++;

		}

/* RESET CLOCK COUNTER HERE... */
	} else if (scanline == 240 && ppudot == 0) {
		ppucc = 0;

/* RENDERED LINES */
	} else if (scanline < 240) {
		sprite_evaluation();
		ppu_render();
		if (ppudot == 256) {
			vertical_increase();
		} else if (ppudot >= 257 && ppudot <= 320  && (ppuMask & 0x18)) {
			/* (reset OAM) */
			if (ppudot == 257)
				/* (x position) */
				horizontal_t_to_v(); /* only if rendering enabled */

			ppuOamAddress = 0; /* only if rendering active? */
		}
	}
	ntimes--;
	ppu_wait--;
	}
}

uint8_t oamOverflow, nSprite1, nSprite2, nData, data, nData2;
uint8_t spriteBuffer[256], zeroBuffer[256], priorityBuffer[256], isSpriteZero = 0xff;

void sprite_evaluation() {
if (ppuMask & 0x18) /* rendering is enabled */
{
	if (!ppudot)
	{
		isSpriteZero = 0xff;
		oamOverflow = 0;
		nSprite1 = 0;
		nSprite2 = 0;
		nData = 0;
		nData2 = 0;
	}
	else if (ppudot >= 1 && ppudot <= 256) /* sprite evaluation */
	{
		if (ppudot%2) /* read cycle */
		{
			if (ppudot < 65)
				data = 0xff; /* dummy reads */
			else
			{
				if (nSprite1 == 64) {
					oamOverflow = 1;
					nSprite1 = 0;
				}
				data = oam[(nSprite1 << 2) + nData]; /* read y coordinate */

				if (!nData && !(data <= scanline && scanline <= (data + 7 + ( (ppuController >> 2) & 0x08)))) /* within range? */
				{
					nSprite1++;
				}
				else if (nData == 3)
				{
					nData = 0;
					nSprite1++;
				}
				else
				{
					nData++;
				}
			}
		}
		else if (!(ppudot%2)) /* write cycle */
		{
			if (!oamOverflow)
			{
				if (ppudot < 65)
					secOam[(ppudot>>1) -1] = data;
				else
					secOam[(nSprite2 << 2) + nData2] = data;

				if (nData2 == 3) {
					nData2 = 0;
					nSprite2++;
					if (nSprite2 == 8)
					{
						nSprite2 = 0;
						oamOverflow = 1;
					}
				}
				else
				{
				if (nData2 == 1 && nSprite1 == 0)
				{
					isSpriteZero = nSprite2;
				}
					nData2 = nData;
				}
			}
		}
	}
	/* HBLANK begins */
	else if (ppudot >= 257 && ppudot <= 320) /* sprite fetches */
	{
	/* cycles 1-4: read secondary OAM */
	/* cycles 5-8: read from pattern table (this means that PPU A12 flips at dot 261) */
		if (ppudot == 257)
		{
			nSprite2 = 0;
			memset(spriteBuffer,0,256);
			memset(priorityBuffer,0,256);
			memset(zeroBuffer,0,256);
		}
		if ((ppudot%8) == 5)
		{
			sprite_fetch(nSprite2++);
		}
	}
	else if (ppudot >= 321 && ppudot <= 340)
	{ /* basically idle */
	}
}
}

void sprite_fetch(uint8_t x)
{
	uint8_t *sprite;
	sprite = secOam + (x<<2);
	uint8_t yOffset = scanline - (sprite[0]);
	uint8_t flipX = ((sprite[2] >> 6) & 1);
	uint8_t flipY = ((sprite[2] >> 7) & 1);
	uint8_t spriteRow = (yOffset & 7) + flipY * (7 - ((yOffset & 7) << 1));
	uint8_t nPalette = (sprite[2] & 3) + 4;
	uint16_t patternOffset;
if (sprite[0] != 0xff) {
	/* define sprite pattern source */
	if (ppuController & 0x20) /* 8x16 sprites */
		patternOffset = (((sprite[1] & 0x01) << 12) 	/* select pattern table */
		+ ((sprite[1] & 0xfe) << 4) 					/* offset for top tile (16b per tile) */
		+ (((yOffset << 1) ^ (flipY << 4)) & 0x10)); 	/* select bottom tile if either offset 8+ or flipped sprite */

	else if (!(ppuController & 0x20)) /* 8x8 sprites */
		patternOffset = (sprite[1] << 4) + ((ppuController & 0x08) ? 0x1000 : 0);

	/* decode pattern data and store in buffer */
	for (int pcol = 0; pcol < 8; pcol++)
	{
		uint8_t spritePixel = (7 - pcol - flipX * (7 - (pcol << 1)));
		uint8_t pixelData = ((*ppuread(patternOffset + spriteRow)     >> spritePixel) & 0x01) +
						   (((*ppuread(patternOffset + spriteRow + 8) >> spritePixel) & 0x01) << 1);
		if (pixelData && !(spriteBuffer[sprite[3] + pcol]) && !(sprite[3] == 0xff))
		{
			spriteBuffer[sprite[3] + pcol] = *ppuread(0x3f00 + (nPalette << 2) + pixelData);
			if (isSpriteZero == x)
				zeroBuffer[sprite[3] + pcol] = 1;
			priorityBuffer[sprite[3] + pcol] = (sprite[2] & 0x20);
		}
	}
}
}

uint8_t bgData, ntData, attData, tileLow, tileHigh;
uint16_t tileShifterLow, tileShifterHigh, attShifterHigh, attShifterLow, colShifter;

void reload_tile_shifter()
{
	tileShifterLow = ((tileShifterLow & 0xff00) | tileLow);
	tileShifterHigh = ((tileShifterHigh & 0xff00) | tileHigh);
	attShifterLow = ((attShifterLow & 0xff00) | ((attData & 1) ? 0xff : 0x00));
	attShifterHigh = ((attShifterHigh & 0xff00) | ((attData & 2) ? 0xff : 0x00));
}

void read_tile_data(uint8_t x)
{
	switch (x) {
	case 1:
		ntData = *ppuread(0x2000 | (ppuV&0xfff));
		break;
	case 3:
		bgData = *ppuread(0x2fc0 | ((ppuV >> 4) & 0x38) | ((ppuV >> 2) & 0x07));
		attData = ((bgData >> ((((ppuV >> 1) & 1) | ((ppuV >> 5) & 2)) << 1)) & 3);
		break;
	case 5:
		tileLow = *ppuread((ntData << 4) + ((ppuController & 0x10) << 8) + ((ppuV >> 12) & 7));
		break;
	case 7:
		tileHigh = *ppuread((ntData << 4) + ((ppuController & 0x10) << 8) + ((ppuV >> 12) & 7) + 8);
		break;
	}
}

void ppu_render()
{/* save for render
 * color mask
 */
	if (!ppudot) /* idle */
	{
	}
	else if (ppudot >= 1 && ppudot <= 256) /* tile data fetch */
	{
		read_tile_data(ppudot%8);
		if (!(ppudot%8))
			horizontal_increase();
		if (ppudot%8 == 1)
		{
			reload_tile_shifter();
		}
		uint8_t pValue;
		int16_t cDot = ppudot - 1;
		pValue = ((attShifterHigh >> (12 - ppuX)) & 8) | ((attShifterLow >> (13 - ppuX)) & 4) | ((tileShifterHigh >> (14 - ppuX)) & 2) | ((tileShifterLow >> (15 - ppuX)) & 1);
		uint8_t bgColor = (!(ppuMask & 0x18) && (ppuV & 0x3f00) == 0x3f00) ? *ppuread(ppuV & 0x3fff) : *ppuread(0x3f00);
		uint8_t isSprite = (spriteBuffer[cDot] && (ppuMask & 0x10) && ((cDot > 7) || (ppuMask & 0x04)));
		uint8_t isBg = ((pValue & 0x03) && (ppuMask & 0x08) && ((cDot > 7) || (ppuMask & 0x02)));
		if (scanline < 240)
		{
		if (isSprite && isBg)
		{
			if (!ppuStatusSpriteZero && zeroBuffer[cDot] && cDot<255)
				ppuStatusSpriteZero = 1;
			frameBuffer[scanline][cDot] = priorityBuffer[cDot] ?  *ppuread(0x3f00 + pValue) : spriteBuffer[cDot];
		}
		else if (isSprite && !isBg)
			frameBuffer[scanline][cDot] = spriteBuffer[cDot];
		else if (isBg && !isSprite)
			frameBuffer[scanline][cDot] = *ppuread(0x3f00 + pValue);
		else
			frameBuffer[scanline][cDot] = bgColor;
		}
		tileShifterHigh = (tileShifterHigh << 1);
		tileShifterLow = (tileShifterLow << 1);
		attShifterHigh = (attShifterHigh << 1);
		attShifterLow = (attShifterLow << 1);
	}
	else if (ppudot >= 257 && ppudot <= 320 && scanline < 240) /* move sprite fetch here? */
	{

	}
	else if (ppudot >= 321 && ppudot <= 336) /* prefetch tiles */
	{
		read_tile_data(ppudot%8);
		if (!(ppudot%8))
			horizontal_increase();
		if (ppudot%8 == 1)
		{
			reload_tile_shifter();
		}
		tileShifterHigh = (tileShifterHigh << 1);
		tileShifterLow = (tileShifterLow << 1);
		attShifterHigh = (attShifterHigh << 1);
		attShifterLow = (attShifterLow << 1);
	}
	else if (ppudot >= 337 && ppudot <= 340) /* dummy NT fetches */
	{

	}
}

void horizontal_increase()
{
if (ppuMask & 0x18)
	{
	if ((ppuV & 0x1f) == 0x1f)
	{
		ppuV &= ~0x1f;
		ppuV ^= 0x0400; /* switch nametable horizontally */
	}
	else
		ppuV++; /* next tile */
	}
}

void vertical_increase()
{
	if (ppuMask & 0x18)
	{
		if ((ppuV & 0x7000) != 0x7000)
			ppuV += 0x1000;
		else
		{
			ppuV &= ~0x7000;
			uint8_t coarseY = ((ppuV & 0x3e0) >> 5);
			if (coarseY == 29)
			{
				coarseY = 0;
				ppuV ^= 0x0800;
			} else if (coarseY == 31)
				coarseY = 0;
			else
				coarseY++;
			ppuV = (ppuV & ~0x03e0) | (coarseY << 5);
		}
	}
}

void horizontal_t_to_v()
{
	if (ppuMask & 0x18)
	{
	ppuV = (ppuV & 0xfbe0) | (ppuT & 0x41f); /* reset x scroll */
	}
}

void vertical_t_to_v()
{
	if (ppuMask & 0x18)
	{
	ppuV = (ppuV & 0x841f) | (ppuT & 0x7be0); /* reset Y scroll */
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
		tmpval8 = ((ppuStatusNmi || (scanline==261 && ppudot==1))  <<7) | (ppuStatusSpriteZero<<6) | (ppuStatusOverflow<<5) | (ppureg & 0x1f);
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
		printf("scroll %i,%i\n",scanline,ppudot);
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
		else if ((ppuV & 0x3fff) >= 0x2000 || cart.cramSize)
			*ppuread(ppuV & 0x3fff) = ppuData;
		ppuV += (ppuController & 0x04) ? 32 : 1;
		break;
	}
}
uint16_t lastAddress = 0x0000;
uint8_t * ppuread(uint16_t address) {
	uint8_t * memLocation;
	if (address < 0x2000) /* pattern tables */
	{
		if ((address & 0x1000) && ((address ^ lastAddress) & 0x1000) && !strcmp(cart.slot,"txrom"))
		{
			mmc3_irq();
		}
		lastAddress = address;
		memLocation = &chrSlot[(address>>10)][address&0x3ff];
	}
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
