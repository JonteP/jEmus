#include "z80.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>	/* memcpy */
#include "cartridge.h"
#include "vdp.h"

						 	  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
static uint_fast8_t ctable[] = { 4,10, 7, 6, 4, 4, 7, 4, 4,11, 7, 6, 4, 4, 7, 4,/* 0 */
								 8,10, 7, 6, 4, 4, 7, 4,12,11, 7, 6, 4, 4, 7, 4,/* 1 */
								 7,10,16, 6, 4, 4, 7, 4, 7,11,16, 6, 4, 4, 7, 4,/* 2 */
								 7,10,13, 6,11,11,10, 4, 7,11,13, 6, 4, 4, 7, 4,/* 3 */
								 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* 4 */
								 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* 5 */
								 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* 6 */
								 7, 7, 7, 7, 7, 7, 4, 7, 4, 4, 4, 4, 4, 4, 7, 4,/* 7 */
								 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* 8 */
								 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* 9 */
								 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* a */
								 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* b */
								 5,10,10,10,10,11, 7,11, 5,10,10, 0,10,17, 7,11,/* c */
								 5,10,10,11,10,11, 7,11, 5, 4,10,11,10, 0, 7,11,/* d */
								 5,10,10,19,10,11, 7,11, 5, 4,10, 4,10, 0, 7,11,/* e */
								 5,10,10, 4,10,11, 7,11, 5, 6,10, 4,10, 0, 7,11,/* f */
							};

static inline void write_cpu_register(uint8_t, uint_fast8_t), cpuwrite(uint16_t, uint_fast8_t), interrupt_polling();
static inline uint8_t read_cpu_register(uint8_t), parcalc(uint8_t);
static inline uint8_t * cpuread(uint16_t);
static inline void adc(), adci(), acixi(), aciyi(), adcrp(), add(), add16(), addi(), addix(), addiy(), adixi(), adiyi(), and(), andi(), anixi(), aniyi(), bit(), bitix(), bitiy(), call(), callc(), cb(), ccf(), cp(), cpi(), cpixi(), cpiyi(), cpl(), daa(), dd(), ddcb(), dec(), dec16(), decix(), deciy(), deixi(), deiyi(), di(), djnz(), ed(), ei(), ex(), exaf(), exix(), exiy(), exx(), fd(), fdcb(), halt(), im(), in(), inc(), inc16(), incix(), inciy(), inixi(), iniyi(), jp(), jpc(), jphl(), jpix(), jpiy(), jr(), jrc(), ld(), ldar(), ld16(), ldim(), ldia(), ldix(), ldiy(), ldiix(), ldiiy(), ldixi(), ldiyi(), ldixh(), ldiyh(), ldixl(), ldiyl(), ldyin(), ldxin(), ldixn(), ldiyn(), ldbca(), lddea(), ldabc(), ldade(), ldidd(), ldihl(), ldai(), ldhli(), ldi(), ldir(), ldrp(), liniy(), lddr(), neg(), none(), noop(), nop(),
				   or(), ori(), orixi(), oriyi(), otir(), out(), outc(), outd(), outi(), pop(), popix(), popiy(), push(), pusix(), pusiy(), res(), resix(), resiy(), ret(), retc(), rl(), rla(), rlca(), rr(), rra(), rrc(), rld(), rrd(), rrca(), rst(), sbc(), sbc16(), sbci(), scf(), set(), setix(), setiy(), sla(), srl(), sub(), subi(), sbixi(), sbiyi(), xor(), xori();

uint_fast8_t mode, opcode, addmode, addcycle, tmpval8, s = 0, dummy, pcl, pch, dummywrite = 0, op, irqPulled = 0, nmiPulled = 0, irqPending = 0, nmiPending = 0, intDelay = 0, displace, halted = 0;
uint16_t addr, tmpval16;

/* Mapped memory */
uint_fast8_t *prgSlot[0x8], cpuRam[0x2000];

/* Internal registers */
uint16_t cpuAF = 0, cpuAFx = 0;
uint16_t cpuBC = 0, cpuDE = 0, cpuHL = 0, cpuBCx = 0, cpuDEx = 0, cpuHLx = 0; /* General purpose */
uint8_t *cpuAreg, *cpuFreg, *cpuBreg, *cpuCreg, *cpuDreg, *cpuEreg, *cpuHreg, *cpuLreg, reg_select = 0;
uint16_t *cpuAFreg, *cpuBCreg, *cpuDEreg, *cpuHLreg;
uint8_t cpuI, cpuR; /* special purpose (8-bit) */
uint16_t cpuIX, cpuIY, cpuPC = 0, cpuSP; /* special purpose (16-bit) */

/* Interrupt flip-flops */
uint8_t iff1, iff2, iMode = 0;

/* Vector pointers */
static const uint16_t nmi = 0xfffa, irq = 0x38;

uint32_t cpucc = 0;

char opmess[] = "Unimplemented opcode";
void opdecode() {

	 	 	 	 	 	 	 	 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
static void (*optable[0x100])() = {  nop, ld16,ldbca,inc16,  inc,  dec, ldim, rlca, exaf,add16,ldabc,dec16,  inc,  dec, ldim, rrca, /* 0 */
								    djnz, ld16,lddea,inc16,  inc,  dec, ldim,  rla,   jr,add16,ldade,dec16,  inc,  dec, ldim,  rra, /* 1 */
								   	 jrc, ld16,ldihl,inc16,  inc,  dec, ldim,  daa,  jrc,add16,ldhli,dec16,  inc,  dec, ldim,  cpl, /* 2 */
								     jrc, ld16, ldia,inc16,  inc,  dec, ldim,  scf,  jrc,add16, ldai,dec16,  inc,  dec, ldim,  ccf, /* 3 */
									  ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld, /* 4 */
									  ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld, /* 5 */
									  ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld, /* 6 */
									  ld,   ld,   ld,   ld,   ld,   ld, halt,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld, /* 7 */
									 add,  add,  add,  add,  add,  add,  add,  add,  adc,  adc,  adc,  adc,  adc,  adc,  adc,  adc, /* 8 */
									 sub,  sub,  sub,  sub,  sub,  sub,  sub,  sub,  sbc,  sbc,  sbc,  sbc,  sbc,  sbc,  sbc,  sbc, /* 9 */
								     and,  and,  and,  and,  and,  and,  and,  and,  xor,  xor,  xor,  xor,  xor,  xor,  xor,  xor, /* a */
									  or,   or,   or,   or,   or,   or,   or,   or,   cp,   cp,   cp,   cp,   cp,   cp,   cp,   cp, /* b */
									retc,  pop,  jpc,   jp,callc, push, addi,  rst, retc,  ret,  jpc,   cb,callc, call, adci,  rst, /* c */
									retc,  pop,  jpc,  out,callc, push, subi,  rst, retc,  exx,  jpc,   in,callc,   dd, sbci,  rst, /* d */
									retc,  pop,  jpc,   ex,callc, push, andi,  rst, retc, jphl,  jpc,   ex,callc,   ed, xori,  rst, /* e */
									retc,  pop,  jpc,   di,callc, push,  ori,  rst, retc,   ld,  jpc,   ei,callc,   fd,  cpi,  rst, /* f */
};


	/* static void (*cpuSlot[0x8])(uint_fast16_t, uint_fast8_t) { write_cpu_ram, write_ppu_register, write_cpu_register,  }; */


		if(irqPulled && iff1){
			halted = 0;
			/* TODO: this is assuming Mode 1 */
			vdp_wait += 13 * 3;
			cpucc += 13;
			irqPulled = 0;
			iff1 = 0;
			cpuwrite(--cpuSP, ((cpuPC & 0xff00) >> 8));
			cpuwrite(--cpuSP, ( cpuPC & 0x00ff));
			cpuPC = irq;
		}
		else if (!halted){
			op = *cpuread(cpuPC++);
			/* TODO: update memory refresh register */
	/*		printf("Opcode is: %02X at address: %04X\n",op,cpuPC-1); */
	/*	fprintf(logfile,"%04X %02X\t\t A:%02X X=%02X Y:%02X P:%02X SP:%02X CYC:%i\n",cpuPC-1,op,cpuA,cpuX,cpuY,cpuP,cpuS,ppudot); */
			addcycle = 0;
			vdp_wait += ctable[op] * 3;
			cpucc += ctable[op];
			(*optable[op])();
		}
		else{
			op = 0;
			vdp_wait += ctable[op] * 3;
			cpucc += ctable[op];
			(*optable[op])();
		}

		run_vdp(vdp_wait);
}

/* EXTENDED OPCODE TABLES */
void cb(){	 	 	 	 	 	 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
static void (*cbtable[0x100])() = { noop, noop, noop, noop, noop, noop, noop, noop,  rrc,  rrc,  rrc,  rrc,  rrc,  rrc,  rrc,  rrc, /* 0 */
									  rl,   rl,   rl,   rl,   rl,   rl,   rl,   rl,   rr,   rr,   rr,   rr,   rr,   rr,   rr,   rr, /* 1 */
									 sla,  sla,  sla,  sla,  sla,  sla,  sla,  sla, noop, noop, noop, noop, noop, noop, noop, noop, /* 2 */
									noop, noop, noop, noop, noop, noop, noop, noop,  srl,  srl,  srl,  srl,  srl,  srl,  srl,  srl, /* 3 */
									 bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit, /* 4 */
									 bit,  bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, /* 5 */
									 bit,  bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, /* 6 */
									 bit,  bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, bit, /* 7 */
									 res,  res, res, res, res, res, res, res, res, res, res, res, res, res, res, res, /* 8 */
									 res,  res, res, res, res, res, res, res, res, res, res, res, res, res, res, res, /* 9 */
									 res,  res, res, res, res, res, res, res, res, res, res, res, res, res, res, res, /* a */
									 res,  res, res, res, res, res, res, res, res, res, res, res, res, res, res, res, /* b */
									 set,  set, set, set, set, set, set, set, set, set, set, set, set, set, set, set, /* c */
									 set,  set, set, set, set, set, set, set, set, set, set, set, set, set, set, set, /* d */
									 set,  set, set, set, set, set, set, set, set, set, set, set, set, set, set, set, /* e */
									 set,  set, set, set, set, set, set, set, set, set, set, set, set, set, set, set, /* f */
									 };
								 /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
static uint_fast8_t ccbtable[] =  { 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* 0 */
								    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* 1 */
								    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* 2 */
									8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* 3 */
								    8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,/* 4 */
								    8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,/* 5 */
								    8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,/* 6 */
								    8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,/* 7 */
								    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* 8 */
								    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* 9 */
								    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* a */
								    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* b */
								    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* c */
								    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* d */
								    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* e */
								    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* f */
};
op = *cpuread(cpuPC++);
vdp_wait += ccbtable[op] * 3;
cpucc += ccbtable[op];
(*cbtable[op])();
}
void dd(){ 	 	 	 	 	 	 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
static void (*ddtable[0x100])() = { noop, noop, noop, noop, noop, noop, noop, noop, noop,addix, noop, noop, noop, noop, noop, noop, /* 0 */
								    noop, noop, noop, noop, noop, noop, noop, noop, noop,addix, noop, noop, noop, noop, noop, noop, /* 1 */
									noop, ldix, noop,incix, noop, noop,ldixh, noop, noop,addix,ldxin,decix, noop, noop,ldixl, noop, /* 2 */
									noop, noop, noop, noop,inixi,deixi,ldixn, noop, noop,addix, noop, noop, noop, noop, noop, noop, /* 3 */
									noop, noop, noop, noop, noop, noop,ldixi, noop, noop, noop, noop, noop, noop, noop,ldixi, noop, /* 4 */
									noop, noop, noop, noop, noop, noop,ldixi, noop, noop, noop, noop, noop, noop, noop,ldixi, noop, /* 5 */
									noop, noop, noop, noop, noop, noop,ldixi, noop, noop, noop, noop, noop, noop, noop,ldixi, noop, /* 6 */
								   ldiix,ldiix,ldiix,ldiix,ldiix,ldiix, noop,ldiix, noop, noop, noop, noop, noop, noop,ldixi, noop, /* 7 */
									noop, noop, noop, noop, noop, noop,adixi, noop, noop, noop, noop, noop, noop, noop,acixi, noop, /* 8 */
									noop, noop, noop, noop, noop, noop,sbixi, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 9 */
									noop, noop, noop, noop, noop, noop,anixi, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* a */
									noop, noop, noop, noop, noop, noop,orixi, noop, noop, noop, noop, noop, noop, noop,cpixi, noop, /* b */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, ddcb, noop, noop, noop, noop, /* c */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop,  nop, noop, noop, /* d */
									noop,popix, noop, exix, noop,pusix, noop, noop, noop, jpix, noop, noop, noop,  nop, noop, noop, /* e */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop,  nop, noop, noop, /* f */
									};
								  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
static uint_fast8_t cddtable[] =   {99,99,99,99,99,99,99,99,99,15,99,99,99,99,99,99,/* 0 */
									99,99,99,99,99,99,99,99,99,15,99,99,99,99,99,99,/* 1 */
									99,14,20,10, 8, 8,11,99,99,15,20,10, 8, 8,11,99,/* 2 */
									99,99,99,99,23,23,19,99,99,15,99,99,99,99,99,99,/* 3 */
									99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,99,/* 4 */
									99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,99,/* 5 */
									 8, 8, 8, 8, 8, 8,19, 8, 8, 8, 8, 8, 8, 8,19, 8,/* 6 */
									19,19,19,19,19,19,99,19,99,99,99,99, 8, 8,19,99,/* 7 */
									99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,99,/* 8 */
									99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,99,/* 9 */
									99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,99,/* a */
									99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,99,/* b */
									99,99,99,99,99,99,99,99,99,99,99, 0,99,99,99,99,/* c */
									99,99,99,99,99,99,99,99,99,99,99,99,99, 0,99,99,/* d */
									99,14,99,23,99,15,99,99,99, 8,99,99,99, 0,99,99,/* e */
									99,99,99,99,99,99,99,99,99,10,99,99,99, 0,99,99,/* f */
									};
op = *cpuread(cpuPC++);
vdp_wait += cddtable[op] * 3;
cpucc += cddtable[op];
(*ddtable[op])();
}
void ed(){ 	 	 	 	 	 	 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
static void (*edtable[0x100])() = { noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 0 */
		 	 	 	 	 	 	 	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 1 */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 2 */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 3 */
									noop, outc,sbc16,ldidd,  neg, noop,   im, noop, noop, outc,adcrp, ldrp,  neg, noop, noop, noop, /* 4 */
									noop, outc,sbc16,ldidd,  neg, noop,   im, noop, noop, outc,adcrp, ldrp,  neg, noop, noop, ldar, /* 5 */
									noop, outc,sbc16,ldidd,  neg, noop,   im,  rrd, noop, outc,adcrp, ldrp,  neg, noop, noop,  rld, /* 6 */
									noop, outc,sbc16,ldidd,  neg, noop,   im, noop, noop, outc,adcrp, ldrp,  neg, noop, noop, noop, /* 7 */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 8 */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 9 */
									 ldi, noop, noop, outi, noop, noop, noop, noop, noop, noop, noop, outd, noop, noop, noop, noop, /* a */
									ldir, noop, noop, otir, noop, noop, noop, noop, lddr, noop, noop, noop, noop, noop, noop, noop, /* b */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* c */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* d */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* e */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* f */
};
								  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
static uint_fast8_t cedtable[] =   {99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 0 */
									99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 1 */
									99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 2 */
									99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 3 */
									12,12,15,20, 8,14, 8, 9,12,12,15,20, 8,14, 8, 9,/* 4 */
									12,12,15,20, 8,14, 8, 9,12,12,15,20, 8,14, 8, 9,/* 5 */
									12,12,15,20, 8,14, 8,18,12,12,15,20, 8,14, 8,18,/* 6 */
									12,12,15,20,99,99,99,99,12,12,15,20,99,99,99,99,/* 7 */
									99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 8 */
									99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 9 */
									16,16,16,16,99,99,99,99,16,16,16,16,99,99,99,99,/* a */
									16,16,16,16,99,99,99,99,16,16,16,16,99,99,99,99,/* b */
									99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* c */
									99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* d */
									99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* e */
									99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* f */
									};
/* extra cycles: CPDR, CPIR, INDR, INIR, LDDR, LDIR, OTDR, OTIR */
	op = *cpuread(cpuPC++);
	vdp_wait += cedtable[op] * 3;
	cpucc += cedtable[op];
	(*edtable[op])();
}
void fd(){	 	     	 	 	 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
static void (*fdtable[0x100])() = { noop, noop, noop, noop, noop, noop, noop, noop, noop,addiy, noop, noop, noop, noop, noop, noop, /* 0 */
	 	 							noop, noop, noop, noop, noop, noop, noop, noop, noop,addiy, noop, noop, noop, noop, noop, noop, /* 1 */
									noop, ldiy,liniy,inciy, noop, noop,ldiyh, noop, noop,addiy,ldyin,deciy, noop, noop,ldiyl, noop, /* 2 */
									noop, noop, noop, noop,iniyi,deiyi,ldiyn, noop, noop,addiy, noop, noop, noop, noop, noop, noop, /* 3 */
									noop, noop, noop, noop, noop, noop,ldiyi, noop, noop, noop, noop, noop, noop, noop,ldiyi, noop, /* 4 */
									noop, noop, noop, noop, noop, noop,ldiyi, noop, noop, noop, noop, noop, noop, noop,ldiyi, noop, /* 5 */
									noop, noop, noop, noop, noop, noop,ldiyi, noop, noop, noop, noop, noop, noop, noop,ldiyi, noop, /* 6 */
								   ldiiy,ldiiy,ldiiy,ldiiy,ldiiy,ldiiy, noop,ldiiy, noop, noop, noop, noop, noop, noop,ldiyi, noop, /* 7 */
									noop, noop, noop, noop, noop, noop,adiyi, noop, noop, noop, noop, noop, noop, noop,aciyi, noop, /* 8 */
									noop, noop, noop, noop, noop, noop,sbiyi, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 9 */
									noop, noop, noop, noop, noop, noop,aniyi, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* a */
									noop, noop, noop, noop, noop, noop,oriyi, noop, noop, noop, noop, noop, noop, noop,cpiyi, noop, /* b */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, fdcb, noop, noop, noop, noop, /* c */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* d */
									noop,popiy, noop, exiy, noop,pusiy, noop, noop, noop, jpiy, noop, noop, noop, noop, noop, noop, /* e */
									noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* f */
									};
								  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
static uint_fast8_t cfdtable[] =   {99,99,99,99,99,99,99,99,99,15,99,99,99,99,99,99,/* 0 */
									99,99,99,99,99,99,99,99,99,15,99,99,99,99,99,99,/* 1 */
									99,14,20,10, 8, 8,11,99,99,15,20,10, 8, 8,11,99,/* 2 */
									99,99,99,99,23,23,19,99,99,15,99,99,99,99,99,99,/* 3 */
									99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,99,/* 4 */
									99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,99,/* 5 */
									8, 8, 8, 8, 8, 8,19, 8, 8, 8, 8, 8, 8, 8,19, 8,/* 6 */
									19,19,19,19,19,19,99,19,99,99,99,99, 8, 8,19,99,/* 7 */
									99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,99,/* 8 */
									99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,99,/* 9 */
									99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,99,/* a */
									99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,99,/* b */
									99,99,99,99,99,99,99,99,99,99,99, 0,99,99,99,99,/* c */
									99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* d */
									99,14,99,23,99,15,99,99,99, 8,99,99,99,99,99,99,/* e */
									99,99,99,99,99,99,99,99,99,10,99,99,99,99,99,99,/* f */
									};
	op = *cpuread(cpuPC++);
	vdp_wait += cfdtable[op] * 3;
	cpucc += cfdtable[op];
	(*fdtable[op])();
}

void ddcb(){ 	 	 	 	 	 	 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
  static void (*ddcbtable[0x100])() = { noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 0 */
									    noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 1 */
										noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 2 */
										noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 3 */
										noop, noop, noop, noop, noop, noop,bitix, noop, noop, noop, noop, noop, noop, noop,bitix, noop, /* 4 */
										noop, noop, noop, noop, noop, noop,bitix, noop, noop, noop, noop, noop, noop, noop,bitix, noop, /* 5 */
										noop, noop, noop, noop, noop, noop,bitix, noop, noop, noop, noop, noop, noop, noop,bitix, noop, /* 6 */
										noop, noop, noop, noop, noop, noop,bitix, noop, noop, noop, noop, noop, noop, noop,bitix, noop, /* 7 */
										noop, noop, noop, noop, noop, noop,resix, noop, noop, noop, noop, noop, noop, noop,resix, noop, /* 8 */
										noop, noop, noop, noop, noop, noop,resix, noop, noop, noop, noop, noop, noop, noop,resix, noop, /* 9 */
										noop, noop, noop, noop, noop, noop,resix, noop, noop, noop, noop, noop, noop, noop,resix, noop, /* a */
										noop, noop, noop, noop, noop, noop,resix, noop, noop, noop, noop, noop, noop, noop,resix, noop, /* b */
										noop, noop, noop, noop, noop, noop,setix, noop, noop, noop, noop, noop, noop, noop,setix, noop, /* c */
										noop, noop, noop, noop, noop, noop,setix, noop, noop, noop, noop, noop, noop, noop,setix, noop, /* d */
										noop, noop, noop, noop, noop, noop,setix, noop, noop, noop, noop, noop, noop, noop,setix, noop, /* e */
										noop, noop, noop, noop, noop, noop,setix, noop, noop, noop, noop, noop, noop, noop,setix, noop, /* f */
										};
									  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
  static uint_fast8_t cddcbtable[] =   {99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 0 */
										99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 1 */
										99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 2 */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,23,99,/* 3 */
										99,99,99,99,99,99,20,99,99,99,99,99,99,99,20,99,/* 4 */
										99,99,99,99,99,99,20,99,99,99,99,99,99,99,20,99,/* 5 */
										99,99,99,99,99,99,20, 8, 8, 8, 8, 8, 8, 8,20, 8,/* 6 */
										99,99,99,99,99,99,20,19,99,99,99,99, 8, 8,20,99,/* 7 */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 8 */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 9 */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* a */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* b */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* c */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* d */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* e */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* f */
										};
	displace = *cpuread(cpuPC++);
	op = *cpuread(cpuPC++);
	vdp_wait += cddcbtable[op] * 3;
	cpucc += cddcbtable[op];
	(*ddcbtable[op])();
}
void fdcb(){ 	 	 	 	 	 	 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
  static void (*fdcbtable[0x100])() = { noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 0 */
									    noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 1 */
										noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 2 */
										noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 3 */
										noop, noop, noop, noop, noop, noop,bitiy, noop, noop, noop, noop, noop, noop, noop,bitiy, noop, /* 4 */
										noop, noop, noop, noop, noop, noop,bitiy, noop, noop, noop, noop, noop, noop, noop,bitiy, noop, /* 5 */
										noop, noop, noop, noop, noop, noop,bitiy, noop, noop, noop, noop, noop, noop, noop,bitiy, noop, /* 6 */
										noop, noop, noop, noop, noop, noop,bitiy, noop, noop, noop, noop, noop, noop, noop,bitiy, noop, /* 7 */
										noop, noop, noop, noop, noop, noop,resiy, noop, noop, noop, noop, noop, noop, noop,resiy, noop, /* 8 */
										noop, noop, noop, noop, noop, noop,resiy, noop, noop, noop, noop, noop, noop, noop,resiy, noop, /* 9 */
										noop, noop, noop, noop, noop, noop,resiy, noop, noop, noop, noop, noop, noop, noop,resiy, noop, /* a */
										noop, noop, noop, noop, noop, noop,resiy, noop, noop, noop, noop, noop, noop, noop,resiy, noop, /* b */
										noop, noop, noop, noop, noop, noop,setiy, noop, noop, noop, noop, noop, noop, noop,setiy, noop, /* c */
										noop, noop, noop, noop, noop, noop,setiy, noop, noop, noop, noop, noop, noop, noop,setiy, noop, /* d */
										noop, noop, noop, noop, noop, noop,setiy, noop, noop, noop, noop, noop, noop, noop,setiy, noop, /* e */
										noop, noop, noop, noop, noop, noop,setiy, noop, noop, noop, noop, noop, noop, noop,setiy, noop, /* f */
										};
									  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
  static uint_fast8_t cfdcbtable[] =   {99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 0 */
										99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 1 */
										99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 2 */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,23,99,/* 3 */
										99,99,99,99,99,99,20,99,99,99,99,99,99,99,20,99,/* 4 */
										99,99,99,99,99,99,20,99,99,99,99,99,99,99,20,99,/* 5 */
										99,99,99,99,99,99,20, 8, 8, 8, 8, 8, 8, 8,20, 8,/* 6 */
										99,99,99,99,99,99,20,19,99,99,99,99, 8, 8,20,99,/* 7 */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 8 */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 9 */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* a */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* b */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* c */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* d */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* e */
										99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* f */
										};
	displace = *cpuread(cpuPC++);
	op = *cpuread(cpuPC++);
	vdp_wait += cfdcbtable[op] * 3;
	cpucc += cfdcbtable[op];
	(*fdcbtable[op])();
}

/* OPCODES */

void adc(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint16_t res = *cpuAreg + *r[op & 7] + (*cpuFreg & 0x01);
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (((*r[op & 7] + (*cpuFreg & 0x01)) + (*cpuFreg & 0x01)) ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void adci(){
	uint8_t val = *cpuread(cpuPC++);
	uint16_t res = *cpuAreg + val + (*cpuFreg & 0x01);
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & ((val + (*cpuFreg & 0x01)) ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void adcrp(){
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP};
	uint32_t res = *cpuHLreg + *rp[(op >> 4) & 0x03] + (*cpuFreg & 0x01);
	*cpuFreg = (res & 0x8000) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xffff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xfff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg = (res > 0xffff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuHLreg = res;
}

void acixi(){
	uint8_t val = *cpuread(cpuPC++);
	uint16_t res = *cpuAreg + *cpuread(cpuIX + val) + (*cpuFreg & 0x01);
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & ((*cpuread(cpuIX + val) + (*cpuFreg & 0x01)) ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void aciyi(){
	uint8_t val = *cpuread(cpuPC++);
	uint16_t res = *cpuAreg + *cpuread(cpuIY + val) + (*cpuFreg & 0x01);
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & ((*cpuread(cpuIY + val) + (*cpuFreg & 0x01)) ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void add(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint16_t res;
	res = *cpuAreg + *r[op & 7];
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (*r[op & 7] ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void add16(){
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP};
	uint32_t res;
	res = *cpuHLreg + *rp[(op >> 4) & 0x03];
	*cpuFreg = (res > 0xfff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (res > 0xffff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuHLreg = res;
}

void addi(){
	uint8_t val = *cpuread(cpuPC++);
	uint16_t res = *cpuAreg + val;
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (val ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void addix(){
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, &cpuIX, &cpuSP};
	uint32_t res;
	res = cpuIX + *rp[(op >> 4) & 0x03];
	*cpuFreg = (res > 0xfff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (res > 0xffff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuHLreg = res;
}

void addiy(){
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, &cpuIX, &cpuSP};
	uint32_t res;
	res = cpuIY + *rp[(op >> 4) & 0x03];
	*cpuFreg = (res > 0xfff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (res > 0xffff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuHLreg = res;
}

void adixi(){
	uint8_t val = *cpuread(cpuPC++);
	uint16_t res = *cpuAreg + *cpuread(cpuIX + val);
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (val ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void adiyi(){
	uint8_t val = *cpuread(cpuPC++);
	uint16_t res = *cpuAreg + *cpuread(cpuIY + val);
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (val ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void and(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*cpuAreg &= *r[op & 7];
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg | 0x10); /* H flag */
	*cpuFreg = (!(parcalc(*cpuAreg) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (*cpuFreg & ~0x01); /* C flag */
}

void andi(){
*cpuAreg &= *cpuread(cpuPC++);
*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
*cpuFreg = (*cpuFreg | 0x10); /* H flag */
*cpuFreg = (!(parcalc(*cpuAreg) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
*cpuFreg = (*cpuFreg & ~0x01); /* C flag */
}

void anixi(){
	uint8_t val = *cpuread(cpuPC++);
	*cpuAreg &= *cpuread(cpuIX + val);
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (!(parcalc(*cpuAreg) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (*cpuFreg & ~0x01); /* C flag */
}

void aniyi(){
	uint8_t val = *cpuread(cpuPC++);
	*cpuAreg &= *cpuread(cpuIY + val);
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (!(parcalc(*cpuAreg) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (*cpuFreg & ~0x01); /* C flag */
}

void bit(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t val = *r[op & 7] & (1 << ((op >> 3) & 7));
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg |= 0x10; /* H flag */
	*cpuFreg &= ~0x02; /* N flag */
}

void bitix(){
	uint8_t val = *cpuread(cpuIX + displace);
	val = val & (1 << ((op >> 3) & 7));
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg |= 0x10; /* H flag */
	*cpuFreg &= ~0x02; /* N flag */
}

void bitiy(){
	uint8_t val = *cpuread(cpuIY + displace);
	val = val & (1 << ((op >> 3) & 7));
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg |= 0x10; /* H flag */
	*cpuFreg &= ~0x02; /* N flag */
}


void call(){
	uint16_t address = {0};
	address = ((address & 0xff00) | (*cpuread(cpuPC++)));
	address = ((address & 0x00ff) | ((*cpuread(cpuPC++)) << 8));
	cpuwrite(--cpuSP, ((cpuPC & 0xFF00) >> 8));
	cpuwrite(--cpuSP, (cpuPC & 0x00FF));
	cpuPC = address;
}

void callc(){
	uint8_t cc[8] = {!(*cpuFreg & 0x40), (*cpuFreg & 0x40), !(*cpuFreg & 0x01), (*cpuFreg & 0x01), !(*cpuFreg & 0x04), (*cpuFreg & 0x04), !(*cpuFreg & 0x80), (*cpuFreg & 0x80)};
	uint16_t address = {0};
	address = ((address & 0xff00) | (*cpuread(cpuPC++)));
	address = ((address & 0x00ff) | ((*cpuread(cpuPC++)) << 8));
	if(cc[(op>>3) & 7]) {
		cpuwrite(--cpuSP, ((cpuPC & 0xFF00) >> 8));
		cpuwrite(--cpuSP, (cpuPC & 0x00FF));
		cpuPC = address;
		/* TODO: add extra cycles */
	}
}

void ccf(){
	*cpuFreg ^= 0x01;
}

void cp(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t val = *r[op & 0x07];
	uint16_t res = *cpuAreg - val;
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (val ^ *cpuAreg) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}

void cpi(){
	uint8_t val;
	uint16_t res;
	val = *cpuread(cpuPC++);
	res = *cpuAreg - val;
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (val ^ *cpuAreg) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}

void cpixi(){
	uint8_t val = *cpuread(cpuPC++);
	uint16_t res = *cpuAreg - *cpuread(cpuIX + val);
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (*cpuread(cpuIX + val) ^ *cpuAreg) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}

void cpiyi(){
	uint8_t val = *cpuread(cpuPC++);
	uint16_t res = *cpuAreg - *cpuread(cpuIY + val);
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (*cpuread(cpuIY + val) ^ *cpuAreg) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}

void cpl(){
	*cpuAreg ^= 0xff;
	*cpuFreg = (*cpuFreg | 0x10); /* H flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
}

void daa(){
	uint8_t nibbleL = (*cpuAreg & 0x0f);
	uint8_t nibbleH = (*cpuAreg >> 4);
	uint8_t addval;
	switch (*cpuFreg & 0x82){ /* consider Subtract + Half carry + Carry flags */
	case 0x00: /* None */
		addval = (nibbleH < 0xa) ? (addval & 0x0f) : ((addval & 0x0f) | 0x60);
		addval = (nibbleL < 0xa) ? (addval & 0xf0) : ((addval & 0xf0) | 0x06);
		*cpuAreg += addval;
		break;
	case 0x01: /* Carry */
		addval = ((addval & 0x0f) | 0x60);
		addval = (nibbleL < 0xa) ? (addval & 0xf0) : ((addval & 0xf0) | 0x06);
		*cpuAreg += addval;
		break;
	case 0x02: /* Subtract */
		*cpuAreg += 0;
		break;
	case 0x03: /* Subtract+Carry */
		*cpuAreg += 0xa0;
		break;
	case 0x10: /* Half-carry */
		addval = (nibbleH < 0xa) ? (addval & 0x0f) : ((addval & 0x0f) | 0x60);
		addval = ((addval & 0xf0) | 0x06);
		*cpuAreg += addval;
		break;
	case 0x11:  /* Half-carry+Carry */
		*cpuAreg += 0x66;
		break;
	case 0x12:  /* Half-carry+Subtract */
		*cpuAreg += 0xfa;
		break;
	case 0x13:  /* Half-carry+Subtract+Carry */
		*cpuAreg += 0x9a;
		break;
	}
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (!(parcalc(*cpuAreg) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	/* TODO: correct C and H flags */
}


void dec(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg}, tmp;
	tmp = (*r[(op>>3) & 7]);
	(*r[(op>>3) & 7])--;
	*cpuFreg = (*r[(op>>3) & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[(op>>3) & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((tmp & 0x1f) == 0x10) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
}

void dec16(){
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP};
	(*rp[(op>>4) & 3])--;
}

void decix(){
	cpuIX--;
}

void deciy(){
	cpuIY--;
}

void deixi(){
	uint8_t val = *cpuread(cpuPC++);
	(*cpuread(cpuIX + val))--;
}

void deiyi(){
	uint8_t val = *cpuread(cpuPC++);
	(*cpuread(cpuIY + val))--;
}

void di() {
	iff1 = iff2 = 0;
}

void djnz(){
	(*cpuBreg)--;
	if(*cpuBreg) {
		cpuPC += ((int8_t)*cpuread(cpuPC) + 1);
		/* TODO: add extra cycles */
	}
	else
		cpuPC++;
}

void ei() {
	iff1 = iff2 = 1;
}

void ex(){
	uint16_t tmp = cpuDE;
	cpuDE = cpuHL;
	cpuHL = tmp;
}

void exaf(){
	uint16_t tmp = cpuAF;
	cpuAF = cpuAFx;
	cpuAFx = tmp;
}

void exix(){
	uint8_t tmp = *cpuread(cpuSP);
	uint8_t tmp2 = *cpuread(cpuSP+1);
	cpuwrite(cpuSP, (cpuIX & 0xff));
	cpuwrite(cpuSP+1, (cpuIX >> 8));
	cpuIX = ((tmp2 << 8) | tmp);
}

void exiy(){
	uint8_t tmp = *cpuread(cpuSP);
	uint8_t tmp2 = *cpuread(cpuSP+1);
	cpuwrite(cpuSP, (cpuIY & 0xff));
	cpuwrite(cpuSP+1, (cpuIY >> 8));
	cpuIY = ((tmp2 << 8) | tmp);
}

void exx(){
	uint16_t tmpBC = cpuBC, tmpDE = cpuDE, tmpHL = cpuHL;
	cpuBC = cpuBCx;
	cpuDE = cpuDEx;
	cpuHL = cpuHLx;
	cpuBCx = tmpBC;
	cpuDEx = tmpDE;
	cpuHLx = tmpHL;
}

void halt(){
	halted = 1;
}

void im(){
	uint8_t im[8] = {0, 0, 1, 2, 0, 0, 1, 2};
	iMode = im[(op>>3) & 7];
	if (iMode != 1){
		printf("Unsupported interrupt mode: %i\n",iMode);
		exit(1);
	}
}

void in(){
	uint8_t reg;
	reg = *cpuread(cpuPC++);
	*cpuAreg = read_cpu_register(reg);
}

void inc(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg}, tmp;
	tmp = (*r[(op>>3) & 7]);
	(*r[(op>>3) & 7])++;
	*cpuFreg = (*r[(op>>3) & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[(op>>3) & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((tmp & 0x0f) == 0x0f) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x7f) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
}

void inc16(){
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP};
	(*rp[(op>>4) & 3])++;
}

void incix(){
	cpuIX++;
}

void inciy(){
	cpuIY++;
}

void inixi(){
	uint8_t val = *cpuread(cpuPC++);
	(*cpuread(cpuIX + val))++;
}

void iniyi(){
	uint8_t val = *cpuread(cpuPC++);
	(*cpuread(cpuIY + val))++;
}

void jp(){
	uint16_t address;
	address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	cpuPC = address;
}

void jpc(){
	uint8_t cc[8] = {!(*cpuFreg & 0x40), (*cpuFreg & 0x40), !(*cpuFreg & 0x01), (*cpuFreg & 0x01), !(*cpuFreg & 0x04), (*cpuFreg & 0x04), !(*cpuFreg & 0x80), (*cpuFreg & 0x80)};
	uint16_t address;
	address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	if(cc[(op>>3) & 7]) {
		cpuPC = address;
	}
}

void jphl(){
	cpuPC = *cpuHLreg;
}

void jpix(){
	cpuPC = cpuIX;
}

void jpiy(){
	cpuPC = cpuIY;
}

void jr(){
	cpuPC += ((int8_t)*cpuread(cpuPC) + 1);
}

void jrc(){
	uint8_t cc[4] = {!(*cpuFreg & 0x40), (*cpuFreg & 0x40), !(*cpuFreg & 0x01), (*cpuFreg & 0x01)};
	if(cc[((op>>3) & 7) - 4]) {
		/* TODO: add extra cycles */
		jr();
	}
	else
		cpuPC++;
}

void ld(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*r[(op>>3) & 7] = *r[op & 7];
}

void ldar(){
	*cpuAreg = cpuR;
}

void ld16(){
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP}, tmp;
	tmp = *cpuread(cpuPC++);
	tmp |= ((*cpuread(cpuPC++)) << 8);
	*rp[((op>>4) & 3)] = tmp;
}

void ldidd(){
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP}, tmp;
	tmp = *cpuread(cpuPC++);
	tmp |= ((*cpuread(cpuPC++)) << 8);
	cpuwrite(tmp++, (*rp[(op >> 4) & 0x03] & 0xff));
	cpuwrite(tmp, (*rp[(op >> 4) & 0x03] >> 8));
}

void ldim(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*r[(op>>3) & 7] = *cpuread(cpuPC++);
}

void ldia(){
	uint16_t tmp;
	tmp = *cpuread(cpuPC++);
	tmp |= ((*cpuread(cpuPC++)) << 8);
	cpuwrite(tmp, *cpuAreg);
}

void ldix(){
	uint16_t tmp = *cpuread(cpuPC++);
	tmp |= ((*cpuread(cpuPC++)) << 8);
	cpuIX = tmp;
}

void ldiy(){
	uint16_t tmp = *cpuread(cpuPC++);
	tmp |= ((*cpuread(cpuPC++)) << 8);
	cpuIY = tmp;
}

void ldiix(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	cpuwrite(cpuIX + *cpuread(cpuPC++), *r[op & 7]);
}

void ldiiy(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	cpuwrite(cpuIY + *cpuread(cpuPC++), *r[op & 7]);
}

void liniy(){
	uint16_t address = *cpuread(cpuPC++);
	address |= (*cpuread(cpuPC++) << 8);
	*cpuread(address++) = (cpuIY & 0xff);
	*cpuread(address) = ((cpuIY & 0xff00) >> 8);
}

void ldixi(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*r[(op >> 3) & 7] = *cpuread(cpuIX + *cpuread(cpuPC++));
}

void ldiyi(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*r[(op >> 3) & 7] = *cpuread(cpuIY + *cpuread(cpuPC++));
}

void ldixh(){
	cpuIX = ((cpuIX & 0x00ff) | (*cpuread(cpuPC++) << 8));
}

void ldiyh(){
	cpuIY = ((cpuIY & 0x00ff) | (*cpuread(cpuPC++) << 8));
}

void ldixl(){
	cpuIX = ((cpuIX & 0xff00) | (*cpuread(cpuPC++) & 0x00ff));
}

void ldiyl(){
	cpuIY = ((cpuIY & 0xff00) | (*cpuread(cpuPC++) & 0x00ff));
}

void ldxin(){
	uint16_t address;
	address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	cpuIX = *cpuread(address++);
	cpuIX |= (*cpuread(address) << 8);
}

void ldyin(){
	uint16_t address;
	address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	cpuIY = *cpuread(address++);
	cpuIY |= (*cpuread(address) << 8);
}

void ldixn(){
	uint8_t offset = *cpuread(cpuPC++);
	uint8_t val = *cpuread(cpuPC++);
	cpuwrite(cpuIX + offset, val);
}

void ldiyn(){
	uint8_t offset = *cpuread(cpuPC++);
	uint8_t val = *cpuread(cpuPC++);
	cpuwrite(cpuIY + offset, val);
}

void ldbca(){
	cpuwrite(*cpuBCreg, *cpuAreg);
}
void lddea(){
	cpuwrite(*cpuDEreg, *cpuAreg);
}
void ldabc(){
	*cpuAreg = *cpuread(*cpuBCreg);
}

void ldade(){
	*cpuAreg = *cpuread(*cpuDEreg);
}

void ldihl(){
	uint16_t address;
	address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	cpuwrite(address++, *cpuLreg);
	cpuwrite(address, *cpuHreg);
}

void ldhli(){
	uint16_t address;
	address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	*cpuLreg = *cpuread(address++);
	*cpuHreg = *cpuread(address);
}

void ldai(){
	uint16_t tmp;
	tmp = *cpuread(cpuPC++);
	tmp |= ((*cpuread(cpuPC++)) << 8);
	*cpuAreg = *cpuread(tmp);
}

void ldi(){
	(*cpuBCreg)--;
	cpuwrite((*cpuDEreg)++, *cpuread((*cpuHLreg)++));
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg = (*cpuBCreg) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
}

void ldir(){
	uint16_t tmp = *cpuBCreg;
	for (uint16_t i = 0; i < tmp; i++) {
		(*cpuBCreg)--;
		cpuwrite((*cpuDEreg)++, *cpuread((*cpuHLreg)++));
	}
}

void ldrp(){ /* Load to register pair from immediate address (16-bit) */
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP};
	uint16_t tmp = *cpuread(cpuPC++);
	tmp |= ((*cpuread(cpuPC++)) << 8);
	*rp[(op >> 4) & 0x03] = *cpuread(tmp++);
	*rp[(op >> 4) & 0x03] |= (*cpuread(tmp) << 8);
}

void lddr(){
	uint16_t tmp = *cpuBCreg;
	for (uint16_t i = 0; i < tmp; i++) {
		(*cpuBCreg)--;
		cpuwrite((*cpuDEreg)--, *cpuread((*cpuHLreg)--));
	}
}

void neg(){
	uint16_t res = 0 - *cpuAreg;
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuAreg == 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
	*cpuFreg = (*cpuFreg != 0x00) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void none(){}

void noop(){
printf("Illegal opcode: %02x %02x %02x %02x\n",*cpuread(cpuPC-4),*cpuread(cpuPC-3),*cpuread(cpuPC-2),op);
exit(1);}

void nop(){}

void or(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*cpuAreg |= *r[op & 7];
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (!(parcalc(*cpuAreg) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (*cpuFreg & ~0x01); /* C flag */
}

void ori(){
	*cpuAreg |= *cpuread(cpuPC++);
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (!(parcalc(*cpuAreg) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (*cpuFreg & ~0x01); /* C flag */
}

void orixi(){
	uint8_t val = *cpuread(cpuPC++);
	*cpuAreg |= *cpuread(cpuIX + val);
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (!(parcalc(*cpuAreg) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (*cpuFreg & ~0x01); /* C flag */
}

void oriyi(){
	uint8_t val = *cpuread(cpuPC++);
	*cpuAreg |= *cpuread(cpuIY + val);
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (!(parcalc(*cpuAreg) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (*cpuFreg & ~0x01); /* C flag */
}

void otir(){
uint8_t tmp;
tmp = *cpuread(*cpuHLreg); /* to be written to port */
(*cpuBreg)--; /* byte counter */
write_cpu_register(*cpuCreg, tmp);
(*cpuHLreg)++;
if (*cpuBreg){
	cpuPC--;
	cpuPC--;
}
*cpuFreg = (*cpuFreg | 0x40); /* Z flag */
*cpuFreg = (*cpuFreg | 0x02); /* N flag */
}

void out(){
	uint8_t reg;
	reg = *cpuread(cpuPC++);
	write_cpu_register(reg, *cpuAreg);
}

void outc(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	write_cpu_register(*cpuCreg, *r[(op >> 3) & 7]);
}

void outd(){
	uint8_t tmp;
	tmp = *cpuread(*cpuHLreg); /* to be written to port */
	(*cpuBreg)--; /* byte counter */
	write_cpu_register(*cpuCreg, tmp);
	(*cpuHLreg)--;
	*cpuFreg = (!*cpuBreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
}

void outi(){
	uint8_t tmp;
	tmp = *cpuread(*cpuHLreg); /* to be written to port */
	(*cpuBreg)--; /* byte counter */
	write_cpu_register(*cpuCreg, tmp);
	(*cpuHLreg)++;
	*cpuFreg = (!*cpuBreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
}

void pop(){
	uint16_t *rp2[4] = {cpuBCreg, cpuDEreg, cpuHLreg, cpuAFreg}, tmp;
	tmp = *cpuread(cpuSP++);
	tmp |= ((*cpuread(cpuSP++)) << 8);
	*rp2[(op>>4) & 3] = tmp;
}

void popix(){
	uint16_t tmp = *cpuread(cpuSP++);
	tmp |= ((*cpuread(cpuSP++)) << 8);
	cpuIX = tmp;
}

void popiy(){
	uint16_t tmp = *cpuread(cpuSP++);
	tmp |= ((*cpuread(cpuSP++)) << 8);
	cpuIY = tmp;
}

void push(){
	uint16_t *rp2[4] = {cpuBCreg, cpuDEreg, cpuHLreg, cpuAFreg}, tmp;
	tmp = *rp2[(op>>4) & 3];
	cpuwrite(--cpuSP, ((tmp & 0xff00) >> 8));
	cpuwrite(--cpuSP, ( tmp & 0x00ff));
}

void pusix(){
	cpuwrite(--cpuSP, ((cpuIX & 0xff00) >> 8));
	cpuwrite(--cpuSP, (cpuIX & 0x00ff));
}

void pusiy(){
	cpuwrite(--cpuSP, ((cpuIY & 0xff00) >> 8));
	cpuwrite(--cpuSP, (cpuIY & 0x00ff));
}

void res(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*r[op & 7] &= ~(1 << ((op >> 3) & 7));
}

void resix(){
	*cpuread(cpuIX+displace) &= ~(1 << ((op >> 3) & 7));
}

void resiy(){
	*cpuread(cpuIY+displace) &= ~(1 << ((op >> 3) & 7));
}

void ret(){
	uint16_t tmp;
	tmp = *cpuread(cpuSP++);
	tmp |= ((*cpuread(cpuSP++)) << 8);
	cpuPC = tmp;
}

void retc(){
	uint8_t cc[8] = {!(*cpuFreg & 0x40), (*cpuFreg & 0x40), !(*cpuFreg & 0x01), (*cpuFreg & 0x01), !(*cpuFreg & 0x04), (*cpuFreg & 0x04), !(*cpuFreg & 0x80), (*cpuFreg & 0x80)};
	if (cc[(op >> 3) & 7]){
		ret();
		/* TODO: add extra cycles */
	}
}

void rl(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] << 1) | (*cpuFreg & 0x01));
	*cpuFreg = (*r[op & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[op & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (!(parcalc(*r[op & 7]) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}

void rla(){
	uint8_t tmp = *cpuAreg;
	*cpuAreg = ((*cpuAreg << 1) | (*cpuFreg & 0x01));
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}

void rlca(){
	uint8_t tmp = *cpuAreg;
	*cpuAreg = ((*cpuAreg << 1) | (tmp >> 7));
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}

void rr(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] >> 1) | (*cpuFreg << 7));
	*cpuFreg = (*r[op & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[op & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (!(parcalc(*r[op & 7]) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}

void rra(){
	uint8_t tmp = *cpuAreg;
	*cpuAreg = ((*cpuAreg >> 1) | (*cpuFreg << 7));
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}

void rrc(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] >> 1) | (tmp << 7));
	*cpuFreg = (*r[op & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[op & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (!(parcalc(*r[op & 7]) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}

void rrca(){
	uint8_t tmp = *cpuAreg;
	*cpuAreg = (*cpuAreg >> 1);
	*cpuAreg |= (tmp << 7);
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}

void rld(){
	uint8_t tmp = *cpuAreg;
	uint8_t val = *cpuread(*cpuHLreg);
	*cpuAreg = ((*cpuAreg & 0xf0) | ((val & 0xf0) >> 4));
	val = (val << 4);
	val |= (tmp & 0x0f);
	cpuwrite(*cpuHLreg, val);
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (!(parcalc(*cpuAreg) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
}

void rrd(){
	uint8_t tmp = *cpuAreg;
	uint8_t val = *cpuread(*cpuHLreg);
	*cpuAreg = ((*cpuAreg & 0xf0) | (val & 0x0f));
	val = (val >> 4);
	val |= (tmp << 4);
	cpuwrite(*cpuHLreg, val);
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (!(parcalc(*cpuAreg) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
}

void rst(){
	cpuwrite(--cpuSP, ((cpuPC & 0xff00) >> 8));
	cpuwrite(--cpuSP, ( cpuPC & 0x00ff));
	cpuPC = (op & 0x38);
}

void sbc(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint16_t res = *cpuAreg - *r[op & 7] - (*cpuFreg & 0x01);
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & ((*r[op & 7] - (*cpuFreg & 0x01)) ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void sbc16(){
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP};
	uint32_t res = (*cpuHLreg - *rp[(op >> 4) & 0x03] - (*cpuFreg & 0x01));
	*cpuFreg = (res & 0x8000) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xffff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xfff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuHLreg ^ res) & (*rp[(op >> 4) & 0x03] ^ res) & 0x8000) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
	*cpuFreg = (res > 0xffff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuHLreg = res;
}

void sbci(){
	printf("%s: %02x\n",opmess,op);
	exit(1);}

void scf(){
*cpuFreg |= 0x01;
}

void set(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*r[op & 7] |= (1 << ((op >> 3) & 7));
}

void setix(){
	*cpuread(cpuIX + displace) |= (1 << ((op >> 3) & 7));
}

void setiy(){
	*cpuread(cpuIY + displace) |= (1 << ((op >> 3) & 7));
}

void sla(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*cpuFreg = (*r[op & 7] & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*r[op & 7] = (*r[op & 7] << 1);
	*cpuFreg &= ~0x80; /* S flag */
	*cpuFreg = (!*r[op & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg = (!(parcalc(*r[op & 7]) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
}

void srl(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*cpuFreg = (*r[op & 7] & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*r[op & 7] = (*r[op & 7] >> 1);
	*cpuFreg &= ~0x80; /* S flag */
	*cpuFreg = (!*r[op & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg = (!(parcalc(*r[op & 7]) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
}

void sub(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint16_t res = *cpuAreg - *r[op & 7];
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (*r[op & 7] ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void subi(){
	uint8_t val = *cpuread(cpuPC++);
	uint16_t res = *cpuAreg - val;
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (val ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void sbixi(){
	uint8_t val = *cpuread(cpuPC++);
	uint16_t res = *cpuAreg - *cpuread(cpuIX + val);
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (*cpuread(cpuIX + val) ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void sbiyi(){
	uint8_t val = *cpuread(cpuPC++);
	uint16_t res = *cpuAreg - *cpuread(cpuIY + val);
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (res > 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (*cpuread(cpuIY + val) ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
	*cpuFreg = (res > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}

void xor(){
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*cpuAreg ^= *r[op & 7];
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (!(parcalc(*cpuAreg) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (*cpuFreg & ~0x01); /* C flag */
}

void xori(){
	*cpuAreg ^= *cpuread(cpuPC++);
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (!(parcalc(*cpuAreg) % 2)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (*cpuFreg & ~0x01); /* C flag */
}

uint8_t read_cpu_register(uint8_t reg) {
	uint8_t value;
	switch (reg & 0xc1){
	case 0x00:
	case 0x01:
		printf("Reading (dummy value) from register: %02x\n",reg);
		break;
	case 0x40: /* Read VDP V Counter */
		value = vCounter;
		break;
	case 0x41: /* Read VDP H Counter */
		value = hCounter;
		break;
	case 0x80: /* Read VDP Data Port */
		value = read_vdp_data();
		controlFlag = 0;
		break;
	case 0x81: /* Read VDP Control Port */
		/*printf("Reading VDP control port: %02x\n",reg);*/
		value = statusFlags;
		statusFlags = 0; /* clear all flags when read */
		controlFlag = 0;
		irqPulled = 0;
		break;
	case 0xc0:
		/*printf("Reading I/O port A/B: %02x\n",reg);*/
		value = 0xff;
		break;
	case 0xc1:
		/*printf("Reading I/O port B/Misc: %02x\n",reg);*/
		value = 0xff;
		break;
	}
	return value;
}

void write_cpu_register(uint8_t reg, uint_fast8_t value) {
	switch (reg & 0xc1){
	case 0x00:
		printf("Writing to memory control register: %02x\n",reg);
		break;
	case 0x01:
		printf("Writing to I/O control register: %02x\n",reg);
		break;
	case 0x40:
	case 0x41:
	/*	printf("Writing to SN76489 PSG: %02x\n",reg);*/
		break;
	case 0x80:
		/*printf("Writing to VDP data port: %02x\n",reg);*/
		write_vdp_data(value);
		controlFlag = 0;
		break;
	case 0x81:
		/*printf("Writing to VDP control port: %02x\n",reg);*/
		write_vdp_control(value);
		break;
	case 0xc0: /* Keyboard support? */
	case 0xc1:
		/*printf("Ineffective write to I/O port: %02x\n",reg);*/
		break;
	}
}

uint8_t * cpuread(uint16_t address) {
	uint_fast8_t *value;
	if (address >= 0xc000) /* reading from RAM */
		value = &cpuRam[address & 0x1fff];
	else if (address < 0xc000 && address >= 0x400) /* reading from ROM */
		value = &bank[address >> 14][address & 0x3fff];
	else if (address < 0x400)
		value = (rom + (address & 0x3ff));
	return value;
}
/* TODO: use cpuwrite for all memory changes to catch register writes */
void cpuwrite(uint16_t address, uint_fast8_t value) {
	if (address >= 0xc000) /* writing to RAM */
		cpuRam[address & 0x1fff] = value;
	if(address >=0xfff8){
		switch(address & 0xf){
		case 0xd:
			fcr[0] = (value & 0xf);
			break;
		case 0xe:
			fcr[1] = (value & 0xf);
			break;
		case 0xf:
			fcr[2] = (value & 0xf);
			break;
		}
		bank[0] = rom + (0x4000 * fcr[0]); /* TODO: wrong behavior for fcr0 */
		bank[1] = rom + (0x4000 * fcr[1]);
		bank[2] = rom + (0x4000 * fcr[2]);
	}
}

void power_reset () {
	halted = 0;
iff1 = iff2 = 0;
cpuAreg = (uint8_t *)&cpuAF+1;
cpuFreg = (uint8_t *)&cpuAF;
cpuBreg = (uint8_t *)&cpuBC+1;
cpuCreg = (uint8_t *)&cpuBC;
cpuDreg = (uint8_t *)&cpuDE+1;
cpuEreg = (uint8_t *)&cpuDE;
cpuHreg = (uint8_t *)&cpuHL+1;
cpuLreg = (uint8_t *)&cpuHL;
cpuAFreg = (uint16_t *)&cpuAF;
cpuBCreg = (uint16_t *)&cpuBC;
cpuDEreg = (uint16_t *)&cpuDE;
cpuHLreg = (uint16_t *)&cpuHL;
}

void interrupt_polling() {
/* mode 1: set cpuPC to 0x38 */
}

void synchronize(uint_fast8_t x) {

}

uint8_t parcalc(uint8_t val){
	uint8_t par = ((val>>7)&1)+((val>>6)&1)+((val>>5)&1)+((val>>4)&1)+((val>>3)&1)+((val>>2)&1)+((val>>1)&1)+(val&1);
	return par;
}
