/*
 * mapper.c
 *
 *  Created on: Jul 25, 2018
 *      Author: jonas
 */
#include "mapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "globals.h"

/* mmc1 globals */
uint8_t mmc1Shift = 0, mmc1Buffer, mmc1Wram, mmc1PrgSelect, mmc1PrgSize, mmc1ChrSize, mmc1PrgLarge = 0;
uint32_t mmc1ChrOffset, mmc1RamOffset, mmc1PrgOffset;

void mapper_mmc1(uint16_t address, uint8_t value) {
	if (value & 0x80) {
		mmc1Shift = 0;
		mmc1Buffer = 0;
		mmc1PrgSelect = 1;
		mmc1PrgSize = 1;
	} else {
		mmc1Buffer = (mmc1Buffer & ~(1<<mmc1Shift)) | ((value & 1)<<mmc1Shift);
		if (mmc1Shift == 4) {
			switch ((address>>13) & 3) {
			case 0: /* Control register */
				switch (mmc1Buffer&3) {
				case 0:
					cart.mirroring = 2;
					break;
				case 1:
					cart.mirroring = 3;
					break;
				case 2:
					cart.mirroring = 1;
					break;
				case 3:
					cart.mirroring = 0;
					break;
				}
				mmc1PrgSelect = ((mmc1Buffer >> 2) & 1); /* 0 low, 1 high */
				mmc1PrgSize = ((mmc1Buffer >> 3) & 1); /* 0 32k, 1 16k */
				mmc1ChrSize = ((mmc1Buffer >> 4) & 1); /* 0 8k,  1 4k */
				break;
			case 1: /* CHR ROM low bank */
				/* TODO: disambiguate pcb */
				mmc1ChrOffset = ((mmc1Buffer>>(!mmc1ChrSize))&((cart.chrSize>>2)-1)) * (0x1000<<(!mmc1ChrSize));
				memcpy(&vram[0], &chr[mmc1ChrOffset], (0x1000<<(!mmc1ChrSize)));
				if (cart.prgSize == 0x80000) {
					mmc1PrgLarge = ((mmc1Buffer>>4)&1);
				}
				if ((cart.wramSize + cart.bwramSize) >= 0x4000) {
					mmc1RamOffset = ((mmc1Buffer>>2)&3) * 0x2000;
					memcpy(&cpu[0x6000], &prg[mmc1RamOffset], 0x2000);
				}
				break;
			case 2: /* CHR ROM high bank (4k mode) */
				/* TODO: disambiguate pcb */
				if (mmc1ChrSize) {
					mmc1ChrOffset = (mmc1Buffer&((cart.chrSize>>2)-1)) * 0x1000;
					memcpy(&vram[0x1000], &chr[mmc1ChrOffset], 0x1000);
					if (cart.prgSize == 0x80000) {
						mmc1PrgLarge = ((mmc1Buffer>>4)&1);
					}
					if ((cart.wramSize + cart.bwramSize) >= 0x4000) {
						mmc1RamOffset = ((mmc1Buffer>>2)&3) * 0x2000;
						memcpy(&cpu[0x6000], &prg[mmc1RamOffset], 0x2000);
					}
				}
				break;
			case 3: /* PRG ROM bank */
				mmc1Wram = ((mmc1Buffer >> 4) & 1);
				/* TODO: 32kb banks */
				if (mmc1PrgSize) {
					mmc1PrgOffset = ((mmc1Buffer & 0xf) * 0x4000 + (mmc1PrgLarge ? 0x40000 : 0));
					memcpy(&cpu[!mmc1PrgSelect ? 0xc000 : 0x8000], &prg[mmc1PrgOffset], 0x4000);
				}
				else if (!mmc1PrgSize) {
					mmc1PrgOffset = ((mmc1Buffer & 0xe) * 0x8000 + (mmc1PrgLarge ? 0x40000 : 0));
					memcpy(&cpu[0x8000], &prg[mmc1PrgOffset], 0x8000);
				}
				break;
		}
			mmc1Shift = 0;
			mmc1Buffer = 0;
		} else
			mmc1Shift++;
	}
}
