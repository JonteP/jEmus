#include "6502.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>	/* memcpy */
#include "globals.h"
#include "nestools.h"
#include "ppu.h"

extern void opdecode(uint8_t);

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

static inline void mirror(), memread(), memwrite();;
static inline void accum(), immed(), zpage(), zpagex(), zpagey(), absol(),
		absx(), absy(), indx(), indy();
static inline void adc(), and(), asl(), branch(), bit(), brkop(), clc(), cld(),
		cli(), clv(), cmp(), cpx(), cpy(), dec(), dex(), dey(), eor(), inc(),
		inx(), iny(), jmpa(), jmpi(), jsr(), lda(), ldx(), ldy(), lsr(), ora(),
		pha(), php(), pla(), plp(), rol(), ror(), rti(), rts(), sbc(), sec(),
		sed(), sei(), sta(), stx(), sty(), tax(), tay(), tsx(), txa(), txs(),
		tya(), none();

uint8_t mm1_shift = 0, mm1_buff, wram, prg_bank, chr_bank, prg_size, chr_size; /* mapper 1 */
uint8_t mode, opcode, addmode, addcycle, *addval, tmpval8, vbuff = 0, s = 0, vraminc, ppureg = 0, w = 0;
uint16_t addr, tmpval16;

int test = 0;

void opdecode(uint8_t op) {
/*	printf("Opcode: %02X at PC: %04X\n",op,pc); */
	test++;
	addcycle = 0;
	addr = 0;
	if (pc < 0x8000)
		printf("PC out of bounds!\n");
	static void (*addp0[8])() = {immed,zpage,accum,absol,absy,zpagex,zpagey,absx};
	static void (*addp1[8])() = {indx,zpage,immed,absol,indy,zpagex,absy,absx};
	static void (*opp0[8])() = {none,bit,none,none,sty,ldy,cpy,cpx};
	static void (*opp0ex2[8])() = {php,plp,pha,pla,dey,tay,iny,inx};
	static void (*opp0ex6[8])() = {clc,sec,cli,sei,tya,clv,cld,sed};
	static void (*opp1[8])() = {ora,and,eor,adc,sta,lda,cmp,sbc};
	static void (*opp2[8])() = {asl,rol,lsr,ror,stx,ldx,dec,inc};
	cpu_wait += (ctable[op] * 3);
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
	addval = &cpu[addr];
}

void zpage() {
	addr = cpu[pc++];
	addval = &cpu[addr];
}

void zpagex() {
	addr = cpu[pc++];
	addr = ((addr + x) & 0xff);
	addval = &cpu[addr];
}

void zpagey() {
	addr = cpu[pc++];
	addr = ((addr + y) & 0xff);
	addval = &cpu[addr];
}

void absol() {
	addr = cpu[pc++];
	addr += cpu[pc++] << 8;
	mirror();
	addval = &cpu[addr];
}

void absx() {
	addr = cpu[pc++];
	addr += cpu[pc++] << 8;
	addr += x;
	if ((addr & 0xff) < x) {
		addcycle = 1;
	}
	mirror();
	addval = &cpu[addr];
}

void absy() {
	addr = cpu[pc++];
	addr += cpu[pc++] << 8;
	addr += y;
	if ((addr & 0xff) < y) {
		addcycle = 1;
	}
	mirror();
	addval = &cpu[addr];
}

void indx() {
	tmpval8 = cpu[pc++];
	addr = cpu[((tmpval8+x) & 0xff)];
	addr += cpu[((tmpval8+x+1) & 0xff)] << 8;
	mirror();
	addval = &cpu[addr];
}

void indy() {
	tmpval8 = cpu[pc++];
	addr = cpu[tmpval8++];
	addr += cpu[(tmpval8 & 0xff)] << 8;
	addr = ((addr + y) & 0xffff);
	if ((addr & 0xff) < y) {
		addcycle = 1;
	}
	mirror();
	addval = &cpu[addr];
}

		/* OPCODES */
void adc() {
	cpu_wait += addcycle*3;
	cpucc += addcycle;
	run_ppu(cpu_wait);
	memread();
	tmpval16 = a + tmpval8 + (flag & 1);
	bitset(&flag, (a ^ tmpval16) & (tmpval8 ^ tmpval16) & 0x80, 6);
	bitset(&flag, tmpval16 > 0xff, 0);
	a = tmpval16;
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void and() {
	cpu_wait += addcycle*3;
	cpucc += addcycle;
	run_ppu(cpu_wait);
	memread();
	a &= tmpval8;
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void asl() {
	run_ppu(cpu_wait);
	bitset(&flag, *addval & 0x80, 0);
	tmpval8 = *addval << 1;
	bitset(&flag, tmpval8 == 0, 1);
	bitset(&flag, tmpval8 >= 0x80, 7);
	memwrite();
}

void bit() {
	run_ppu(cpu_wait);
	memread();
	bitset(&flag, !(a & tmpval8), 1);
	bitset(&flag, tmpval8 & 0x80, 7);
	bitset(&flag, tmpval8 & 0x40, 6);
}

void branch() {
	uint8_t reflag[4] = { 7, 6, 0, 1 };
	if (((flag >> reflag[(opcode >> 1) & 3]) & 1) == (opcode & 1)) {
		if (((pc + 1) & 0xff00)	!= ((pc + ((int8_t) cpu[pc] + 1)) & 0xff00)) {
			cpu_wait += 3;
			cpucc += 1;
		}
		pc = pc + (int8_t) cpu[pc] + 1;
		cpu_wait += 3;
		cpucc += 1;
	} else
		pc++;
}

/* TODO */
void brkop() { /* does not care about the I flag */
	pc++;
	isnmi = 0;
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
	cpu_wait += addcycle*3;
	cpucc += addcycle;
	run_ppu(cpu_wait);
	memread();
	bitset(&flag, (a - tmpval8) & 0x80, 7);
	bitset(&flag, a == tmpval8, 1);
	bitset(&flag, a >= tmpval8, 0);
}

void cpx() {
	run_ppu(cpu_wait);
	memread();
	bitset(&flag, (x - tmpval8) & 0x80, 7);
	bitset(&flag, x == tmpval8, 1);
	bitset(&flag, x >= tmpval8, 0);
}

void cpy() {
	run_ppu(cpu_wait);
	memread();
	bitset(&flag, (y - tmpval8) & 0x80, 7);
	bitset(&flag, y == tmpval8, 1);
	bitset(&flag, y >= tmpval8, 0);
}

void dec() {
	run_ppu(cpu_wait);
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
	cpu_wait += addcycle*3;
	cpucc += addcycle;
	run_ppu(cpu_wait);
	memread();
	a ^= tmpval8;
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void inc() {
	run_ppu(cpu_wait);
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
	addr = cpu[pc++];
	addr += cpu[pc++] << 8;
	pc = addr;
}

void jmpi() {
	tmpval8 = cpu[pc++];
	tmpval16 = (cpu[pc] << 8);
	addr = cpu[tmpval16 | tmpval8];
	addr += cpu[tmpval16 | ((tmpval8+1) & 0xff)] << 8;
	pc = addr;
}

void jsr() {
	cpu[sp--] = ((pc + 1) & 0xff00) >> 8;
	cpu[sp--] = ((pc + 1) & 0x00ff);
	addr = cpu[pc++];
	addr += cpu[pc] << 8;
	pc = addr;
}

void lda() {
	cpu_wait += addcycle*3;
	cpucc += addcycle;
	run_ppu(cpu_wait);
	memread();
	a = tmpval8;
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void ldx() {
	cpu_wait += addcycle*3;
	cpucc += addcycle;
	run_ppu(cpu_wait);
	memread();
	x = tmpval8;
	bitset(&flag, x == 0, 1);
	bitset(&flag, x >= 0x80, 7);
}

void ldy() {
	cpu_wait += addcycle*3;
	cpucc += addcycle;
	run_ppu(cpu_wait);
	memread();
	y = tmpval8;
	bitset(&flag, y == 0, 1);
	bitset(&flag, y >= 0x80, 7);
}

void lsr() {
	run_ppu(cpu_wait);
	bitset(&flag, *addval & 1, 0);
	tmpval8 = *addval >> 1;
	bitset(&flag, tmpval8 == 0, 1);
	bitset(&flag, tmpval8 >= 0x80, 7);
	memwrite();
}

void ora() {
	cpu_wait += addcycle*3;
	cpucc += addcycle;
	run_ppu(cpu_wait);
	memread();
	a |= tmpval8;
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void pha() {
	cpu[sp--] = a;
}

void php() {
	cpu[sp--] = (flag | 0x30); /* bit 4 is set if from an instruction */
}

void pla() {
	sp++;
	a = cpu[sp];
	bitset(&flag, a == 0, 1);
	bitset(&flag, a >= 0x80, 7);
}

void plp() {
	sp++;
	flag = cpu[sp];
	bitset(&flag, 1, 5);
	bitset(&flag, 0, 4); /* b flag should be discarded */
}

void rol() {
	run_ppu(cpu_wait);
	tmpval8 = *addval << 1;
	bitset(&tmpval8, flag & 1, 0);
	bitset(&flag, *addval & 0x80, 0);
	bitset(&flag, tmpval8 == 0, 1);
	bitset(&flag, tmpval8 >= 0x80, 7);
	memwrite();
}

void ror() {
	run_ppu(cpu_wait);
	tmpval8 = *addval >> 1;
	bitset(&tmpval8, flag & 1, 7);
	bitset(&flag, *addval & 1, 0);
	bitset(&flag, tmpval8 == 0, 1);
	bitset(&flag, tmpval8 >= 0x80, 7);
	memwrite();
}

void rti() {
	sp++;
	flag = cpu[sp++];
	bitset(&flag, 1, 5); /* bit 5 always set */
/*	bitset(&flag, 0, 4);  b flag should be discarded */
	pc = cpu[sp++];
	pc += (cpu[sp] << 8);
}

void rts() {
	sp++;
	addr = cpu[sp++];
	addr += cpu[sp] << 8;
	pc = addr + 1;
}

void sbc() {
	cpu_wait += addcycle*3;
	cpucc += addcycle;
	run_ppu(cpu_wait);
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
		if (ppucc == 342) /* suppress if read and set at same time (+1 cycle for cpu timing??) */
			isvblank = 0;
		tmpval8 = (isvblank<<7) | (spritezero<<6) | (spriteof<<5) | (ppureg & 0x1f);
	/*	printf("Reads VBLANK flag at PPU cycle: %i\n",ppucc); */
		isvblank = 0;
	/* 	printf("VBLANK flag is clear PPU cycle %i\n",ppucc);*/
		w = 0;
		break;
	case 0x2004:
		/* TODO: read during rendering */
		tmpval8 = oam[oamaddr];
		break;
	case 0x2007:
		if ((namev) < 0x3f00) {
			if ((namev) >= 0x2000)
				mirrmode ? (namev = (namev & 0xf7ff)) :	(namev = (namev & 0xfbff));
			tmpval8 = vbuff;
			vbuff = vram[namev];
		}
		/* TODO: buffer update when reading palette */
		else if ((namev) >= 0x3f00) {
			namev = (namev & 0xff1f);
			if (namev==0x3f10)
				namev=0x3f00;
			tmpval8 = vram[namev];
		}
		namev += vraminc;
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
	*addval = tmpval8;
	switch (addr) {
	case 0x2000:
		ppureg = tmpval8;
		namet &= 0xf3ff;
		namet |= ((tmpval8 & 3)<<10);
		bpattern = &vram[((tmpval8 >> 4) & 1)	* 0x1000];
		if (!(tmpval8 & 0x20))
			spattern = &vram[((tmpval8 >> 3) & 1)	* 0x1000];
		else
			spattern = &vram[0];
		vraminc = ((tmpval8 >> 2) & 1) * 31 + 1;
		nmi_output = ((tmpval8>>7)&1);
		if (nmi_output && isvblank)
			vblank_wait = 1;
		if (nmi_output == 0) {
			nmi_allow = 1;
		}
		break;
	case 0x2001:
		ppureg = tmpval8;
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
		ppureg = tmpval8;
		if (namev >= 0x2000) {
			if ((namev) < 0x3f00)
				mirrmode ? (namev = (namev & 0xf7ff)) :	(namev = (namev & 0xfbff));
			else if ((namev) >= 0x3f00) {
			 	namev = (namev & 0x3f1f);
			}
			if (namev==0x3f10) /* TODO: complete mirroring behavior */
				vram[0x3f00] = tmpval8;
			else
				vram[namev] = tmpval8;
		}
				namev += vraminc;
		break;
	case 0x4014:
		ppureg = tmpval8;
		tmpval16 = ((tmpval8 << 8) & 0xff00);
		for (int i = 0; i < 256; i++) {
			if (oamaddr > 255)
				oamaddr = 0;
			oam[oamaddr] = cpu[tmpval16++];
			oamaddr++;
		}
		if (cpucc%2) {
			cpu_wait += 3;
			cpucc += 1;
		}
		cpu_wait += 513 * 3;
		cpucc += 513;
		break;
	case 0x4016:
		ppureg = tmpval8;
		s = (tmpval8 & 1);
		if (s == 1) {
			ctrb = 0;
			ctrb2 = 0;
		}
		break;
	}
	if (addr >= 0x8000 && mapper == 1) {
		if (tmpval8 & 0x80) {
			mm1_shift = 0;
			mm1_buff = 0;
		} else {
			mm1_buff = (mm1_buff & ~(1<<mm1_shift)) | ((tmpval8 & 1)<<mm1_shift);
			if (mm1_shift == 4) {
				switch ((addr>>13) & 3) {
				case 0: /* Control register */
					mirrmode = (mm1_buff & 1); /* TODO: implement extra modes */
					prg_bank = ((mm1_buff >> 2) & 1); /* 0 low, 1 high */
					prg_size = ((mm1_buff >> 3) & 1); /* 0 32k, 1 16k */
					chr_size = ((mm1_buff >> 4) & 1); /* 0 8k,  1 4k */
					break;
				case 1: /* CHR ROM low bank */
					if (!chr_size)
						memcpy(&vram[0], &chr[(mm1_buff & 1) * 0x2000], 0x2000);
					break;
				case 2: /* CHR ROM high bank (4k mode) */
					break;
				case 3: /* PRG ROM bank */
					wram = ((mm1_buff >> 4) & 1);
					if ((mm1_buff & 0xf) <= (psize>>14))
						memcpy(&cpu[0x8000], &prg[(mm1_buff & 0xf) * 0x4000], 0x4000);
					break;
			}
				mm1_shift = 0;
				mm1_buff = 0;
			} else
				mm1_shift++;
		}
	}
}
