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
#include "6502.h"
#include "ppu.h"

static inline void prg_8_0(uint32_t offset), prg_8_1(uint32_t offset), prg_8_2(uint32_t offset), prg_8_3(uint32_t offset),
				   prg_16low(uint32_t offset), prg_16high(uint32_t offset), prg_32(uint32_t offset),
				   chr_8(uint32_t offset), chr_4low(uint32_t offset), chr_4high(uint32_t offset),
				   chr_2_0(uint32_t offset), chr_2_1(uint32_t offset), chr_2_2(uint32_t offset), chr_2_3(uint32_t offset),
				   chr_1_0(uint32_t offset), chr_1_1(uint32_t offset), chr_1_2(uint32_t offset), chr_1_3(uint32_t offset),
				   chr_1_4(uint32_t offset), chr_1_5(uint32_t offset), chr_1_6(uint32_t offset), chr_1_7(uint32_t offset);

/*/////////////////////////////////*/
/*               NROM              */
/*/////////////////////////////////*/

/* TODO:
 *
 * Game specific:
 * -Burger Time: random crash (stack pointer)
 * -Super Mario Bros; Tower of Druaga: sprite zero issues
 * -10-Yard Fight: glitch line on top of playfield
 * -1942: missing letters....
 */

void reset_nrom() {
	if (cart.prgSize == 0x4000) {
		prg_16low(0);
		prg_16high(0);
} else {
	prg_32(0);
}
	chr_8(0);
}

/*/////////////////////////////////*/
/*               AxROM             */
/*/////////////////////////////////*/

/* TODO:
 *
 * -bus conflicts?
 *
 * Game specific:
 * -Battletoads: severe graphics issues
 * -Cabal: does not boot
 * */

void mapper_axrom(uint8_t value) {
	prg_32((value & 0x07) * 0x8000);
	(value & 0x10) ? (cart.mirroring = 3) : (cart.mirroring = 2);
}

void reset_axrom() {
	prg_32(0);
	chr_8(0);
}

/*/////////////////////////////////*/
/*               CNROM             */
/*/////////////////////////////////*/

/* TODO:
 *
 * -bus conflicts
 *
 * Game specific:
 * -Banana: does not boot
 * -Gradius: BG graphics glitches
 * */

void mapper_cnrom (uint8_t value) {
	chr_8((value & 0x03) * 0x2000);
}

void reset_cnrom () {
	prg_32(0);
	chr_8(0);
}

/*/////////////////////////////////*/
/*               UxROM             */
/*/////////////////////////////////*/

/* TODO:
 *
 * -implement bus conflict (test roms available)
 * -should mappers 71 and 232 go here?
 *
 * Game specific:
 * -Senjou no Ookami: garbage graphics flicker
 * -Akumajou; Ducktales: the usual sprite zero issues
 * -Tatakai no Banka: one frame graphics glitches (see Senjou..)
 */

void mapper_uxrom (uint8_t value) {
	uint8_t bank;
	if (!strcmp(cart.slot,"un1rom"))
		bank = ((value >> 2) & 0x07);
	else
		bank = (value & 0x0f); /* differ between UNROM / UOROM? */
	if (!strcmp(cart.slot,"unrom_cc")) { /* switch 0xc000 */
		prg_16low(0);
		prg_16high(bank * 0x4000);
	} else {							 /* switch 0x8000 */
		prg_16low(bank * 0x4000);
		prg_16high(cart.prgSize - 0x4000);
	}
}

void reset_uxrom () {
	prg_16low(0);
	prg_16high(cart.prgSize - 0x4000);
	chr_8(0);
}


/*/////////////////////////////////*/
/*              MMC 1              */
/*              SxROM              */
/*/////////////////////////////////*/

/* TODO:
 *
 * -PRG RAM banking
 * -PRG ROM banking, fixed low, is last bank high handled correctly?
 *
 * Game specific:
 * -Dragon Warrior III: resets/hangs
 * -Bill and Ted: does not boot
 * -Bomber Man 2: missing graphics on start screen
 * -Ninja Gaiden: wrong graphics during cutscenes
 * -Zelda 2: wrong graphics during title screen
 * -Dragon Warrior: glitchy graphics / speed issues
 * -Ys and TMNT: flashing graphics and slowdown during gameplay
 * -Chip and Dale 2: graphics issues during title and intro
 * -Bubble Bobble: wrong BG color
 */

/* mmc1 globals */
uint8_t mmc1Shift = 0, mmc1Buffer,
		mmc1Reg0, mmc1Reg1, mmc1Reg2, mmc1Reg3;
uint32_t mmc1RamOffset, mmc1PrgOffset = 0;

static inline void mmc1_prg_bank_switch(), mmc1_chr_bank_switch();

void mapper_mmc1(uint16_t address, uint8_t value) {
	if (value & 0x80) {
		mmc1Shift = 0;
		mmc1Buffer = 0;
		mmc1Reg0 |= 0x0c;
	} else {
		mmc1Buffer = (mmc1Buffer & ~(1<<mmc1Shift)) | ((value & 1)<<mmc1Shift);
		if (mmc1Shift == 4) {
			switch ((address>>13) & 3) {
			case 0: /* Control register */
				mmc1Reg0 = mmc1Buffer;
				switch (mmc1Reg0&3) {
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
				mmc1_prg_bank_switch();
				mmc1_chr_bank_switch();
				break;
			case 1: /* CHR ROM low bank */
				mmc1Reg1 = mmc1Buffer;
				if (cart.prgSize == 0x80000) {
					mmc1PrgOffset = ((mmc1Reg1&0x10)<<14);
				}
				mmc1_chr_bank_switch();
				mmc1_prg_bank_switch();
				break;
			case 2: /* CHR ROM high bank (4k mode) */
				mmc1Reg2 = mmc1Buffer;
				mmc1_chr_bank_switch();
				break;
			case 3: /* PRG ROM bank */
				mmc1Reg3 = mmc1Buffer;
				if (strcmp(cart.mmc1_type,"MMC1A"))
					wramEnable = !((mmc1Reg3 >> 4) & 1);
				mmc1_prg_bank_switch();
				break;
		}
			mmc1Shift = 0;
			mmc1Buffer = 0;
		} else
			mmc1Shift++;
	}
}

void mmc1_prg_bank_switch() {
	 uint8_t mmc1PrgSize = (mmc1Reg0 & 0x08); /* 0 32k, 1 16k */
	 uint8_t mmc1PrgSelect = (mmc1Reg0 & 0x04); /* 0 low, 1 high */
	if (mmc1PrgSize) {
		if (mmc1PrgSelect) { /* switch 0x8000, fix 0xc000 to last bank */
			prg_16low((mmc1Reg3 & 0xf) * 0x4000 + mmc1PrgOffset);
			prg_16high(cart.prgSize - 0x4000);
		} else if (!mmc1PrgSelect) { /* switch 0xc000, fix 0x8000 to first bank */
			prg_16low(mmc1PrgOffset);
			prg_16high((mmc1Reg3 & 0xf) * 0x4000 + mmc1PrgOffset);
		}
	}
	else if (!mmc1PrgSize) {
		prg_32(((mmc1Reg3 & 0xf) >> 1) * 0x4000 + mmc1PrgOffset);

	}
}

void mmc1_chr_bank_switch() {
	uint8_t mmc1ChrSize = (mmc1Reg0 & 0x10); /* 0 8k,  1 4k */
	if (mmc1ChrSize) { /* 4k banks */
		chr_4low((mmc1Reg1 & ((csize >> 12) - 1)) * 0x1000);
		chr_4high((mmc1Reg2 & ((csize >> 12) - 1)) * 0x1000);
	} else if (!mmc1ChrSize) { /* 8k bank */
		chr_8(((mmc1Reg1 & ((csize >> 12) - 1)) >> 1) * 0x2000);
	}
}

void reset_mmc1() {
	if ((cart.wramSize+cart.bwramSize) && (!strcmp(cart.mmc1_type,"MMC1A") ||
			!strcmp(cart.mmc1_type,"MMC1B1") ||
				!strcmp(cart.mmc1_type,"MMC1B1-H") ||
					!strcmp(cart.mmc1_type,"MMC1B2") ||
						!strcmp(cart.mmc1_type,"MMC1B3"))) {
		wramEnable = 1;
	}
	mmc1Shift = 0;
	mmc1Buffer = 0;
	mmc1Reg0 = 0x0f;
	mmc1Reg1 = 0;
	mmc1Reg2 = 0;
	mmc1Reg3 = 0;
	cart.mirroring = 0;
	mmc1_prg_bank_switch();
	mmc1_chr_bank_switch();
}

/*/////////////////////////////////*/
/*              MMC 3              */
/*              TxROM              */
/*/////////////////////////////////*/

uint8_t mmc3BankSelect, mmc3Reg[0x08];
static inline void mmc3_prg_bank_switch(), mmc3_chr_bank_switch();

void mapper_mmc3 (uint16_t address, uint8_t value) {
	switch ((address>>13) & 3) {
			case 0:
				if (!(address %2)) { /* Bank select */
					mmc3BankSelect = value;
				} else if (address %2) { /* Bank data */
					mmc3Reg[(mmc3BankSelect & 0x07)] = value;
				}
				mmc3_prg_bank_switch();
				mmc3_chr_bank_switch();
				break;
			case 1:
				if (!(address %2)) { /* Mirroring */
					cart.mirroring = 1-(value & 0x01);
				} else if (address%2) { /* PRG RAM protect */
				}
				break;
			case 2:
				if (!(address%2)) { /* IRQ latch */
					} else if (address%2) { /* IRQ reload */
				}
				break;
			case 3:
				if (!(address%2)) { /* IRQ disable */

				} else if (address%2) { /* IRQ enable */
				}
				break;
		}

}

void mmc3_prg_bank_switch() {
	if (mmc3BankSelect & 0x40) {
		prg_8_0(cart.prgSize - 0x4000);
		prg_8_1((mmc3Reg[7] & 0x3f) * 0x2000);
		prg_8_2((mmc3Reg[6] & 0x3f) * 0x2000);
		prg_8_3(cart.prgSize - 0x2000);
	} else if (!(mmc3BankSelect & 0x40)) {
		prg_8_0((mmc3Reg[6] & 0x3f) * 0x2000);
		prg_8_1((mmc3Reg[7] & 0x3f) * 0x2000);
		prg_8_2(cart.prgSize - 0x4000);
		prg_8_3(cart.prgSize - 0x2000);
	}
}

void mmc3_chr_bank_switch() {
	if (mmc3BankSelect & 0x80) {
		chr_1_0(mmc3Reg[2] * 0x400);
		chr_1_1(mmc3Reg[3] * 0x400);
		chr_1_2(mmc3Reg[4] * 0x400);
		chr_1_3(mmc3Reg[5] * 0x400);
		chr_1_4((mmc3Reg[0] & 0xfe) * 0x400);
		chr_1_5((mmc3Reg[0] | 0x01) * 0x400);
		chr_1_6((mmc3Reg[1] & 0xfe) * 0x400);
		chr_1_7((mmc3Reg[1] | 0x01) * 0x400);
	} else if (!(mmc3BankSelect & 0x80)) {
		chr_1_0((mmc3Reg[0] & 0xfe) * 0x400);
		chr_1_1((mmc3Reg[0] | 0x01) * 0x400);
		chr_1_2((mmc3Reg[1] & 0xfe) * 0x400);
		chr_1_3((mmc3Reg[1] | 0x01) * 0x400);
		chr_1_4(mmc3Reg[2] * 0x400);
		chr_1_5(mmc3Reg[3] * 0x400);
		chr_1_6(mmc3Reg[4] * 0x400);
		chr_1_7(mmc3Reg[5] * 0x400);
	}
}

void reset_mmc3 () {
	mmc3Reg[0] = 0x00;
	mmc3Reg[1] = 0x02;
	mmc3Reg[2] = 0x04;
	mmc3Reg[3] = 0x05;
	mmc3Reg[4] = 0x06;
	mmc3Reg[5] = 0x07;
	mmc3Reg[6] = 0x00;
	mmc3Reg[7] = 0x01;
	mmc3BankSelect = 0x00;
	mmc3_prg_bank_switch();
	mmc3_chr_bank_switch();
}

void prg_16low(uint32_t offset) {
	prgSlot[0] = &prg[offset];
	prgSlot[1] = &prg[offset + 0x1000];
	prgSlot[2] = &prg[offset + 0x2000];
	prgSlot[3] = &prg[offset + 0x3000];
}

void prg_16high(uint32_t offset) {
	prgSlot[4] = &prg[offset];
	prgSlot[5] = &prg[offset + 0x1000];
	prgSlot[6] = &prg[offset + 0x2000];
	prgSlot[7] = &prg[offset + 0x3000];
}

void prg_8_0(uint32_t offset) {
	prgSlot[0] = &prg[offset];
	prgSlot[1] = &prg[offset + 0x1000];
}

void prg_8_1(uint32_t offset) {
	prgSlot[2] = &prg[offset];
	prgSlot[3] = &prg[offset + 0x1000];
}

void prg_8_2(uint32_t offset) {
	prgSlot[4] = &prg[offset];
	prgSlot[5] = &prg[offset + 0x1000];
}

void prg_8_3(uint32_t offset) {
	prgSlot[6] = &prg[offset];
	prgSlot[7] = &prg[offset + 0x1000];
}

void prg_32(uint32_t offset) {
	prgSlot[0] = &prg[offset];
	prgSlot[1] = &prg[offset + 0x1000];
	prgSlot[2] = &prg[offset + 0x2000];
	prgSlot[3] = &prg[offset + 0x3000];
	prgSlot[4] = &prg[offset + 0x4000];
	prgSlot[5] = &prg[offset + 0x5000];
	prgSlot[6] = &prg[offset + 0x6000];
	prgSlot[7] = &prg[offset + 0x7000];
}

void chr_8(uint32_t offset) {
	chrSlot[0] = &chr[offset];
	chrSlot[1] = &chr[offset +  0x400];
	chrSlot[2] = &chr[offset +  0x800];
	chrSlot[3] = &chr[offset +  0xc00];
	chrSlot[4] = &chr[offset + 0x1000];
	chrSlot[5] = &chr[offset + 0x1400];
	chrSlot[6] = &chr[offset + 0x1800];
	chrSlot[7] = &chr[offset + 0x1c00];
}

void chr_4low(uint32_t offset) {
	chrSlot[0] = &chr[offset];
	chrSlot[1] = &chr[offset +  0x400];
	chrSlot[2] = &chr[offset +  0x800];
	chrSlot[3] = &chr[offset +  0xc00];
}

void chr_4high(uint32_t offset) {
	chrSlot[4] = &chr[offset];
	chrSlot[5] = &chr[offset +  0x400];
	chrSlot[6] = &chr[offset +  0x800];
	chrSlot[7] = &chr[offset +  0xc00];
}

void chr_2_0(uint32_t offset) {
	chrSlot[0] = &chr[offset];
	chrSlot[1] = &chr[offset +  0x400];
}

void chr_2_1(uint32_t offset) {
	chrSlot[2] = &chr[offset];
	chrSlot[3] = &chr[offset +  0x400];
}

void chr_2_2(uint32_t offset) {
	chrSlot[4] = &chr[offset];
	chrSlot[5] = &chr[offset +  0x400];
}

void chr_2_3(uint32_t offset) {
	chrSlot[6] = &chr[offset];
	chrSlot[7] = &chr[offset +  0x400];
}

void chr_1_0(uint32_t offset) {
	chrSlot[0] = &chr[offset];
}

void chr_1_1(uint32_t offset) {
	chrSlot[1] = &chr[offset];
}

void chr_1_2(uint32_t offset) {
	chrSlot[2] = &chr[offset];
}

void chr_1_3(uint32_t offset) {
	chrSlot[3] = &chr[offset];
}

void chr_1_4(uint32_t offset) {
	chrSlot[4] = &chr[offset];
}

void chr_1_5(uint32_t offset) {
	chrSlot[5] = &chr[offset];
}

void chr_1_6(uint32_t offset) {
	chrSlot[6] = &chr[offset];
}

void chr_1_7(uint32_t offset) {
	chrSlot[7] = &chr[offset];
}
