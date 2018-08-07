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
#include "nestools.h"

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
 * -Super Mario Bros; line glitch below status screen
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
 * -Battletoads: crashes at level 2 - timing issue
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
 * */

void mapper_cnrom (uint8_t value) {
	chr_8((value & ((csize >> 13) - 1)) * 0x2000);

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
 * -Ducktales: should status screen flicker when scrooge overlaps?
 * -Tatakai no Banka: one frame graphics glitches - worse now, sprite zero?
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

/* TODO:
 *
 *-Correct IRQ behavior (requires better ppu emulation)
 *-implement TQROM (mapper 119)
 * - implement TKSROM and TLSROM (mapper 118)
 *
 *Game specific:
 *-batman does not boot (stack pointer goes crazy)
 *-batman returns does not boot...
 *-many game have irq related issues
 */

uint8_t mmc3BankSelect, mmc3Reg[0x08], mmc3IrqEnable,
		mmc3PramProtect, mmc3IrqLatch, mmc3IrqReload, mmc3IrqCounter, mmc3Int = 0;
static inline void mmc3_prg_bank_switch(), mmc3_chr_bank_switch();

void mapper_mmc3 (uint16_t address, uint8_t value) {
	switch ((address>>13) & 3) {
			case 0:
				if (!(address %2)) { /* Bank select (0x8000) */
					mmc3BankSelect = value;
				} else if (address %2) { /* Bank data (0x8001) */
					mmc3Reg[(mmc3BankSelect & 0x07)] = value;
				}
				mmc3_prg_bank_switch();
				mmc3_chr_bank_switch();
				break;
			case 1:
				if (!(address %2)) { /* Mirroring (0xA000) */
					cart.mirroring = 1-(value & 0x01);
				} else if (address%2) { /* PRG RAM protect (0xA001) */
					mmc3PramProtect = value;
				}
				break;
			case 2:
				if (!(address%2)) { /* IRQ latch (0xC000) */
					mmc3IrqLatch = value;
				} else if (address%2) { /* IRQ reload (0xC001) */
					mmc3IrqCounter = mmc3IrqLatch;
					}
				break;
			case 3:
				if (!(address%2)) { /* IRQ disable and acknowledge (0xE000) */
					mmc3IrqEnable = 0;
					mmc3Int = 0;
				} else if (address%2) { /* IRQ enable (0xE001) */
					mmc3IrqEnable = 1;
				}
				break;
		}
}

void mmc3_prg_bank_switch() {
	if (mmc3BankSelect & 0x40) {
		prg_8_0(psize - 0x4000);
		prg_8_1((mmc3Reg[7] & 0x3f) * 0x2000);
		prg_8_2((mmc3Reg[6] & 0x3f) * 0x2000);
		prg_8_3(psize - 0x2000);
	} else if (!(mmc3BankSelect & 0x40)) {
		prg_8_0((mmc3Reg[6] & 0x3f) * 0x2000);
		prg_8_1((mmc3Reg[7] & 0x3f) * 0x2000);
		prg_8_2(psize - 0x4000);
		prg_8_3(psize - 0x2000);
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
	memset(mmc3Reg, 0, 8);
	mmc3Reg[6] = (cart.prgSize / 0x2000) - 2;
	mmc3Reg[7] = (cart.prgSize / 0x2000) - 1;
	mmc3BankSelect = 0x00;
	mmc3_prg_bank_switch();
	mmc3_chr_bank_switch();
	wramEnable = 1;
	mmc3IrqEnable = 0;
	mmc3IrqCounter = 0;
	mmc3IrqLatch = 0;
	mmc3IrqReload = 0;
}

/*/////////////////////////////////*/
/*            Konami VRC           */
/*/////////////////////////////////*/

/*/////////////////////////////////*/
/*            VRC 2 / 4            */
/*/////////////////////////////////*/

/* TODO:
 *
 *-IRQ timing issues? (more general timing issues)
 *-Microwire interface in VRC2
 *-readback of written value in some games...
 *
 *Game specific:
 *-Boku Dracula-kun, crashes with garbage at specific point - IRQ timing?
 *-TMNT 2: does not boot
 *-TMNT: severe graphics issues - IRQ timing?
 */

uint8_t vrc24SwapMode, vrcIrqControl = 0, vrcIrqInt, vrcIrqLatch, vrcIrqCounter;
uint16_t vrcChr0 = 0, vrcChr1 = 0, vrcChr2 = 0, vrcChr3 = 0,
		 vrcChr4 = 0, vrcChr5 = 0, vrcChr6 = 0, vrcChr7 = 0,
		 vrcIrqPrescale;

static inline void vrc24_chr_bank_switch();

void mapper_vrc24(uint16_t address, uint8_t value) {
	/* reroute addressing */
	if (cart.vrcPrg1 > 1)
		address = (address & 0xff00) | ((address>>(cart.vrcPrg1-1)) & 0x02) | ((address>>cart.vrcPrg0) & 0x01);
	else
		address = (address & 0xff00) | ((address<<(1-cart.vrcPrg1)) & 0x02) | ((address>>cart.vrcPrg0) & 0x01);
	/* handle register writes */
	if ((address&0xf003) >= 0x8000 && (address&0xf003) <= 0x8003) { /* PRG select 0 */
		if (vrc24SwapMode) {
			prg_8_0(cart.prgSize - 0x4000);
			prg_8_2((value & ((cart.prgSize>>13)-1)) * 0x2000);
		}
		else {
			prg_8_0((value & ((cart.prgSize>>13)-1)) * 0x2000);
			prg_8_2(cart.prgSize - 0x4000);
		}
		/* always 0x8000 in vrc 2, otherwise depending on swap mode */
	} else if ((address&0xf003) >= 0xa000  && (address&0xf003) <= 0xa003) { /* PRG select 1 */
		prg_8_1((value & ((cart.prgSize>>13)-1)) * 0x2000);
	} else if ((address&0xf003) >= 0x9000  && (address&0xf003) <= 0x9003) { /* mirroring control */
		if (!strcmp(cart.slot,"vrc4") && (address&0xf003) >= 0x9002) {
			vrc24SwapMode = ((value >> 1) & 0x01);
		} else if (!strcmp(cart.slot,"vrc4") && (address&0xf003) < 0x9002) {
			switch (value & 0x03) {
			case 0:
				cart.mirroring = 1;
				break;
			case 1:
				cart.mirroring = 0;
				break;
			case 2:
				cart.mirroring = 2;
				break;
			case 3:
				cart.mirroring = 3;
				break;
			}
		} else if (!strcmp(cart.slot,"vrc2")) {
			cart.mirroring = (value&1) ? 0 : 1;
		}
	} else if ((address&0xf003) == 0xb000) { /* CHR select 0 low */
		vrcChr0 = (vrcChr0&0x1f0) | (value&0xf);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xb001) { /* CHR select 0 high */
		vrcChr0 = (vrcChr0&0xf) | ((value&0x1f)<<4);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xb002) { /* CHR select 1 low */
		vrcChr1 = (vrcChr1&0x1f0) | (value&0xf);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xb003) { /* CHR select 1 high */
		vrcChr1 = (vrcChr1&0xf) | ((value&0x1f)<<4);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xc000) { /* CHR select 2 low */
		vrcChr2 = (vrcChr2&0x1f0) | (value&0xf);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xc001) { /* CHR select 2 high */
		vrcChr2 = (vrcChr2&0xf) | ((value&0x1f)<<4);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xc002) { /* CHR select 3 low */
		vrcChr3 = (vrcChr3&0x1f0) | (value&0xf);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xc003) { /* CHR select 3 high */
		vrcChr3 = (vrcChr3&0xf) | ((value&0x1f)<<4);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xd000) { /* CHR select 4 low */
		vrcChr4 = (vrcChr4&0x1f0) | (value&0xf);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xd001) { /* CHR select 4 high */
		vrcChr4 = (vrcChr4&0xf) | ((value&0x1f)<<4);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xd002) { /* CHR select 5 low */
		vrcChr5 = (vrcChr5&0x1f0) | (value&0xf);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xd003) { /* CHR select 5 high */
		vrcChr5 = (vrcChr5&0xf) | ((value&0x1f)<<4);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xe000) { /* CHR select 6 low */
		vrcChr6 = (vrcChr6&0x1f0) | (value&0xf);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xe001) { /* CHR select 6 high */
		vrcChr6 = (vrcChr6&0xf) | ((value&0x1f)<<4);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xe002) { /* CHR select 7 low */
		vrcChr7 = (vrcChr7&0x1f0) | (value&0xf);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xe003) { /* CHR select 7 high */
		vrcChr7 = (vrcChr7&0xf) | ((value&0x1f)<<4);
		vrc24_chr_bank_switch();
	} else if ((address&0xf003) == 0xf000) { /* IRQ Latch low */
		vrcIrqLatch = (vrcIrqLatch & 0xf0) | (value & 0x0f);
	} else if ((address&0xf003) == 0xf001) { /* IRQ Latch high */
		vrcIrqLatch = (vrcIrqLatch & 0x0f) | ((value & 0xf) << 4);
	} else if ((address&0xf003) == 0xf002) { /* IRQ Control */
		vrcChr7 = (vrcChr7&0xf) | ((value&0x1f)<<4);
		vrcIrqControl = (value & 0x07);
		if (vrcIrqControl & 0x02) {
			vrcIrqCounter = vrcIrqLatch;
			vrcIrqPrescale = 341; /* in reality, it counts CPU cycles */
		}
		vrcIrqInt = 0;
	} else if ((address&0xf003) == 0xf003) { /* IRQ Acknowledge */
		vrcIrqInt = 0;
		vrcIrqControl = (vrcIrqControl & 0x04) | ((vrcIrqControl & 0x01) << 1);
	}
}

void vrc24_chr_bank_switch() {
	if (!strcmp(cart.slot,"vrc2")) {
		if (cart.vrcChr) {
			chr_1_0((vrcChr0 & 0xff) * 0x400);
			chr_1_1((vrcChr1 & 0xff) * 0x400);
			chr_1_2((vrcChr2 & 0xff) * 0x400);
			chr_1_3((vrcChr3 & 0xff) * 0x400);
			chr_1_4((vrcChr4 & 0xff) * 0x400);
			chr_1_5((vrcChr5 & 0xff) * 0x400);
			chr_1_6((vrcChr6 & 0xff) * 0x400);
			chr_1_7((vrcChr7 & 0xff) * 0x400);
		} else if (!cart.vrcChr) {
			chr_1_0((vrcChr0 >> 1) * 0x400);
			chr_1_1((vrcChr1 >> 1) * 0x400);
			chr_1_2((vrcChr2 >> 1) * 0x400);
			chr_1_3((vrcChr3 >> 1) * 0x400);
			chr_1_4((vrcChr4 >> 1) * 0x400);
			chr_1_5((vrcChr5 >> 1) * 0x400);
			chr_1_6((vrcChr6 >> 1) * 0x400);
			chr_1_7((vrcChr7 >> 1) * 0x400);
		}
	} else if (!strcmp(cart.slot,"vrc4")){
		chr_1_0(vrcChr0 * 0x400);
		chr_1_1(vrcChr1 * 0x400);
		chr_1_2(vrcChr2 * 0x400);
		chr_1_3(vrcChr3 * 0x400);
		chr_1_4(vrcChr4 * 0x400);
		chr_1_5(vrcChr5 * 0x400);
		chr_1_6(vrcChr6 * 0x400);
		chr_1_7(vrcChr7 * 0x400);
	}
}

void reset_vrc24() {
	prg_16low(0);
	prg_16high(cart.prgSize-0x4000);
	chr_8(0);
	wramEnable = 1;
	vrc24SwapMode = 0;
}

void reset_default() {
	prg_32(0);
	chr_8(0);
	cart.mirroring = 1;
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
