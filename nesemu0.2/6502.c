#include "6502.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>	/* memcpy */
#include "globals.h"
#include "nestools.h"
#include "ppu.h"
#include "apu.h"
#include "mapper.h"

						 /* 0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f */
static uint8_t ctable[] = { 7, 6, 0, 0, 3, 3, 5, 0, 3, 2, 2, 0, 4, 4, 6, 0,/* 0 */
							2, 5, 0, 0, 4, 4, 6, 0, 2, 4, 2, 0, 4, 4, 7, 0,/* 1 */
							6, 6, 0, 0, 3, 3, 5, 0, 4, 2, 2, 0, 4, 4, 6, 0,/* 2 */
							2, 5, 0, 0, 4, 4, 6, 0, 2, 4, 2, 0, 4, 4, 7, 0,/* 3 */
							6, 6, 0, 0, 3, 3, 5, 0, 3, 2, 2, 0, 3, 4, 6, 0,/* 4 */
							2, 5, 0, 0, 4, 4, 6, 0, 2, 4, 2, 0, 4, 4, 7, 0,/* 5 */
							6, 6, 0, 0, 3, 3, 5, 0, 4, 2, 2, 0, 5, 4, 6, 0,/* 6 */
							2, 5, 0, 0, 4, 4, 6, 0, 2, 4, 2, 0, 4, 4, 7, 0,/* 7 */
							2, 6, 2, 0, 3, 3, 3, 0, 2, 2, 2, 0, 4, 4, 4, 0,/* 8 */
							2, 6, 0, 0, 4, 4, 4, 0, 2, 5, 2, 0, 0, 5, 0, 0,/* 9 */
							2, 6, 2, 0, 3, 3, 3, 0, 2, 2, 2, 0, 4, 4, 4, 0,/* a */
							2, 5, 0, 0, 4, 4, 4, 0, 2, 4, 2, 0, 4, 4, 4, 0,/* b */
							2, 6, 2, 0, 3, 3, 5, 0, 2, 2, 2, 0, 4, 4, 6, 0,/* c */
							2, 5, 0, 0, 4, 4, 6, 0, 2, 4, 2, 0, 4, 4, 7, 0,/* d */
							2, 6, 2, 0, 3, 3, 5, 0, 2, 2, 2, 0, 4, 4, 6, 0,/* e */
							2, 5, 0, 0, 4, 4, 6, 0, 2, 4, 2, 0, 4, 4, 7, 0 /* f */
							};

static inline void mirror(), memread(), memwrite();
static inline void accum(), immed(), zpage(), zpagex(), zpagey(), absol(),
		absx(), absy(), indx(), indy();
static inline void adc(), and(), asl(), branch(), bit(), brkop(), clc(), cld(),
		cli(), clv(), cmp(), cpx(), cpy(), dec(), dex(), dey(), eor(), inc(),
		inx(), iny(), jmpa(), jmpi(), jsr(), lda(), ldx(), ldy(), lsr(), ora(),
		pha(), php(), pla(), plp(), rol(), ror(), rti(), rts(), sbc(), sec(),
		sed(), sei(), sta(), stx(), sty(), tax(), tay(), tsx(), txa(), txs(),
		tya(), none();
uint8_t *prgSlot[0x8], cpuRam[0x2000];
uint8_t mode, opcode, addmode, addcycle, *addval, tmpval8, vbuff = 0, s = 0, vraminc, ppureg = 0, w = 0;
uint16_t addr, tmpval16;

int test = 0;

void opdecode(uint8_t op) {
/*	printf("Opcode: %02X at PC: %04X\n",op,pc); */
	test++;
	addcycle = 0;
	addr = 0;
	static void (*addp0[8])() = {immed,zpage,accum,absol,absy,zpagex,zpagey,absx};
	static void (*addp1[8])() = {indx,zpage,immed,absol,indy,zpagex,absy,absx};
	static void (*opp0[8])() = {none,bit,none,none,sty,ldy,cpy,cpx};
	static void (*opp0ex2[8])() = {php,plp,pha,pla,dey,tay,iny,inx};
	static void (*opp0ex6[8])() = {clc,sec,cli,sei,tya,clv,cld,sed};
	static void (*opp1[8])() = {ora,and,eor,adc,sta,lda,cmp,sbc};
	static void (*opp2[8])() = {asl,rol,lsr,ror,stx,ldx,dec,inc};
	apu_wait += ctable[op];
	ppu_wait += ctable[op] * 3;
	cpucc += ctable[op];
	mode = op & 0x03;
	opcode = (op >> 5) & 0x07;
	addmode = (op >> 2) & 0x07;
	switch (mode) {
	case 0:
		if (opcode == 0 && addmode == 0) {
			brkop();
			break;
		} else if (opcode == 0 && (addmode == 1 || addmode == 5)) {
			pc++;
			break;
		} else if (opcode == 0 && addmode == 3) {
			pc += 2; /* (TOP) TODO: extra cycles */
			break;
		} else if (opcode == 1 && addmode == 0) {
			jsr();
			break;
		} else if (opcode == 1 && addmode == 5) {
			pc++;
			break;
		} else if (opcode == 2 && addmode == 0) {
			rti();
			break;
		} else if (opcode == 2 && (addmode == 1 || addmode == 5)) {
			pc++;
			break;
		} else if (opcode == 2 && addmode == 3) {
			jmpa();
			break;
		} else if (opcode == 3 && addmode == 0) {
			rts();
			break;
		} else if (opcode == 3 && (addmode == 1 || addmode == 5)) {
			pc++;
			break;
		} else if (opcode == 3 && addmode == 3) {
			jmpi();
			break;
		} else if (opcode == 4 && addmode == 0) {
			pc++;
			break;
		} else if (opcode == 6 && addmode == 5) {
			pc++;
			break;
		} else if (opcode == 7 && addmode == 5) {
			pc++;
			break;
		} else if ((opcode == 0 || opcode == 1 || opcode == 2 || opcode == 3 || opcode == 6 || opcode == 7) && addmode == 7) {
			pc += 2; /* (TOP) TODO: extra cycles */
			break;
		} else if (addmode == 2) {
			(*opp0ex2[opcode])();
			break;
		} else if (addmode == 4) {
			branch();
			break;
		} else if (addmode == 6) {
			(*opp0ex6[opcode])();
			break;
		}
		(*addp0[addmode])();
		(*opp0[opcode])();
		break;
	case 1:
		if (opcode == 4 && addmode == 2) {
			pc++;
			break;
		}
		(*addp1[addmode])();
		(*opp1[opcode])();
		break;
	case 2:
		if 		  (opcode == 4 && addmode == 0) {
			pc++;
			break;
		} else if (opcode == 4 && addmode == 2) {
			txa();
			break;
		} else if (opcode == 4 && addmode == 6) {
			txs();
			break;
		} else if (opcode == 5 && addmode == 2) {
			tax();
			break;
		} else if (opcode == 5 && addmode == 6) {
			tsx();
			break;
		} else if (opcode == 6 && addmode == 0) {
			pc++;
			break;
		} else if (opcode == 6 && addmode == 2) {
			dex();
			break;
		} else if (opcode == 7 && addmode == 0) {
			pc++;
			break;
		} else if (opcode == 7 && addmode == 2)
			break;
		  else if ((opcode == 0 || opcode == 1 || opcode == 2 || opcode == 3 || opcode == 6 || opcode == 7) && addmode == 6)
			break;
		  else if (opcode == 4 && addmode == 5)
			addmode = 6;
		  else if (opcode == 5 && addmode == 5)
			addmode = 6;
		  else if (opcode == 5 && addmode == 7)
			addmode = 4;
		(*addp0[addmode])();
		(*opp2[opcode])();
		break;
	}
}

void mirror() {
	if (addr < 0x2000)
		addr = (addr & 0x7ff);
	else if (addr >= 0x2000 && addr < 0x4000)
		addr = (addr & 0x2007);
}

/*ADDRESS MODES */
void accum() {
	addval = &a;
}

void immed() {
	addr = pc++;
	addval = cpuread(addr);
}

void zpage() {
	addr = *cpuread(pc++);
	addval = cpuread(addr);
}

void zpagex() {
	addr = *cpuread(pc++);
	addr = ((addr + x) & 0xff);
	addval = cpuread(addr);
}

void zpagey() {
	addr = *cpuread(pc++);
	addr = ((addr + y) & 0xff);
	addval = cpuread(addr);
}

void absol() {
	addr = *cpuread(pc++);
	addr += *cpuread(pc++) << 8;
	mirror();
	addval = cpuread(addr);
}

void absx() {
	addr = *cpuread(pc++);
	addr += *cpuread(pc++) << 8;
	addr += x;
	if ((addr & 0xff) < x) {
		addcycle = 1;
	}
	mirror();
	addval = cpuread(addr);
}

void absy() {
	addr = *cpuread(pc++);
	addr += *cpuread(pc++) << 8;
	addr += y;
	if ((addr & 0xff) < y) {
		addcycle = 1;
	}
	mirror();
	addval = cpuread(addr);
}

void indx() {
	tmpval8 = *cpuread(pc++);
	addr = *cpuread(((tmpval8+x) & 0xff));
	addr += *cpuread(((tmpval8+x+1) & 0xff)) << 8;
	mirror();
	addval = cpuread(addr);
}

void indy() {
	tmpval8 = *cpuread(pc++);
	addr = *cpuread(tmpval8++);
	addr += *cpuread((tmpval8 & 0xff)) << 8;
	addr = ((addr + y) & 0xffff);
	if ((addr & 0xff) < y) {
		addcycle = 1;
	}
	mirror();
	addval = cpuread(addr);
}

		/* OPCODES */
void adc() {
	apu_wait += addcycle;
	ppu_wait += addcycle * 3;
	cpucc += addcycle;
	run_ppu(ppu_wait);
	memread();
	tmpval16 = a + tmpval8 + (flag & 1);
	bitset(&flag, (a ^ tmpval16) & (tmpval8 ^ tmpval16) & 0x80, 6);
	bitset(&flag, tmpval16 > 0xff, 0);
	a = tmpval16;
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void and() {
	apu_wait += addcycle;
	ppu_wait += addcycle * 3;
	cpucc += addcycle;
	run_ppu(ppu_wait);
	memread();
	a &= tmpval8;
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void asl() {
	run_ppu(ppu_wait);
	bitset(&flag, *addval & 0x80, 0);
	tmpval8 = *addval << 1;
	bitset(&flag, tmpval8 == 0, 1);
	bitset(&flag, tmpval8 >= 0x80, 7);
	memwrite();
}

void bit() {
	run_ppu(ppu_wait);
	memread();
	bitset(&flag, !(a & tmpval8), 1);
	bitset(&flag, tmpval8 & 0x80, 7);
	bitset(&flag, tmpval8 & 0x40, 6);
}

void branch() {
	uint8_t reflag[4] = { 7, 6, 0, 1 };
	if (((flag >> reflag[(opcode >> 1) & 3]) & 1) == (opcode & 1)) {
		if (((pc + 1) & 0xff00)	!= ((pc + ((int8_t) *cpuread(pc) + 1)) & 0xff00)) {
			apu_wait += 1;
			ppu_wait += 3;
			cpucc += 1;
		}
		pc = pc + (int8_t) *cpuread(pc) + 1;
		apu_wait += 1;
		ppu_wait += 3;
		cpucc += 1;
	} else
		pc++;
}

void brkop() {
	pc++;
	nmiVblankTriggered = 0;
	donmi();
}

void clc() {
	bitset(&flag, 0, 0);
}

void cld() {
	bitset(&flag, 0, 3);
}

void cli() {
	bitset(&flag, 0, 2);
}

void clv() {
	bitset(&flag, 0, 6);
}

void cmp() {
	apu_wait += addcycle;
	ppu_wait += addcycle * 3;
	cpucc += addcycle;
	run_ppu(ppu_wait);
	memread();
	bitset(&flag, (a - tmpval8) & 0x80, 7);
	bitset(&flag, a == tmpval8, 1);
	bitset(&flag, a >= tmpval8, 0);
}

void cpx() {
	run_ppu(ppu_wait);
	memread();
	bitset(&flag, (x - tmpval8) & 0x80, 7);
	bitset(&flag, x == tmpval8, 1);
	bitset(&flag, x >= tmpval8, 0);
}

void cpy() {
	run_ppu(ppu_wait);
	memread();
	bitset(&flag, (y - tmpval8) & 0x80, 7);
	bitset(&flag, y == tmpval8, 1);
	bitset(&flag, y >= tmpval8, 0);
}

void dec() {
	run_ppu(ppu_wait);
	tmpval8 = *addval-1;
	bitset(&flag, tmpval8 == 0, 1);
	bitset(&flag, tmpval8 >= 0x80, 7);
	memwrite();
}

void dex() {
	x--;
	bitset(&flag, x == 0, 1);
	bitset(&flag, x >= 0x80, 7);
}

void dey() {
	y--;
	bitset(&flag, y == 0, 1);
	bitset(&flag, y >= 0x80, 7);
}

void eor() {
	apu_wait += addcycle;
	ppu_wait += addcycle * 3;
	cpucc += addcycle;
	run_ppu(ppu_wait);
	memread();
	a ^= tmpval8;
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void inc() {
	run_ppu(ppu_wait);
	tmpval8 = *addval + 1;
	bitset(&flag, tmpval8 == 0, 1);
	bitset(&flag, tmpval8 >= 0x80, 7);
	memwrite();
}

void inx() {
	x++;
	bitset(&flag, x == 0, 1);
	bitset(&flag, x >= 0x80, 7);
}

void iny() {
	y++;
	bitset(&flag, y == 0, 1);
	bitset(&flag, y >= 0x80, 7);
}

void jmpa() {
	addr = *cpuread(pc++);
	addr += *cpuread(pc++) << 8;
	pc = addr;
}

void jmpi() {
	tmpval8 = *cpuread(pc++);
	tmpval16 = (*cpuread(pc) << 8);
	addr = *cpuread(tmpval16 | tmpval8);
	addr += *cpuread(tmpval16 | ((tmpval8+1) & 0xff)) << 8;
	pc = addr;
}

void jsr() {
	*cpuread(sp--) = ((pc + 1) & 0xff00) >> 8;
	*cpuread(sp--) = ((pc + 1) & 0x00ff);
	addr = *cpuread(pc++);
	addr += *cpuread(pc) << 8;
	pc = addr;
}

void lda() {
	apu_wait += addcycle;
	ppu_wait += addcycle * 3;
	cpucc += addcycle;
	run_ppu(ppu_wait);
	memread();
	a = tmpval8;
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void ldx() {
	apu_wait += addcycle;
	ppu_wait += addcycle * 3;
	cpucc += addcycle;
	run_ppu(ppu_wait);
	memread();
	x = tmpval8;
	bitset(&flag, x == 0, 1);
	bitset(&flag, x >= 0x80, 7);
}

void ldy() {
	apu_wait += addcycle;
	ppu_wait += addcycle * 3;
	cpucc += addcycle;
	run_ppu(ppu_wait);
	memread();
	y = tmpval8;
	bitset(&flag, y == 0, 1);
	bitset(&flag, y >= 0x80, 7);
}

void lsr() {
	run_ppu(ppu_wait);
	bitset(&flag, *addval & 1, 0);
	tmpval8 = *addval >> 1;
	bitset(&flag, tmpval8 == 0, 1);
	bitset(&flag, tmpval8 >= 0x80, 7);
	memwrite();
}

void ora() {
	apu_wait += addcycle;
	ppu_wait += addcycle * 3;
	cpucc += addcycle;
	run_ppu(ppu_wait);
	memread();
	a |= tmpval8;
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void pha() {
	*cpuread(sp--) = a;
}

void php() {
	*cpuread(sp--) = (flag | 0x30); /* bit 4 is set if from an instruction */
}

void pla() {
	sp++;
	a = *cpuread(sp);
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void plp() {
	sp++;
	flag = *cpuread(sp);
	bitset(&flag, 1, 5);
	bitset(&flag, 0, 4); /* b flag should be discarded */
}

void rol() {
	run_ppu(ppu_wait);
	tmpval8 = *addval << 1;
	bitset(&tmpval8, flag & 1, 0);
	bitset(&flag, *addval & 0x80, 0);
	bitset(&flag, tmpval8 == 0, 1);
	bitset(&flag, tmpval8 >= 0x80, 7);
	memwrite();
}

void ror() {
	run_ppu(ppu_wait);
	tmpval8 = *addval >> 1;
	bitset(&tmpval8, flag & 1, 7);
	bitset(&flag, *addval & 1, 0);
	bitset(&flag, tmpval8 == 0, 1);
	bitset(&flag, tmpval8 >= 0x80, 7);
	memwrite();
}

void rti() {
	sp++;
	flag = *cpuread(sp++);
	bitset(&flag, 1, 5); /* bit 5 always set */
/*	bitset(&flag, 0, 4);  b flag should be discarded */
	pc = *cpuread(sp++);
	pc += (*cpuread(sp) << 8);
}

void rts() {
	sp++;
	addr = *cpuread(sp++);
	addr += *cpuread(sp) << 8;
	pc = addr + 1;
}

void sbc() {
	apu_wait += addcycle;
	ppu_wait += addcycle * 3;
	cpucc += addcycle;
	run_ppu(ppu_wait);
	memread();
	tmpval16 = a + (tmpval8 ^ 0xff) + (flag & 1);
	bitset(&flag, (a ^ tmpval16) & (tmpval8 ^ a) & 0x80, 6);
	bitset(&flag, tmpval16 > 0xff, 0);
	a = tmpval16;
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void sec() {
	bitset(&flag, 1, 0);
}

void sed() {
	bitset(&flag, 1, 3);
}

void sei() {
	bitset(&flag, 1, 2);
}

void sta() {
	tmpval8 = a;
	memwrite();
}

void stx() {
	tmpval8 = x;
	memwrite();
}

void sty() {
	tmpval8 = y;
	memwrite();
}

void tax() {
	x = a;
	bitset(&flag, x == 0, 1);
	bitset(&flag, x >= 0x80, 7);
}

void tay() {
	y = a;
	bitset(&flag, y == 0, 1);
	bitset(&flag, y >= 0x80, 7);
}

void tsx() {
	x = (sp & 0xff);
	bitset(&flag, x == 0, 1);
	bitset(&flag, x >= 0x80, 7);
}

void txa() {
	a = x;
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void txs() {
	sp = (0x100 | (uint16_t) x);
}

void tya() {
	a = y;
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void none() {
	printf("Warning: Illegal opcode!\n");
							}
void memread() {
	tmpval8 = *addval;
	switch (addr) {
	case 0x2002:
		if (ppucc == 342 || ppucc == 343) {/* suppress if read and set at same time */
			nmiIsTriggered = 0;
		} else if (ppucc == 341) {
			ppuStatus_nmiOccurred = 0;
			vblankSuppressed = 1;
		}
		tmpval8 = (ppuStatus_nmiOccurred<<7) | (spritezero<<6) | (spriteof<<5) | (ppureg & 0x1f);
		/*printf("Reads VBLANK flag at PPU cycle: %i\n",ppucc); */
		ppuStatus_nmiOccurred = 0;
	/* 	printf("VBLANK flag is clear PPU cycle %i\n",ppucc);*/
		w = 0;
		break;
	case 0x2004:
		/* TODO: read during rendering */
		tmpval8 = oam[oamaddr];
		break;
	case 0x2007:
		if (namev < 0x3f00) {
			tmpval8 = vbuff;
			vbuff = *ppuread(namev);
		}
		/* TODO: buffer update when reading palette */
		else if ((namev) >= 0x3f00) {
			tmpval8 = *ppuread(namev);
		}
		namev += vraminc;
		break;
	case 0x4015: /* APU status read */
		tmpval8 = (dmcInt ? 0x80 : 0) | (frameInt ? 0x40 : 0) | (dmcBytesLeft ? 0x10 : 0) | (noiseLength ? 0x08 : 0) | (triLength ? 0x04 : 0) | (pulse2Length ? 0x02 : 0) | (pulse1Length ? 0x01 : 0);
		/* TODO: timing related inhibition of frameInt clear */
		frameInt = 0;
		break;
	case 0x4016:
		tmpval8 = ((ctr1 >> ctrb) & 1);
		if (s == 0)
			ctrb++;
		break;
	case 0x4017:
		tmpval8 = ((ctr2 >> ctrb2) & 1);
		if (s == 0)
			ctrb2++;
		break;
	}
}

void memwrite() {
	switch (addr) {
	case 0x2000:
		ppuController = tmpval8;
		ppureg = ppuController;
		namet &= 0xf3ff;
		namet |= ((ppuController & 3)<<10);
		vraminc = ((ppuController >> 2) & 1) * 31 + 1;
		nmi_output = ((ppuController >> 7) & 1);
		if (nmi_output && ppuStatus_nmiOccurred) {
			nmiDelayed = 1;
			nmiIsTriggered = 1;
		}
		if (nmi_output == 0) {
			nmiAlreadyDone = 0;
		}
		break;
	case 0x2001:
		ppuMask = tmpval8;
		ppureg = ppuMask;
		break;
	case 0x2002:
		tmpval8 = *addval; /* prevent writing to */
		break;
	case 0x2003:
		ppureg = tmpval8;
		oamaddr = tmpval8;
		break;
	case 0x2004:
		ppureg = tmpval8;
		/* TODO: writing during rendering */
		if (vblank_period) {
			oam[oamaddr] = tmpval8;
			oamaddr++;
		}
		break;
	case 0x2005:
		ppureg = tmpval8;
		if (w == 0) {
			namet &= 0xffe0;
			namet |= ((tmpval8 & 0xf8)>>3); /* coarse X scroll */
			scrollx = (tmpval8 & 0x07); /* fine X scroll  */
			w = 1;
		} else if (w == 1) {
			namet &= 0x8c1f;
			namet |= ((tmpval8 & 0xf8)<<2); /* coarse Y scroll */
			namet |= ((tmpval8 & 0x07)<<12); /* fine Y scroll */
			w = 0;
		}
		break;
	case 0x2006:
		ppureg = tmpval8;
		if (w == 0) {
			namet &= 0x80ff;
			namet |= ((tmpval8 & 0x3f) << 8);
			w = 1;
		} else if (w == 1) {
			namet &= 0xff00;
			namet |= tmpval8;
			namev = namet;
			w = 0;
		}
		break;
	case 0x2007:
		ppuData = tmpval8;
		ppureg = ppuData;
		*ppuread(namev & 0x3fff) = ppuData;
		namev += vraminc;
		break;
	case 0x4000: /* Pulse 1 duty, envel., volume */
		pulse1Control = tmpval8;
		env1Divide = (pulse1Control&0xf);
		break;
	case 0x4001: /* Pulse 1 sweep, period, negate, shift */
		sweep1 = tmpval8;
		sweep1Divide = ((sweep1>>4)&7);
		sweep1Shift = (sweep1&7);
		sweep1Reload = 1;
		break;
	case 0x4002: /* Pulse 1 timer low */
			pulse1Timer = (pulse1Timer&0x700) | tmpval8;
		break;
	case 0x4003: /* Pulse 1 length counter, timer high */
		if (apuStatus & 1)
			pulse1Length = lengthTable[((tmpval8>>3)&0x1f)];
		pulse1Timer = (pulse1Timer&0xff) | ((tmpval8 & 7)<<8);
		env1Start = 1;
		pulse1Duty = 0;
		break;
	case 0x4004: /* Pulse 2 duty, envel., volume */
		pulse2Control = tmpval8;
		env2Divide = (pulse2Control&0xf);
		break;
	case 0x4005: /* Pulse 2 sweep, period, negate, shift */
		sweep2 = tmpval8;
		sweep2Divide = ((sweep2>>4)&7);
		sweep2Shift = (sweep2&7);
		sweep2Reload = 1;
		break;
	case 0x4006: /* Pulse 2 timer low */
			pulse2Timer = (pulse2Timer&0x700) | tmpval8;
		break;
	case 0x4007: /* Pulse 2 length counter, timer high */
		if (apuStatus & 2)
			pulse2Length = lengthTable[((tmpval8>>3)&0x1f)];
		pulse2Timer = (pulse2Timer&0xff) | ((tmpval8 & 7)<<8);
		env2Start = 1;
		pulse2Duty = 0;
		break;
	case 0x4008: /* Triangle misc. */
		if (apuStatus & 4) {
			triControl = tmpval8;
		}
		break;
	case 0x400a: /* Triangle timer low */
		if (apuStatus & 4) {
			triTimer = (triTimer&0x700) | tmpval8;
			triTemp = triTimer;
		}
		break;
	case 0x400b: /* Triangle length, timer high */
		if (apuStatus & 4) {
			triLength = lengthTable[((tmpval8>>3)&0x1f)];
			triTimer = (triTimer&0xff) | ((tmpval8 & 7)<<8);
			triTemp = triTimer;
			triLinReload = 1;
		}
		break;
	case 0x400c: /* Noise misc. */
		noiseControl = tmpval8;
		envNoiseDivide = (noiseControl&0xf);
		break;
	case 0x400e: /* Noise loop, period */
		noiseTimer = noiseTable[(tmpval8&0xf)];
		noiseMode = (tmpval8&0x80);
		break;
	case 0x400f: /* Noise length counter */
		if (apuStatus & 8) {
			noiseLength = lengthTable[((tmpval8>>3)&0x1f)];
			envNoiseStart = 1;
		}
		break;
	case 0x4010: /* DMC IRQ, loop, freq. */
		dmcControl = tmpval8;
		dmcRate = rateTable[(dmcControl&0xf)];
		dmcTemp = dmcRate;
		if (!(dmcControl&0x80))
			dmcInt = 0;
		break;
	case 0x4011: /* DMC load counter */
		dmcOutput = (tmpval8&0x7f);
		break;
	case 0x4012: /* DMC sample address */
		dmcAddress = (0xc000 + (64 * tmpval8));
		dmcCurAdd = dmcAddress;
		break;
	case 0x4013: /* DMC sample length */
		dmcLength = ((16 * tmpval8) + 1);
		dmcBytesLeft = dmcLength;
		break;
	case 0x4014:
		ppureg = tmpval8;
		tmpval16 = ((tmpval8 << 8) & 0xff00);
		if (cpucc%2) {
			apu_wait += 2;
			ppu_wait += 6;
			cpucc += 2;
		} else {
			apu_wait += 1;
			ppu_wait += 3;
			cpucc += 1;
		}
		run_ppu(ppu_wait);
		run_apu(apu_wait);
		for (int i = 0; i < 256; i++) {
			if (oamaddr > 255)
				oamaddr = 0;
			oam[oamaddr] = *cpuread(tmpval16++);
			oamaddr++;
			apu_wait += 2;
			ppu_wait += 6;
			cpucc += 2;
			run_ppu(ppu_wait);
			run_apu(apu_wait);
		}
	/*	apu_wait += 513;
		ppu_wait += 513 * 3;
		cpucc += 513; */
		break;
	case 0x4015: /* APU status */
		dmcInt = 0;
		apuStatus = tmpval8;
		if (!(apuStatus&0x01))
			pulse1Length = 0;
		if (!(apuStatus&0x02))
			pulse2Length = 0;
		if (!(apuStatus&0x04))
			triLength = 0;
		if (!(apuStatus&0x08))
			noiseLength = 0;
		if (!(apuStatus&0x10))
			dmcBytesLeft = 0;
		else if (apuStatus&0x10)
			dmc_fill_buffer();
		break;
	case 0x4016:
		ppureg = tmpval8;
		s = (tmpval8 & 1);
		if (s == 1) {
			ctrb = 0;
			ctrb2 = 0;
		}
		break;
	case 0x4017: /* APU frame counter */
		apuFrameCounter = tmpval8;
		frameCounter = 0; /* TODO: correct behavior */
		apucc = 0;
		if (apuFrameCounter&0x40)
			frameInt = 0;
		if (apuFrameCounter&0x80) {
			quarter_frame();
			half_frame();
		}
		break;
	}

	if (addr >= 0x8000) {
	if ((!strcmp(cart.slot,"sxrom") ||
			!strcmp(cart.slot,"sxrom_a") ||
				!strcmp(cart.slot,"sorom") ||
					!strcmp(cart.slot,"sorom_a"))) {
		mapper_mmc1(addr, tmpval8);
	}
	else if (!strcmp(cart.slot,"uxrom") ||
				!strcmp(cart.slot,"un1rom") ||
					!strcmp(cart.slot,"unrom_cc")) {
		mapper_uxrom(tmpval8);
	}
	else if (!strcmp(cart.slot,"cnrom")) {
		mapper_cnrom(tmpval8);
	}
	else if (!strcmp(cart.slot,"txrom")) {
		mapper_mmc3(addr,tmpval8);
	}
	else if (!strcmp(cart.slot,"axrom")) {
		mapper_axrom(tmpval8);
	}
	else if (!strcmp(cart.slot,"vrc2") ||
				!strcmp(cart.slot,"vrc4")) {
		mapper_vrc24(addr,tmpval8);
	}
	}

	if (addr < 0x8000)
		*addval = tmpval8;
}

uint8_t * cpuread(uint16_t address) {
	if (address >= 0x8000)
		return &prgSlot[(address>>12)&~8][address&0xfff];
	else if (address >= 0x6000 && address < 0x8000) {
		if (wramEnable)
			return &cpu[address];
		else {
			openBus = (address>>4);
			return &openBus;
		}
	}
		return &cpu[address];
}

uint8_t * ppuread(uint16_t address) {
	if (address < 0x2000) /* pattern tables */
		return &chrSlot[(address>>10)][address&0x3ff];
	else if (address >= 0x2000 && address <0x3f00) /* nametables */
		return mirroring[cart.mirroring][((namev&0xc00)>>10)] ? &nameTableB[address&0x3ff] : &nameTableA[address&0x3ff];
	else if (address >= 0x3f00) { /* palette RAM */
		if (address == 0x3f10)
			address = 0x3f00;
		else if (address == 0x3f14)
			address = 0x3f04;
		else if (address == 0x3f18)
			address = 0x3f08;
		else if (address == 0x3f1c)
			address = 0x3f0c;
		return &vram[(address&0x3f1f)];
	}
}

void set_irq() {
	if (!(flag&4)) {
		*cpuread(sp--) = ((pc) & 0xff00) >> 8;
		*cpuread(sp--) = ((pc) & 0xff);
		*cpuread(sp--) = flag;
		pc = (*cpuread(irq + 1) << 8) + *cpuread(irq);
		bitset(&flag, 1, 2); /* set I flag */
	}
}
