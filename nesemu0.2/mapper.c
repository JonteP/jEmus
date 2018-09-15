#include "mapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "globals.h"
#include "6502.h"
#include "ppu.h"
#include "cartridge.h"

uint_fast8_t mapperInt = 0, expSound = 0,
			 prgBank[8], chrBank[8];
static inline void prg_8_0(uint32_t offset), prg_8_1(uint32_t offset), prg_8_2(uint32_t offset), prg_8_3(uint32_t offset),
				   prg_16low(uint32_t offset), prg_16high(uint32_t offset), prg_32(uint32_t offset),
				   chr_8(uint32_t offset), chr_4low(uint32_t offset), chr_4high(uint32_t offset),
				   chr_2_0(uint32_t offset), chr_2_1(uint32_t offset), chr_2_2(uint32_t offset), chr_2_3(uint32_t offset),
				   chr_1_0(uint32_t offset), chr_1_1(uint32_t offset), chr_1_2(uint32_t offset), chr_1_3(uint32_t offset),
				   chr_1_4(uint32_t offset), chr_1_5(uint32_t offset), chr_1_6(uint32_t offset), chr_1_7(uint32_t offset),
				   prg_bank_switch(), chr_bank_switch();

/*/////////////////////////////////*/
/*               NROM              */
/*/////////////////////////////////*/

static inline void reset_nrom();

void reset_nrom()
{
	prgBank[0] = 0;
	prgBank[1] = 1;
	prgBank[2] = 2;
	prgBank[3] = 3;
	prgBank[4] = cart.pSlots - 4;
	prgBank[5] = cart.pSlots - 3;
	prgBank[6] = cart.pSlots - 2;
	prgBank[7] = cart.pSlots - 1;
	prg_bank_switch();
	chrBank[0] = 0;
	chrBank[1] = 1;
	chrBank[2] = 2;
	chrBank[3] = 3;
	chrBank[4] = 4;
	chrBank[5] = 5;
	chrBank[6] = 6;
	chrBank[7] = 7;
	chr_bank_switch();
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
static inline void mapper_axrom(uint_fast8_t);


void mapper_axrom(uint_fast8_t value) {
	prgBank[0] = ((value & 0x07) << 3);
	prgBank[1] = prgBank[0] + 1;
	prgBank[2] = prgBank[0] + 2;
	prgBank[3] = prgBank[0] + 3;
	prgBank[4] = prgBank[0] + 4;
	prgBank[5] = prgBank[0] + 5;
	prgBank[6] = prgBank[0] + 6;
	prgBank[7] = prgBank[0] + 7;
	prg_bank_switch();
	(value & 0x10) ? (cart.mirroring = 3) : (cart.mirroring = 2);
}

/*/////////////////////////////////*/
/*               CNROM             */
/*/////////////////////////////////*/

/* TODO:
 *
 * -bus conflicts
 *
 * */
static inline void mapper_cnrom(uint_fast8_t);

void mapper_cnrom (uint_fast8_t value) {
	chrBank[0] = (value << 3);
	chrBank[1] = chrBank[0] + 1;
	chrBank[2] = chrBank[0] + 2;
	chrBank[3] = chrBank[0] + 3;
	chrBank[4] = chrBank[0] + 4;
	chrBank[5] = chrBank[0] + 5;
	chrBank[6] = chrBank[0] + 6;
	chrBank[7] = chrBank[0] + 7;
	chr_bank_switch();
}

/*/////////////////////////////////*/
/*               UxROM             */
/*/////////////////////////////////*/

/* TODO:
 *
 * -implement bus conflict (test roms available)
 * -should mappers 71 and 232 go here?
 *
 */

static inline void mapper_uxrom(uint_fast8_t);

void mapper_uxrom (uint_fast8_t value) {
	uint_fast8_t bank;
	if (!strcmp(cart.slot,"un1rom"))
		bank = ((value >> 2) & 0x07);
	else
		bank = (value & ((cart.prgSize >> 14)-1)); /* differ between UNROM / UOROM? */
	if (!strcmp(cart.slot,"unrom_cc")) { /* switch 0xc000 */
		prg_16low(0);
		prg_16high(bank * 0x4000);
	} else {							 /* switch 0x8000 */
		prg_16low(bank * 0x4000);
		prg_16high(cart.prgSize - 0x4000);
	}
}

/*/////////////////////////////////*/
/*              MMC 1              */
/*              SxROM              */
/*/////////////////////////////////*/

/* TODO:
 *
 * -PRG RAM banking
 */

/* mmc1 globals */
static uint_fast8_t mmc1Shift = 0, mmc1Buffer,
		mmc1Reg0, mmc1Reg1, mmc1Reg2, mmc1Reg3;
static uint32_t mmc1PrgOffset = 0;

static inline void mapper_mmc1(uint_fast16_t, uint_fast8_t), reset_mmc1(), mmc1_prg_bank_switch(), mmc1_chr_bank_switch();

void mapper_mmc1(uint_fast16_t address, uint_fast8_t value) {
	/* TODO: clean implementation - mmc1 checks write cycle instead */
	if (!dummywrite) {
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
				if (cart.prgSize == 0x80000) {
					mmc1PrgOffset = ((mmc1Reg1&0x10)<<14);
				}
				mmc1_chr_bank_switch();
				mmc1_prg_bank_switch();
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
}

void mmc1_prg_bank_switch() {
	uint32_t size;
	if (cart.prgSize == 0x80000)
		size = (cart.prgSize>>1);
	else size = cart.prgSize;
	 uint_fast8_t mmc1PrgSize = (mmc1Reg0 & 0x08); /* 0 32k, 1 16k */
	 uint_fast8_t mmc1PrgSelect = (mmc1Reg0 & 0x04); /* 0 low, 1 high */
	if (mmc1PrgSize) {
		if (mmc1PrgSelect) { /* switch 0x8000, fix 0xc000 to last bank */
			prg_16low((mmc1Reg3 & ((size >> 14) - 1)) * 0x4000 + mmc1PrgOffset);
			prg_16high(size - 0x4000);
		} else if (!mmc1PrgSelect) { /* switch 0xc000, fix 0x8000 to first bank */
			prg_16low(mmc1PrgOffset);
			prg_16high((mmc1Reg3 & ((size >> 14) - 1)) * 0x4000 + mmc1PrgOffset);
		}
	}
	else if (!mmc1PrgSize) {
		prg_32(((mmc1Reg3 & ((size >> 14) - 1)) >> 1) * 0x8000 + mmc1PrgOffset);

	}
}

void mmc1_chr_bank_switch() {
	uint_fast8_t mmc1ChrSize = (mmc1Reg0 & 0x10); /* 0 8k,  1 4k */
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
 *-Correct IRQ timing
 *-implement TQROM (mapper 119)
 * - implement TKSROM and TLSROM (mapper 118)
 *
 *Game specific:
 *-Rockman 5: IRQ issues - shaking screen
 */

static uint_fast8_t mmc3BankSelect, mmc3Reg[0x08], mmc3IrqEnable,
		mmc3PramProtect, mmc3IrqLatch, mmc3IrqReload, mmc3IrqCounter;
static inline void mapper_mmc3(uint_fast16_t, uint_fast8_t), reset_mmc3(), mmc3_prg_bank_switch(), mmc3_chr_bank_switch();

void mapper_mmc3 (uint_fast16_t address, uint_fast8_t value) {
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
					mmc3IrqReload = 1;
					mmc3IrqCounter = 0;
					}
				break;
			case 3:
				if (!(address%2)) { /* IRQ disable and acknowledge (0xE000) */
					mmc3IrqEnable = 0;
					mapperInt = 0;
				} else if (address%2) { /* IRQ enable (0xE001) */
					mmc3IrqEnable = 1;
				}
				break;
		}
}

void mmc3_prg_bank_switch() {
	if (mmc3BankSelect & 0x40) {
		prg_8_0(psize - 0x4000);
		prg_8_1((mmc3Reg[7] & ((cart.prgSize >> 13) - 1)) * 0x2000);
		prg_8_2((mmc3Reg[6] & ((cart.prgSize >> 13) - 1)) * 0x2000);
		prg_8_3(psize - 0x2000);
	} else if (!(mmc3BankSelect & 0x40)) {
		prg_8_0((mmc3Reg[6] & ((cart.prgSize >> 13) - 1)) * 0x2000);
		prg_8_1((mmc3Reg[7] & ((cart.prgSize >> 13) - 1)) * 0x2000);
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

void mmc3_irq ()
{
	if (mmc3IrqReload)
	{
		mmc3IrqReload = 0;
		mmc3IrqCounter = mmc3IrqLatch;
		if (mmc3IrqEnable && !mmc3IrqCounter)
		{
			mapperInt = 1;
			mmc3IrqReload = 1;
		}
	}
	else if (mmc3IrqCounter > 0)
	{
		mmc3IrqCounter--;
		if (mmc3IrqCounter == 0)
			{
				mmc3IrqReload = 1;
				if (mmc3IrqEnable)
					mapperInt = 1;
			}
	}
}

void reset_mmc3 () {
	memset(mmc3Reg, 0, 8);
	mmc3Reg[6] = (cart.prgSize / 0x2000) - 2;
	mmc3Reg[7] = (cart.prgSize / 0x2000) - 1;
	mmc3BankSelect = 0x00;
	mmc3_prg_bank_switch();
	mmc3_chr_bank_switch();
	mmc3IrqEnable = 0;
	mmc3IrqCounter = 0;
	mmc3IrqLatch = 0;
	mmc3IrqReload = 0;
}


/*/////////////////////////////////*/
/*            Irem G-101           */
/*/////////////////////////////////*/


static inline void reset_g101(), mapper_g101(uint_fast16_t, uint_fast8_t), g101_prg_bank_switch(), g101_chr_bank_switch();

static uint_fast8_t g101Prg0, g101Prg1, g101PrgMode,
	    g101Chr0, g101Chr1, g101Chr2, g101Chr3,
		g101Chr4, g101Chr5, g101Chr6, g101Chr7;

void mapper_g101(uint_fast16_t address, uint_fast8_t value)
{
	if ((address & 0xf007) >= 0x8000 && (address & 0xf007) < 0x9000) {
		g101Prg0 = value;
		g101_prg_bank_switch();
	} else if ((address & 0xf007) >= 0x9000 && (address & 0xf007) < 0xa000) {
		g101PrgMode = (value & 0x02);
		if (!(cart.mirroring == 3))
			cart.mirroring = (value & 0x01) ? 0 : 1;
		g101_prg_bank_switch();
	} else if ((address & 0xf007) >= 0xa000 && (address & 0xf007) < 0xb000) {
		g101Prg1 = value;
		g101_prg_bank_switch();
	} else if ((address & 0xf007) == 0xb000) {
		g101Chr0 = value;
		g101_chr_bank_switch();
	} else if ((address & 0xf007) == 0xb001) {
		g101Chr1 = value;
		g101_chr_bank_switch();
	} else if ((address & 0xf007) == 0xb002) {
		g101Chr2 = value;
		g101_chr_bank_switch();
	} else if ((address & 0xf007) == 0xb003) {
		g101Chr3 = value;
		g101_chr_bank_switch();
	} else if ((address & 0xf007) == 0xb004) {
		g101Chr4 = value;
		g101_chr_bank_switch();
	} else if ((address & 0xf007) == 0xb005) {
		g101Chr5 = value;
		g101_chr_bank_switch();
	} else if ((address & 0xf007) == 0xb006) {
		g101Chr6 = value;
		g101_chr_bank_switch();
	} else if ((address & 0xf007) == 0xb007) {
		g101Chr7 = value;
		g101_chr_bank_switch();
	}
}

void g101_prg_bank_switch() {
	if (g101PrgMode) {
		prg_8_0(cart.prgSize - 0x4000);
		prg_8_1((g101Prg1 & ((cart.prgSize >> 13) -1)) * 0x2000);
		prg_8_2((g101Prg0 & ((cart.prgSize >> 13) -1)) * 0x2000);
		prg_8_3(cart.prgSize - 0x2000);
	} else {
		prg_8_0((g101Prg0 & ((cart.prgSize >> 13) -1)) * 0x2000);
		prg_8_1((g101Prg1 & ((cart.prgSize >> 13) -1)) * 0x2000);
		prg_8_2(cart.prgSize - 0x4000);
		prg_8_3(cart.prgSize - 0x2000);
	}
}

void g101_chr_bank_switch() {
	chr_1_0((g101Chr0 & ((cart.chrSize >> 10) -1)) * 0x400);
	chr_1_1((g101Chr1 & ((cart.chrSize >> 10) -1)) * 0x400);
	chr_1_2((g101Chr2 & ((cart.chrSize >> 10) -1)) * 0x400);
	chr_1_3((g101Chr3 & ((cart.chrSize >> 10) -1)) * 0x400);
	chr_1_4((g101Chr4 & ((cart.chrSize >> 10) -1)) * 0x400);
	chr_1_5((g101Chr5 & ((cart.chrSize >> 10) -1)) * 0x400);
	chr_1_6((g101Chr6 & ((cart.chrSize >> 10) -1)) * 0x400);
	chr_1_7((g101Chr7 & ((cart.chrSize >> 10) -1)) * 0x400);
}

void reset_g101() {
	g101PrgMode = 0;
	g101Prg0 = (cart.prgSize / 0x2000) - 2;
	g101Prg1 = (cart.prgSize / 0x2000) - 1;
	g101_prg_bank_switch();
	chr_8(0);
}

/*/////////////////////////////////*/
/*            Konami VRC           */
/*/////////////////////////////////*/

static inline void vrc_clock_irq();
static uint_fast8_t vrcIrqControl = 0, vrcIrqLatch, vrcIrqCounter, vrcIrqCycles[3] = { 114, 114, 113 }, vrcIrqCc = 0;
static int16_t vrcIrqPrescale;

/*/////////////////////////////////*/
/*              VRC 1              */
/*/////////////////////////////////*/

uint_fast8_t vrc1Chr0, vrc1Chr1;
static inline void mapper_vrc1(uint_fast16_t, uint_fast8_t);

void mapper_vrc1(uint_fast16_t address, uint_fast8_t value)
{
	if (address >= 0x8000 && address <= 0x8fff) /* PRG select 0 */
	{
		prgBank[0] = ((value & 0x0f) << 1);
		prgBank[1] = prgBank[0] + 1;
		prg_bank_switch();
	}
	if (address >= 0xa000 && address <= 0xafff) /* PRG select 1 */
	{
		prgBank[2] = ((value & 0x0f) << 1);
		prgBank[3] = prgBank[2] + 1;
		prg_bank_switch();
	}
	if (address >= 0xc000 && address <= 0xcfff) /* PRG select 2 */
	{
		prgBank[4] = ((value & 0x0f) << 1);
		prgBank[5] = prgBank[4] + 1;
		prg_bank_switch();
	}
	if (address >= 0x9000 && address <= 0x9fff) /* Mirroring + CHR */
	{
		cart.mirroring = (1 - (value & 0x01));
		vrc1Chr0 = ((vrc1Chr0 & 0x0f) | ((value & 0x02) << 3));
		vrc1Chr1 = ((vrc1Chr1 & 0x0f) | ((value & 0x04) << 2));
		chrBank[0] = (vrc1Chr0 << 2);
		chrBank[1] = chrBank[0] + 1;
		chrBank[2] = chrBank[0] + 2;
		chrBank[3] = chrBank[0] + 3;
		chrBank[4] = (vrc1Chr1 << 2);
		chrBank[5] = chrBank[4] + 1;
		chrBank[6] = chrBank[4] + 2;
		chrBank[7] = chrBank[4] + 3;
		chr_bank_switch();
	}
	if (address >= 0xe000 && address <= 0xefff) /* CHR select 0 */
	{
		vrc1Chr0 = ((vrc1Chr0 & 0x10) | (value & 0x0f));
		chrBank[0] = (vrc1Chr0 << 2);
		chrBank[1] = chrBank[0] + 1;
		chrBank[2] = chrBank[0] + 2;
		chrBank[3] = chrBank[0] + 3;
		chr_bank_switch();
	}
	if (address >= 0xf000 && address <= 0xffff) /* CHR select 1 */
	{
		vrc1Chr1 = ((vrc1Chr1 & 0x10) | (value & 0x0f));
		chrBank[4] = (vrc1Chr1 << 2);
		chrBank[5] = chrBank[4] + 1;
		chrBank[6] = chrBank[4] + 2;
		chrBank[7] = chrBank[4] + 3;
		chr_bank_switch();
	}

}

/*/////////////////////////////////*/
/*            VRC 2 / 4            */
/*/////////////////////////////////*/

static uint_fast8_t vrc24SwapMode, vrcPrg0, vrcPrg1;
static uint_fast16_t vrcChr0 = 0, vrcChr1 = 0, vrcChr2 = 0, vrcChr3 = 0,
		 vrcChr4 = 0, vrcChr5 = 0, vrcChr6 = 0, vrcChr7 = 0;

static inline void mapper_vrc24(uint_fast16_t, uint_fast8_t), reset_vrc24(), vrc24_chr_bank_switch(), vrc24_prg_bank_switch();

void mapper_vrc24(uint_fast16_t address, uint_fast8_t value) {
	/* reroute addressing */
	if (cart.vrc24Prg1 > 1)
		address = (address & 0xff00) | ((address>>(cart.vrc24Prg1-1)) & 0x02) | ((address>>cart.vrc24Prg0) & 0x01);
	else
		address = (address & 0xff00) | ((address<<(1-cart.vrc24Prg1)) & 0x02) | ((address>>cart.vrc24Prg0) & 0x01);

	/* handle register writes */
	if ((address&0xf003) >= 0x8000 && (address&0xf003) <= 0x8003) { /* PRG select 0 */
		vrcPrg0 = value;
		vrc24_prg_bank_switch();
	} else if ((address&0xf003) >= 0xa000  && (address&0xf003) <= 0xa003) { /* PRG select 1 */
		vrcPrg1 = value;
		vrc24_prg_bank_switch();
	} else if ((address&0xf003) >= 0x9000  && (address&0xf003) <= 0x9003) { /* mirroring control */
		if (!strcmp(cart.slot,"vrc4") && (address&0xf003) >= 0x9002) {
			vrc24SwapMode = ((value >> 1) & 0x01);
			vrc24_prg_bank_switch();
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
		vrcIrqLatch = (vrcIrqLatch & 0x0f) | ((value & 0x0f) << 4);
	} else if ((address&0xf003) == 0xf002) { /* IRQ Control */
		vrcIrqControl = (value & 0x07);
		if (vrcIrqControl & 0x02) {
			vrcIrqCounter = vrcIrqLatch;
			vrcIrqPrescale = vrcIrqCycles[0];
			vrcIrqCc = 0;
		}
		mapperInt = 0;
	} else if ((address&0xf003) == 0xf003) { /* IRQ Acknowledge */
		mapperInt = 0;
		vrcIrqControl = ((vrcIrqControl & 0x04) | ((vrcIrqControl & 0x01) << 1) | (vrcIrqControl & 0x01));
	}
}

void vrc24_prg_bank_switch() {
	if (vrc24SwapMode) {
		prg_8_0(cart.prgSize - 0x4000);
		prg_8_2((vrcPrg0 & ((cart.prgSize>>13)-1)) * 0x2000);
	}
	else {
		prg_8_0((vrcPrg0 & ((cart.prgSize>>13)-1)) * 0x2000);
		prg_8_2(cart.prgSize - 0x4000);
	}
	prg_8_1((vrcPrg1 & ((cart.prgSize>>13)-1)) * 0x2000);
}

void vrc24_chr_bank_switch() {
	if (!strcmp(cart.slot,"vrc2")) {
		if (cart.vrc24Chr) {
			chr_1_0((vrcChr0 & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_1((vrcChr1 & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_2((vrcChr2 & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_3((vrcChr3 & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_4((vrcChr4 & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_5((vrcChr5 & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_6((vrcChr6 & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_7((vrcChr7 & ((cart.chrSize >> 10) -1)) * 0x400);
		} else if (!cart.vrc24Chr) {
			chr_1_0(((vrcChr0 >> 1) & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_1(((vrcChr1 >> 1) & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_2(((vrcChr2 >> 1) & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_3(((vrcChr3 >> 1) & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_4(((vrcChr4 >> 1) & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_5(((vrcChr5 >> 1) & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_6(((vrcChr6 >> 1) & ((cart.chrSize >> 10) -1)) * 0x400);
			chr_1_7(((vrcChr7 >> 1) & ((cart.chrSize >> 10) -1)) * 0x400);
		}
	} else if (!strcmp(cart.slot,"vrc4")){
		chr_1_0((vrcChr0 & ((cart.chrSize >> 10) -1)) * 0x400);
		chr_1_1((vrcChr1 & ((cart.chrSize >> 10) -1)) * 0x400);
		chr_1_2((vrcChr2 & ((cart.chrSize >> 10) -1)) * 0x400);
		chr_1_3((vrcChr3 & ((cart.chrSize >> 10) -1)) * 0x400);
		chr_1_4((vrcChr4 & ((cart.chrSize >> 10) -1)) * 0x400);
		chr_1_5((vrcChr5 & ((cart.chrSize >> 10) -1)) * 0x400);
		chr_1_6((vrcChr6 & ((cart.chrSize >> 10) -1)) * 0x400);
		chr_1_7((vrcChr7 & ((cart.chrSize >> 10) -1)) * 0x400);
	}
}

uint_fast8_t wramBit = 0, wramBitVal;
void reset_vrc24() {
	vrc24SwapMode = 0;
	vrcPrg0 = 0;
	vrcPrg1 = 0;
	prg_8_3(cart.prgSize-0x2000);
	vrc24_prg_bank_switch();
	chr_8(0);
	if (!strcmp(cart.slot,"vrc2") && (!cart.wramSize && !cart.bwramSize))
		wramBit = 1;
}

/*/////////////////////////////////*/
/*              VRC 6              */
/*/////////////////////////////////*/

static uint_fast8_t vrc6Pulse1Mode, vrc6Pulse1Duty, vrc6Pulse1Volume, vrc6Pulse2Mode, vrc6Pulse2Duty, vrc6Pulse2Volume,
					vrc6SawAccumulator,	vrc6Pulse1Enable, vrc6Pulse2Enable, vrc6SawEnable, vrc6Pulse1DutyCounter, vrc6Pulse2DutyCounter,
					vrc6SawAccCounter = 0, vrc6SawAcc = 0;
static uint_fast16_t vrc6Pulse1Period, vrc6Pulse2Period, vrc6SawPeriod, vrc6Pulse1Counter = 0, vrc6Pulse2Counter = 0, vrc6SawCounter = 0;
static inline void mapper_vrc6(uint_fast16_t, uint_fast8_t), reset_vrc6();


void mapper_vrc6(uint_fast16_t address, uint_fast8_t value)
{
	address = (address & 0xff00) | ((address<<(1-cart.vrc6Prg1)) & 0x02) | ((address>>cart.vrc6Prg0) & 0x01);
	if ((address&0xf003) >= 0x8000 && (address&0xf003) <= 0x8003) /* 16k PRG select */
	{
		prgBank[0] = ((value & 0x0f) << 2);
		prgBank[1] = prgBank[0] + 1;
		prgBank[2] = prgBank[0] + 2;
		prgBank[3] = prgBank[0] + 3;
		prg_bank_switch();
	}
	else if ((address&0xf003) >= 0xc000 && (address&0xf003) <= 0xc003) /* 8k PRG select */
	{
		prgBank[4] = ((value & 0x1f) << 1);
		prgBank[5] = prgBank[4] + 1;
		prg_bank_switch();
	}
	else if ((address&0xf003) == 0xb003) /* PPU banking */
	{
	/*	printf("Mirroring: %02x, %i,%i\n",value,scanline,ppudot); */
		switch (value & 0x0c)
		{
		case 0x00:
			cart.mirroring = 1;
			break;
		case 0x04:
			cart.mirroring = 0;
			break;
		case 0x08:
			cart.mirroring = 2;
			break;
		case 0x0c:
			cart.mirroring = 3;
			break;
		}
		wramEnable = (value >> 7);
	}
	else if ((address&0xf003) == 0xd000) /* CHR select */
	{
		chrBank[0] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xd001) /* CHR select */
	{
		chrBank[1] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xd002) /* CHR select */
	{
		chrBank[2] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xd003) /* CHR select */
	{
		chrBank[3] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xe000) /* CHR select */
	{
		chrBank[4] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xe001) /* CHR select */
	{
		chrBank[5] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xe002) /* CHR select */
	{
		chrBank[6] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xe003) /* CHR select */
	{
		chrBank[7] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xf000) /* IRQ Latch */
	{
		vrcIrqLatch = value;
	}
	else if ((address&0xf003) == 0xf001) /* IRQ Control */
	{
		vrcIrqControl = (value & 0x07);
		if (vrcIrqControl & 0x02)
		{
			vrcIrqCounter = vrcIrqLatch;
			vrcIrqPrescale = vrcIrqCycles[0];
			vrcIrqCc = 0;
		}
		mapperInt = 0;
	}
	else if ((address&0xf003) == 0xf002) /* IRQ Acknowledge */
	{
		mapperInt = 0;
		vrcIrqControl = ((vrcIrqControl & 0x04) | ((vrcIrqControl & 0x01) << 1) | (vrcIrqControl & 0x01));
	}
	else if ((address&0xf003) == 0x9000) /* Pulse 1 control */
	{
		vrc6Pulse1Mode = (value & 0x80);
		vrc6Pulse1Duty = ((value >> 4) & 7);
		vrc6Pulse1Volume = (value & 0xf);
	}
	else if ((address&0xf003) == 0x9001) /* Pulse 1 period low */
	{
		vrc6Pulse1Period = ((vrc6Pulse1Period & 0x0f00) | value);
	}
	else if ((address&0xf003) == 0x9002) /* Pulse 1 period high */
	{
		vrc6Pulse1Period = ((vrc6Pulse1Period & 0x00ff) | ((value & 0xf) << 8));
		vrc6Pulse1Enable = (value & 0x80);
		if (!vrc6Pulse1Enable)
			vrc6Pulse1DutyCounter = 15;
	}
	else if ((address&0xf003) == 0x9003) /* Pulse 1 frequency */
	{
		/* unused by commerical games */
	}
	else if ((address&0xf003) == 0xa000) /* Pulse 2 control */
	{
		vrc6Pulse2Mode = (value & 0x80);
		vrc6Pulse2Duty = ((value >> 4) & 7);
		vrc6Pulse2Volume = (value & 0xf);
	}
	else if ((address&0xf003) == 0xa001) /* Pulse 2 period low */
	{
		vrc6Pulse2Period = ((vrc6Pulse2Period & 0x0f00) | value);
	}
	else if ((address&0xf003) == 0xa002) /* Pulse 2 period high */
	{
		vrc6Pulse2Period = ((vrc6Pulse2Period & 0x00ff) | ((value & 0xf) << 8));
		vrc6Pulse2Enable = (value & 0x80);
		if (!vrc6Pulse2Enable)
			vrc6Pulse2DutyCounter = 15;
	}
	else if ((address&0xf003) == 0xa003) /* Pulse 2 frequency */
	{
		/* unused by commerical games */
	}
	else if ((address&0xf003) == 0xb000) /* Saw accumulator */
	{
		vrc6SawAccumulator = (value & 0x3f);
	}
	else if ((address&0xf003) == 0xb001) /* Saw period low */
	{
		vrc6SawPeriod = ((vrc6SawPeriod & 0x0f00) | value);
	}
	else if ((address&0xf003) == 0xb002) /* Saw period high */
	{
		vrc6SawPeriod = ((vrc6SawPeriod & 0x00ff) | ((value & 0xf) << 8));
		vrc6SawEnable = (value & 0x80);
		if (!vrc6SawEnable)
		{
			vrc6SawAccCounter = 0;
			vrc6SawAcc = 0;
		}
	}
}

float vrc6_sound()
{
	uint_fast8_t sample1 = 0;
	uint_fast8_t sample2 = 0;
	uint_fast8_t sample3 = (vrc6SawAcc >> 3);

	if (((vrc6Pulse1DutyCounter <= vrc6Pulse1Duty) || vrc6Pulse1Mode) && vrc6Pulse1Enable)
		sample1 = vrc6Pulse1Volume;
	if (!vrc6Pulse1Counter)
	{
		vrc6Pulse1Counter = vrc6Pulse1Period;
		if (!vrc6Pulse1DutyCounter)
			vrc6Pulse1DutyCounter = 15;
		else if (vrc6Pulse1Enable)
		{
			vrc6Pulse1DutyCounter--;
		}
	}
	else
		vrc6Pulse1Counter--;

	if (((vrc6Pulse2DutyCounter <= vrc6Pulse2Duty) || vrc6Pulse2Mode) && vrc6Pulse2Enable)
			sample2 = vrc6Pulse2Volume;
	if (!vrc6Pulse2Counter)
	{
		vrc6Pulse2Counter = vrc6Pulse2Period;
		if (!vrc6Pulse2DutyCounter)
			vrc6Pulse2DutyCounter = 15;
		else if (vrc6Pulse2Enable)
		{
			vrc6Pulse2DutyCounter--;
		}
	}
	else
		vrc6Pulse2Counter--;

	if (!vrc6SawCounter)
	{
		vrc6SawCounter = vrc6SawPeriod;
		if (vrc6SawAccCounter == 13)
		{
			vrc6SawAccCounter = 0;
			vrc6SawAcc = 0;
		}
		else if (vrc6SawEnable)
		{
			vrc6SawAccCounter++;
			if (!(vrc6SawAccCounter%2))
			{
				vrc6SawAcc += vrc6SawAccumulator;
			}
		}
	}
	else
		vrc6SawCounter--;

	return ((sample1 + sample2 + sample3));
}

void reset_vrc6()
{
	prgBank[0] = 0;
	prgBank[1] = 1;
	prgBank[2] = 2;
	prgBank[3] = 3;
	prgBank[4] = 4;
	prgBank[5] = 5;
	prgBank[6] = cart.pSlots - 2;
	prgBank[7] = cart.pSlots - 1;
	prg_bank_switch();
	chrBank[0] = 0;
	chrBank[1] = 1;
	chrBank[2] = 2;
	chrBank[3] = 3;
	chrBank[4] = 4;
	chrBank[5] = 5;
	chrBank[6] = 6;
	chrBank[7] = 7;
	chr_bank_switch();
}

void vrc_clock_irq()
{
	if (vrcIrqCounter == 0xff) {
		mapperInt = 1;
		irqPulled = 1; /* otherwise gets read too late */
		vrcIrqCounter = vrcIrqLatch;
	}
	else
	{
		vrcIrqCounter++;
	}
}

void vrc_irq() {
	if ((vrcIrqControl & 0x02)) {
		if (!(vrcIrqControl & 0x04)) {
			vrcIrqPrescale--;
			if (vrcIrqPrescale == 0)
				{
				vrcIrqPrescale = vrcIrqCycles[vrcIrqCc++];
				if (vrcIrqCc == 3)
					vrcIrqCc = 0;
				vrc_clock_irq();
				}
		} else if (vrcIrqControl & 0x04) {
			vrc_clock_irq();
		}
	}
}

void reset_default()
{
	prgBank[0] = 0;
	prgBank[1] = 1;
	prgBank[2] = 2;
	prgBank[3] = 3;
	prgBank[4] = cart.pSlots - 4;
	prgBank[5] = cart.pSlots - 3;
	prgBank[6] = cart.pSlots - 2;
	prgBank[7] = cart.pSlots - 1;
	prg_bank_switch();
	chrBank[0] = 0;
	chrBank[1] = 1;
	chrBank[2] = 2;
	chrBank[3] = 3;
	chrBank[4] = 4;
	chrBank[5] = 5;
	chrBank[6] = 6;
	chrBank[7] = 7;
	chr_bank_switch();
}

void prg_bank_switch()
{
	prgSlot[0] = &prg[((prgBank[0] & (cart.pSlots - 1)) << 12)];
	prgSlot[1] = &prg[((prgBank[1] & (cart.pSlots - 1)) << 12)];
	prgSlot[2] = &prg[((prgBank[2] & (cart.pSlots - 1)) << 12)];
	prgSlot[3] = &prg[((prgBank[3] & (cart.pSlots - 1)) << 12)];
	prgSlot[4] = &prg[((prgBank[4] & (cart.pSlots - 1)) << 12)];
	prgSlot[5] = &prg[((prgBank[5] & (cart.pSlots - 1)) << 12)];
	prgSlot[6] = &prg[((prgBank[6] & (cart.pSlots - 1)) << 12)];
	prgSlot[7] = &prg[((prgBank[7] & (cart.pSlots - 1)) << 12)];
}

void chr_bank_switch()
{
	chrSlot[0] = &chr[((chrBank[0] & (cart.cSlots - 1)) << 10)];
	chrSlot[1] = &chr[((chrBank[1] & (cart.cSlots - 1)) << 10)];
	chrSlot[2] = &chr[((chrBank[2] & (cart.cSlots - 1)) << 10)];
	chrSlot[3] = &chr[((chrBank[3] & (cart.cSlots - 1)) << 10)];
	chrSlot[4] = &chr[((chrBank[4] & (cart.cSlots - 1)) << 10)];
	chrSlot[5] = &chr[((chrBank[5] & (cart.cSlots - 1)) << 10)];
	chrSlot[6] = &chr[((chrBank[6] & (cart.cSlots - 1)) << 10)];
	chrSlot[7] = &chr[((chrBank[7] & (cart.cSlots - 1)) << 10)];
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

void init_mapper() {
	if(!strcmp(cart.slot,"nrom")) {
		reset_default();
	} else if(!strcmp(cart.slot,"sxrom") ||
				!strcmp(cart.slot,"sxrom_a") ||
					!strcmp(cart.slot,"sorom") ||
						!strcmp(cart.slot,"sorom_a")) {
		reset_mmc1();
	} else if(!strcmp(cart.slot,"uxrom") ||
				!strcmp(cart.slot,"un1rom") ||
					!strcmp(cart.slot,"unrom_cc")) {
		reset_default();
	} else if (!strcmp(cart.slot,"cnrom")) {
		reset_default();
	} else if (!strcmp(cart.slot,"axrom")) {
		reset_default();
	} else if (!strcmp(cart.slot,"txrom")) {
		reset_mmc3();
	} else if (!strcmp(cart.slot,"vrc1")) {
		reset_default();
	}
	else if (!strcmp(cart.slot,"vrc2") ||
			!strcmp(cart.slot,"vrc4")) {
		reset_vrc24();
	} else if (!strcmp(cart.slot,"vrc6")) {
	    expansion_sound = &vrc6_sound;
	    expSound = 1;
		reset_vrc6();
	} else if (!strcmp(cart.slot,"g101")) {
		reset_g101();
	} else
		reset_default();
}

void write_mapper_register(uint_fast16_t address, uint_fast8_t value) {

	if ((!strcmp(cart.slot,"sxrom") ||
			!strcmp(cart.slot,"sxrom_a") ||
				!strcmp(cart.slot,"sorom") ||
					!strcmp(cart.slot,"sorom_a"))) {
		mapper_mmc1(address, value);
	}
	else if (!strcmp(cart.slot,"uxrom") ||
				!strcmp(cart.slot,"un1rom") ||
					!strcmp(cart.slot,"unrom_cc")) {
		mapper_uxrom(value);
	}
	else if (!strcmp(cart.slot,"cnrom")) {
		mapper_cnrom(value);
	}
	else if (!strcmp(cart.slot,"txrom")) {
		mapper_mmc3(address,value);
	}
	else if (!strcmp(cart.slot,"axrom")) {
		mapper_axrom(value);
	}
	else if (!strcmp(cart.slot,"vrc1")) {
		mapper_vrc1(address,value);
	}
	else if (!strcmp(cart.slot,"vrc2") ||
				!strcmp(cart.slot,"vrc4")) {
		mapper_vrc24(address,value);
	}
	else if (!strcmp(cart.slot,"vrc6")) {
		mapper_vrc6(address,value);
	}
	else if (!strcmp(cart.slot,"g101")) {
		mapper_g101(address,value);
	}
}
