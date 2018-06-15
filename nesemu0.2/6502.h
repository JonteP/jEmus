#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "globals.h"

						 /* 0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f */
static uint8_t ctable[] = { 7, 6, 0, 0, 0, 3, 5, 0, 3, 2, 2, 0, 0, 4, 6, 0,/* 0 */
							2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,/* 1 */
							6, 6, 0, 0, 3, 3, 5, 0, 4, 2, 2, 0, 4, 4, 6, 0,/* 2 */
							2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,/* 3 */
							6, 6, 0, 0, 5, 3, 5, 0, 3, 2, 2, 0, 3, 4, 6, 0,/* 4 */
							2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,/* 5 */
							6, 6, 0, 0, 0, 3, 5, 0, 4, 2, 2, 0, 5, 4, 6, 0,/* 6 */
							2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,/* 7 */
							0, 6, 0, 0, 3, 3, 3, 0, 2, 0, 2, 0, 4, 4, 4, 0,/* 8 */
							2, 6, 0, 0, 4, 4, 4, 0, 2, 5, 2, 0, 0, 5, 0, 0,/* 9 */
							2, 6, 2, 0, 3, 3, 3, 0, 2, 2, 2, 0, 4, 4, 4, 0,/* a */
							2, 5, 0, 0, 4, 4, 4, 0, 2, 4, 2, 0, 4, 4, 4, 0,/* b */
							2, 6, 0, 0, 3, 3, 5, 0, 2, 2, 2, 0, 4, 4, 6, 0,/* c */
							2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,/* d */
							2, 6, 0, 0, 3, 3, 5, 0, 2, 2, 2, 0, 4, 4, 6, 0,/* e */
							2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0 /* f */
							};

static inline void opdecode(uint8_t);
static inline void mirror(), memread(), memwrite();
static inline void accum(), immed(), zpage(), zpagex(), zpagey(), absol(),
		absx(), absy(), indx(), indy();
static inline void adc(), and(), asl(), branch(), bit(), brkop(), clc(), cld(),
		cli(), clv(), cmp(), cpx(), cpy(), dec(), dex(), dey(), eor(), inc(),
		inx(), iny(), jmpa(), jmpi(), jsr(), lda(), ldx(), ldy(), lsr(), ora(),
		pha(), php(), pla(), plp(), rol(), ror(), rti(), rts(), sbc(), sec(),
		sed(), sei(), sta(), stx(), sty(), tax(), tay(), tsx(), txa(), txs(),
		tya(), none();

uint8_t mode, opcode, addmode, addcycle;

int test = 0;

void opdecode(uint8_t op) {
	test++;
	addcycle = 0;
	sp_cnt = 0;
	addr = 0;
	rw = 0;
	dest = &vval; /* dummy pointer */
	pcbuff = pc;
	sp_add = sp;
	flagbuff = flag;
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
		} else if (opcode == 1 && addmode == 0) {
			jsr();
			break;
		} else if (opcode == 2 && addmode == 0) {
			rti();
			break;
		} else if (opcode == 2 && addmode == 3) {
			jmpa();
			break;
		} else if (opcode == 3 && addmode == 3) {
			jmpi();
			break;
		} else if (opcode == 3 && addmode == 0) {
			rts();
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
		(*addp1[addmode])();
		(*opp1[opcode])();
		break;
	case 2:
		if (opcode == 4 && addmode == 2) {
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
		} else if (opcode == 6 && addmode == 2) {
			dex();
			break;
		} else if (opcode == 7 && addmode == 2)
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
	vval = cpu[pc++];
	addr = cpu[((vval+x) & 0xff)];
	addr += cpu[((vval+x+1) & 0xff)] << 8;
	mirror();
	addval = &cpu[addr];
}

void indy() {
	vval = cpu[pc++];
	addr = cpu[vval++];
	addr += cpu[(vval & 0xff)] << 8;
	addr = ((addr + y) & 0xffff);
	if ((addr & 0xff) < y) {
		addcycle = 1;
	}
	mirror();
	addval = &cpu[addr];
}

		/* OPCODES */
void adc() {
	rw = 1;
	dest = &a;
	tmp = a + *addval + (flag & 1);
	bitset(&flagbuff, (a ^ tmp) & (*addval ^ tmp) & 0x80, 6);
	bitset(&flagbuff, tmp > 0xff, 0);
	vval = tmp;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
	cpu_wait += addcycle*3;
	cpucc += addcycle;
}

void and() {
	rw = 1;
	dest = &a;
	vval = (a & *addval);
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
	cpu_wait += addcycle*3;
	cpucc += addcycle;
}

void asl() {
	rw = 2;
	dest = addval;
	bitset(&flagbuff, *addval & 0x80, 0);
	vval = *addval << 1;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void bit() {
	rw = 1;
	bitset(&flagbuff, !(a & *addval), 1);
	bitset(&flagbuff, *addval & 0x80, 7);
	bitset(&flagbuff, *addval & 0x40, 6);
}

void branch() {
	uint8_t reflag[4] = { 7, 6, 0, 1 };
	if (((flag >> reflag[(opcode >> 1) & 3]) & 1) == (opcode & 1)) {
		if (((pc + 1) & 0xff00)	!= ((pc + ((int8_t) cpu[pc] + 1)) & 0xff00)) {
			cpu_wait += 3;
			cpucc += 1;
		}
		pcbuff = pc + (int8_t) cpu[pc] + 1;
		cpu_wait += 3;
		cpucc += 1;
	}
	pc++;
}

/* TODO */
void brkop() { /* does not care about the I flag */
	pc++;
	isnmi = 0;
	donmi();
}

void clc() {
	bitset(&flagbuff, 0, 0);
}

void cld() {
	bitset(&flagbuff, 0, 3);
}

void cli() {
	bitset(&flagbuff, 0, 2);
}

void clv() {
	bitset(&flagbuff, 0, 6);
}

void cmp() {
	rw = 1;
	bitset(&flagbuff, (a - *addval) & 0x80, 7);
	bitset(&flagbuff, a == *addval, 1);
	bitset(&flagbuff, a >= *addval, 0);
	cpu_wait += addcycle*3;
	cpucc += addcycle;
}

void cpx() {
	rw = 1;
	bitset(&flagbuff, (x - *addval) & 0x80, 7);
	bitset(&flagbuff, x == *addval, 1);
	bitset(&flagbuff, x >= *addval, 0);
}

void cpy() {
	rw = 1;
	bitset(&flagbuff, (y - *addval) & 0x80, 7);
	bitset(&flagbuff, y == *addval, 1);
	bitset(&flagbuff, y >= *addval, 0);
}

void dec() {
	rw = 2;
	dest = addval;
	vval = *addval-1;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void dex() {
	dest = &x;
	vval = x - 1;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void dey() {
	dest = &y;
	vval = y - 1;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void eor() {
	rw = 1;
	dest = &a;
	vval = a ^ *addval;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
	cpu_wait += addcycle*3;
	cpucc += addcycle;
}

void inc() {
	rw = 2;
	dest = addval;
	vval = *addval + 1;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void inx() {
	dest = &x;
	vval = x + 1;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void iny() {
	dest = &y;
	vval = y + 1;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void jmpa() {
	addr = cpu[pc++];
	addr += cpu[pc++] << 8;
	pcbuff = addr;
}

void jmpi() {
	vval = cpu[pc++];
	tmp = (cpu[pc] << 8);
	addr = cpu[tmp | vval];
	addr += cpu[tmp | ((vval+1) & 0xff)] << 8;
	pcbuff = addr;
}

void jsr() {
	sp_cnt = 2;
	sp_buff[0] = ((pc + 1) & 0xff00) >> 8;
	sp_buff[1] = ((pc + 1) & 0x00ff);
	addr = cpu[pc++];
	addr += cpu[pc++] << 8;
	pcbuff = addr;
}

void lda() {
	rw = 1;
	dest = &a;
	vval = *addval;
	bitset(&flagbuff, a == 0, 1);
	bitset(&flagbuff, a >= 0x80, 7);
	cpu_wait += addcycle*3;
	cpucc += addcycle;
}

void ldx() {
	rw = 1;
	dest = &x;
	vval = *addval;
	bitset(&flagbuff, x == 0, 1);
	bitset(&flagbuff, x >= 0x80, 7);
	cpu_wait += addcycle*3;
	cpucc += addcycle;
}

void ldy() {
	rw = 1;
	dest = &y;
	vval = *addval;
	bitset(&flagbuff, y == 0, 1);
	bitset(&flagbuff, y >= 0x80, 7);
	cpu_wait += addcycle*3;
	cpucc += addcycle;
}

void lsr() {
	rw = 2;
	dest = addval;
	bitset(&flagbuff, *addval & 1, 0);
	vval = *addval >> 1;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void ora() {
	rw = 1;
	dest = &a;
	vval = a | *addval;
	bitset(&flagbuff, a == 0, 1);
	bitset(&flagbuff, a >= 0x80, 7);
	cpu_wait += addcycle*3;
	cpucc += addcycle;
}

void pha() {
	sp_cnt = 1;
	sp_buff[0] = a;
}

void php() {
	sp_cnt = 1;
	sp_buff[0] = (flag | 0x30); /* bit 4 is set if from an instruction */
}

void pla() {
	dest = &a;
	sp_cnt = -1;
	sp_buff[0] = cpu[sp];
	bitset(&flagbuff, sp_buff[0] == 0, 1);
	bitset(&flagbuff, sp_buff[0] >= 0x80, 7);
}

void plp() {
	dest = &flag;
	sp_cnt = -1;
	sp_buff[0] = cpu[sp];
	bitset(&sp_buff[0], 1, 5);
	bitset(&sp_buff[0],0,4); /* b flag should be discarded */
}

void rol() {
	rw = 2;
	dest = addval;
	vval = *addval << 1;
	bitset(&vval, flag & 1, 0);
	bitset(&flagbuff, *addval & 0x80, 0);
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void ror() {
	rw = 2;
	dest = addval;
	vval = *addval >> 1;
	bitset(&vval, flag & 1, 7);
	bitset(&flagbuff, *addval & 1, 0);
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void rti() {
	sp_cnt = -3;
	flagbuff = cpu[sp];
	bitset(&flagbuff, 1, 5); /* bit 5 always set */
/*	bitset(&flagbuff, 0, 4);  b flag should be discarded */
	pcbuff = cpu[sp+1];
	pcbuff += (cpu[sp+2] << 8);
}

void rts() {
	sp_cnt = -2;
	addr = cpu[sp];
	addr += cpu[sp+1] << 8;
	pcbuff = addr + 1;
}

void sbc() {
	rw = 1;
	dest = &a;
	tmp = a + (*addval ^ 0xff) + (flag & 1);
	bitset(&flagbuff, (a ^ tmp) & (*addval ^ a) & 0x80, 6);
	bitset(&flagbuff, tmp > 0xff, 0);
	vval = tmp;
	bitset(&flagbuff, a == 0, 1);
	bitset(&flagbuff, a >= 0x80, 7);
	cpu_wait += addcycle*3;
	cpucc += addcycle;
}

void sec() {
	bitset(&flagbuff, 1, 0);
}

void sed() {
	bitset(&flagbuff, 1, 3);
}

void sei() {
	bitset(&flagbuff, 1, 2);
}

void sta() {
	rw = 2;
	dest = addval;
	vval = a;
}

void stx() {
	rw = 2;
	dest = addval;
	vval = x;
}

void sty() {
	rw = 2;
	dest = addval;
	vval = y;
}

void tax() {
	dest = &x;
	vval = a;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void tay() {
	dest = &y;
	vval = a;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void tsx() {
	dest = &x;
	vval = (sp & 0xff);
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void txa() {
	dest = &a;
	vval = x;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void txs() {
	sp_add = (0x100 | (uint16_t) x);
}

void tya() {
	dest = &a;
	vval = y;
	bitset(&flagbuff, vval == 0, 1);
	bitset(&flagbuff, vval >= 0x80, 7);
}

void none() {
	printf("Warning: Illegal opcode!\n");
							}

void memread() {
	vval = *addval;
	switch (addr) {
	case 0x2002:
	/*	if (scanline == 240 && (ppudot + cpu_wait) == 343)
			isvblank = 0; */
		vval = (isvblank<<7) | (spritezero<<6) | (spriteof<<5) | (ppureg & 0x1f);
	/*	printf("Reads VBLANK flag at PPU cycle: %i\n",ppucc); */
		isvblank = 0;
		w = 0;
		break;
	case 0x2004:
		/* TODO: read during rendering */
		vval = oam[oamaddr];
		break;
	case 0x2007:
		if ((namev) < 0x3f00) {
			if ((namev) >= 0x2000)
				mirrmode ? (namev = (namev & 0xf7ff)) :	(namev = (namev & 0xfbff));
			vval = vbuff;
			vbuff = vram[namev];
		}
		/* TODO: buffer update when reading palette */
		else if ((namev) >= 0x3f00) {
			namev = (namev & 0xff1f);
			if (namev==0x3f10)
				namev=0x3f00;
			vval = vram[namev];
		}
		namev += vraminc;
		break;
	case 0x4016:
		vval = ((ctr1 >> ctrb) & 1);
		if (s == 0)
			ctrb++;
		break;
	case 0x4017:
		vval = ((ctr2 >> ctrb2) & 1);
		if (s == 0)
			ctrb2++;
		break;
	}
}

void memwrite() {
	switch (addr) {
	case 0x2000:
		ppureg = vval;
		namet &= 0xf3ff;
		namet |= ((vval & 3)<<10);
		bpattern = &vram[((vval >> 4) & 1)	* 0x1000];
		if (!(vval & 0x20))
			spattern = &vram[((vval >> 3) & 1)	* 0x1000];
		else
			spattern = &vram[0];
		vraminc = ((vval >> 2) & 1) * 31 + 1;
		nmi_output = ((vval>>7)&1);
		if (nmi_output)
			printf("NMI_output set to: %i at PPU cycle: %i\n",nmi_output,ppucc);
		if (nmi_output == 0)
			nmiset = 1;
		if (nmi_output && isvblank)
			vblank_wait = 1;
		break;
	case 0x2001:
		ppureg = vval;
		break;
	case 0x2002:
		vval = *addval; /* prevent writing to */
		break;
	case 0x2003:
		ppureg = vval;
		oamaddr = vval;
		break;
	case 0x2004:
		ppureg = vval;
		/* TODO: writing during rendering */
		if (vblank_period) {
			oam[oamaddr] = vval;
			oamaddr++;
		}
		break;
	case 0x2005:
		ppureg = vval;
		if (w == 0) {
			namet &= 0xffe0;
			namet |= ((vval & 0xf8)>>3); /* coarse X scroll */
			scrollx = (vval & 0x07); /* fine X scroll  */
		/*	printf("Scroll X is %02X\n",scrollx); */
			w = 1;
		} else if (w == 1) {
			namet &= 0x8c1f;
			namet |= ((vval & 0xf8)<<2); /* coarse Y scroll */
			namet |= ((vval & 0x07)<<12); /* fine Y scroll */
			w = 0;
		}
		break;
	case 0x2006:
		ppureg = vval;
		if (w == 0) {
			namet &= 0x80ff;
			namet |= ((vval & 0x3f) << 8);
			w = 1;
		} else if (w == 1) {
			namet &= 0xff00;
			namet |= vval;
			namev = namet;
			w = 0;
		}
		break;
	case 0x2007:
		ppureg = vval;
		if (namev >= 0x2000) {
			if ((namev) < 0x3f00)
				mirrmode ? (namev = (namev & 0xf7ff)) :	(namev = (namev & 0xfbff));
			else if ((namev) >= 0x3f00) {
			 	namev = (namev & 0x3f1f);
			}
			if (namev==0x3f10) /* TODO: complete mirroring behavior */
				vram[0x3f00] = vval;
			else
				vram[namev] = vval;
		}
	/*	printf("Wrote %02X to VRAM %04X at scanline %x when render is %x\n", cpu[addr],namev,scanline,(cpu[0x2001] & 0x18)); */
				namev += vraminc;
		break;
	case 0x4014:
		ppureg = vval;
		tmp = ((vval << 8) & 0xff00);
		for (int i = 0; i < 256; i++) {
			if (oamaddr > 255)
				oamaddr = 0;
			oam[oamaddr] = cpu[tmp++];
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
		ppureg = vval;
		s = (vval & 1);
		if (s == 1) {
			ctrb = 0;
			ctrb2 = 0;
		}
		break;
	}
	if (addr >= 0x8000 && mapper == 1) {
		if (vval & 0x80) {
			mm1_shift = 0;
			mm1_buff = 0;
		} else {
			mm1_buff = (mm1_buff & ~(1<<mm1_shift)) | ((vval & 1)<<mm1_shift);
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
		*addval = vval;
}
