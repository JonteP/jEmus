#include "z80.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>	/* memcpy */
#include "smsemu.h"
#include "cartridge.h"
#include "vdp.h"
#include "my_sdl.h"


static uint_fast8_t ctable[] = {
 /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	4,10, 7, 6, 4, 4, 7, 4, 4,11, 7, 6, 4, 4, 7, 4,/* 0 */
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

static uint_fast8_t ccbtable[] = {
 /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* 0 */
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
static uint_fast8_t cddtable[] = {
 /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	4,99,99,99,99,99,99,99,99,15,99,99,99,99,99, 4,/* 0 */
	4,99,99,99,99,99,99,99,99,15,99,99,99,99,99, 4,/* 1 */
	4,14,20,10, 8, 8,11,99,99,15,20,10, 8, 8,11, 4,/* 2 */
	4,99,99,99,23,23,19,99,99,15,99,99,99,99,99, 4,/* 3 */
	4, 4, 4, 4, 8, 8,19, 4, 4, 4, 4, 4, 8, 8,19, 4,/* 4 */
	4, 4, 4, 4, 8, 8,19, 4, 4, 4, 4, 4, 8, 8,19, 4,/* 5 */
	8, 8, 8, 8, 8, 8,19, 8, 8, 8, 8, 8, 8, 8,19, 8,/* 6 */
   19,19,19,19,19,19,99,19, 4, 4, 4, 4, 8, 8,19, 4,/* 7 */
	4,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19, 4,/* 8 */
	4,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19, 4,/* 9 */
	4,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19, 4,/* a */
	4,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19, 4,/* b */
	4,99,99,99,99,99,99,99,99,99,99, 0,99,99,99, 4,/* c */
	4,99,99,99,99,99,99,99,99,99,99,99,99, 0,99, 4,/* d */
	4,14,99,23,99,15,99,99,99, 8,99,99,99, 0,99, 4,/* e */
	4,99,99,99,99,99,99,99,99,10,99,99,99, 0,99, 4,/* f */
};
static uint_fast8_t cedtable[] = {
  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 0 */
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

static uint_fast8_t cfdtable[] = {
  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	99,99,99,99,99,99,99,99,99,15,99,99,99,99,99,04,/* 0 */
	99,99,99,99,99,99,99,99,99,15,99,99,99,99,99,04,/* 1 */
	99,14,20,10, 8, 8,11,99,99,15,20,10, 8, 8,11,04,/* 2 */
	99,99,99,99,23,23,19,99,99,15,99,99,99,99,99,04,/* 3 */
	 4, 4, 4, 4, 8, 8,19, 4, 4, 4, 4, 4, 8, 8,19, 4,/* 4 */
	 4, 4, 4, 4, 8, 8,19, 4, 4, 4, 4, 4, 8, 8,19, 4,/* 5 */
	 8, 8, 8, 8, 8, 8,19, 8, 8, 8, 8, 8, 8, 8,19, 8,/* 6 */
	19,19,19,19,19,19, 4,19, 4, 4, 4, 4, 8, 8,19, 4,/* 7 */
	99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,04,/* 8 */
	99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,04,/* 9 */
	99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,04,/* a */
	99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,04,/* b */
	99,99,99,99,99,99,99,99,99,99,99, 0,04,04,04,04,/* c */
	99,99,99,99,99,99,99,99,99,99,99,99,04,04,04,04,/* d */
	99,14,99,23,99,15,99,99,99, 8,99,99,04,04,04,04,/* e */
	99,99,99,99,99,99,99,99,99,10,99,99,04,04,04,04,/* f */
};

static uint_fast8_t cddcbtable[] = {
  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 0 */
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

static uint_fast8_t cfdcbtable[] = {
  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 0 */
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

FILE *logfile;
static inline void write_cpu_register(uint8_t, uint_fast8_t), cpuwrite(uint16_t, uint_fast8_t), interrupt_polling(), addcycles(uint8_t), noop(), unp();
static inline uint8_t read_cpu_register(uint8_t), parcalc(uint8_t);
static inline uint8_t * cpuread(uint16_t);
/* 8-bit load */
static inline void ld(), ldim(), ldixi(), ldiyi(), ldiix(), ldiiy(), ldixn(), ldiyn(), ldabc(), ldade(), ldai(), ldbca(), lddea(), ldia(), ldain(), ldar(), ldina(), ldra(), lhlim(), ldhlr();
/* 16-bit load */
static inline void ld16(), ldix(), ldiy(), ldhli(), ldrp(), ldxin(), ldyin(), ldihl(), ldidd(), linix(), liniy(), lsphl(), lspix(), lspiy(), push(), pusix(), pusiy(), pop(), popix(), popiy();
/* exchange, block transfer, search */
static inline void ex(), exaf(), exx(), exhl(), exix(), exiy(), ldi(), ldir(), ldd(), lddr(), cpi(), cpir(), cpd(), cpdr();
/* 8-bit arithmetic */
static inline void add(uint8_t), addr(), addi(), adixh(), adixl(), adiyh(), adiyl(), adixi(), adiyi(), adc(uint8_t), adcr(), adci(), acixh(), acixl(), aciyh(), aciyl(), acixi(), aciyi(), sub(uint8_t), subr(), subi(), sbixh(), sbixl(), sbiyh(), sbiyl(), sbixi(), sbiyi(), sbc(uint8_t), sbcr(), sbci(), scixh(), scixl(), sciyh(), sciyl(), scixi(), sciyi(), inchl(), dechl();
static inline void and(uint8_t), andr(), andi(), anixh(), anixl(), aniyh(), aniyl(), anixi(), aniyi(), or(uint8_t), orr(), ori(), orixh(), orixl(), oriyh(), oriyl(), orixi(), oriyi(), xor(uint8_t), xorr(), xori(), xrixh(), xrixl(), xriyh(), xriyl(), xrixi(), xriyi(), cp(uint8_t), cpr(), cpn(), cpixh(), cpixl(), cpiyh(), cpiyl(), cpixi(), cpiyi(), inc(), inixi(), iniyi(), dec(), deixi(), deiyi();
/* general purpose */
static inline void daa(), cpl(), neg(), ccf(), scf(), nop(), halt(), di(), ei(), im();
/* 16-bit arithmetic */
static inline void add16(), adcrp(), sbc16(), addix(), addiy(), inc16(), incix(), inciy(), dec16(), decix(), deciy();
/* rotate, shift */
static inline void rlca(), rla(), rrca(), rra(), rlc(), rlcix(), rlciy(), rl(), rlix(), rliy(), rrc(), rrcix(), rrciy(), rr(), rrix(), rriy(), sla(), slaix(), slaiy(), sra(), sraix(), sraiy(), sll(), sllix(), slliy(), srl(), srlix(), srliy(), rld(), rrd();
/* bit set, reset, test */
static inline void bit(), bitix(), bitiy(), set(), setix(), setiy(), res(), resix(), resiy();
/* jump */
static inline void jp(), jpc(), jr(), jrc(), jphl(), jpix(), jpiy(), djnz();
/* call, return */
static inline void call(), callc(), ret(), retc(), reti(), retn(), rst();
/* input, output */
static inline void in(), inrc(), ini(), inir(), out(), outc(), outi(), otir(), outd(), otdr();
/* prefixed opcodes */
static inline void cb(), dd(), ed(), fd(), ddcb(), fdcb();
/* undocumented opcodes */
static inline void dcixh(), dciyh(), dcixl(), dciyl(), inixh(), iniyh(), inixl(), iniyl(), ldixh(), ldiyh(), ldixl(), ldiyl(), lrixh(), lrixl(), lriyh(), lriyl(), lixhr(), lixlr(), liyhr(), liylr();

int8_t displace;
uint8_t op, irqPulled = 0, nmiPulled = 0, intDelay = 0, halted = 0, reset = 0;
uint32_t cpucc = 0;

/* Internal registers */
uint16_t cpuAF, cpuAFx;
uint16_t cpuBC, cpuDE, cpuHL, cpuBCx, cpuDEx, cpuHLx; /* General purpose */
uint8_t cpuI, cpuR; /* special purpose (8-bit) */
uint16_t cpuIX, cpuIY, cpuPC, cpuSP; /* special purpose (16-bit) */
uint8_t *cpuAreg, *cpuFreg, *cpuBreg, *cpuCreg, *cpuDreg, *cpuEreg, *cpuHreg, *cpuLreg, *cpuIXhreg, *cpuIXlreg, *cpuIYhreg, *cpuIYlreg;
uint16_t *cpuAFreg, *cpuBCreg, *cpuDEreg, *cpuHLreg, *cpuIXreg, *cpuIYreg;

/* Interrupt flip-flops */
uint8_t iff1, iff2, iMode;

/* Vector pointers */
static const uint16_t nmi = 0x66, irq = 0x38;

/* Mapped memory  - should be moved to machine specific code */
uint_fast8_t cpuRam[0x2000];

char opmess[] = "Unimplemented opcode";
void opdecode() {

static void (*optable[0x100])() = {
  /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
	  nop, ld16,ldbca,inc16,  inc,  dec, ldim, rlca, exaf,add16,ldabc,dec16,  inc,  dec, ldim, rrca, /* 0 */
	 djnz, ld16,lddea,inc16,  inc,  dec, ldim,  rla,   jr,add16,ldade,dec16,  inc,  dec, ldim,  rra, /* 1 */
	  jrc, ld16,ldihl,inc16,  inc,  dec, ldim,  daa,  jrc,add16,ldhli,dec16,  inc,  dec, ldim,  cpl, /* 2 */
	  jrc, ld16, ldia,inc16,inchl,dechl,lhlim,  scf,  jrc,add16, ldai,dec16,  inc,  dec, ldim,  ccf, /* 3 */
	   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld, /* 4 */
	   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld, /* 5 */
	   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld, /* 6 */
	ldhlr,ldhlr,ldhlr,ldhlr,ldhlr,ldhlr, halt,ldhlr,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld, /* 7 */
	 addr, addr, addr, addr, addr, addr, addr, addr, adcr, adcr, adcr, adcr, adcr, adcr, adcr, adcr, /* 8 */
	 subr, subr, subr, subr, subr, subr, subr, subr, sbcr, sbcr, sbcr, sbcr, sbcr, sbcr, sbcr, sbcr, /* 9 */
	 andr, andr, andr, andr, andr, andr, andr, andr, xorr, xorr, xorr, xorr, xorr, xorr, xorr, xorr, /* a */
	  orr,  orr,  orr,  orr,  orr,  orr,  orr,  orr,  cpr,  cpr,  cpr,  cpr,  cpr,  cpr,  cpr,  cpr, /* b */
	 retc,  pop,  jpc,   jp,callc, push, addi,  rst, retc,  ret,  jpc,   cb,callc, call, adci,  rst, /* c */
	 retc,  pop,  jpc,  out,callc, push, subi,  rst, retc,  exx,  jpc,   in,callc,   dd, sbci,  rst, /* d */
	 retc,  pop,  jpc, exhl,callc, push, andi,  rst, retc, jphl,  jpc,   ex,callc,   ed, xori,  rst, /* e */
	 retc,  pop,  jpc,   di,callc, push,  ori,  rst, retc,lsphl,  jpc,   ei,callc,   fd,  cpn,  rst, /* f */
};
	if(reset){
		reset = 0;
		power_reset();
	}
	else if((irqPulled && iff1 && !intDelay) || nmiPulled){
		if(halted){
			halted = 0;
			cpuPC++;
		}
		/* TODO: this is assuming Mode 1 */
		cpuwrite(--cpuSP, ((cpuPC & 0xff00) >> 8));
		cpuwrite(--cpuSP, ( cpuPC & 0x00ff));
		if (irqPulled){
			iff1 = iff2 = 0;
			addcycles(13);
			irqPulled = 0;
			cpuPC = irq;
		}
		else if (nmiPulled){
			iff2 = iff1;
			iff1 = 0;
			addcycles(11);
			nmiPulled = 0;
			cpuPC = nmi;
		}
	}
	else {
		intDelay = 0;
		op = *cpuread(cpuPC++);
		/* TODO: update memory refresh register */
		/*	printf("%04X: %02X\n",cpuPC-1,op);*/
	/*	if(cpuPC==0xc41c)
			fprintf(logfile,"%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x\n",cpuPC-1,*cpuAFreg,*cpuBCreg,*cpuDEreg,*cpuHLreg,*cpuIXreg,*cpuIYreg,cpuSP);*/
		addcycles(ctable[op]);
		(*optable[op])();
	}
	run_vdp();
}

/* EXTENDED OPCODE TABLES */
void cb(){
static void (*cbtable[0x100])() = {
/*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
	rlc,  rlc,  rlc,  rlc,  rlc,  rlc,  rlc,  rlc,  rrc,  rrc,  rrc,  rrc,  rrc,  rrc,  rrc,  rrc, /* 0 */
	 rl,   rl,   rl,   rl,   rl,   rl,   rl,   rl,   rr,   rr,   rr,   rr,   rr,   rr,   rr,   rr, /* 1 */
	sla,  sla,  sla,  sla,  sla,  sla,  sla,  sla,  sra,  sra,  sra,  sra,  sra,  sra,  sra,  sra, /* 2 */
	sll,  sll,  sll,  sll,  sll,  sll,  sll,  sll,  srl,  srl,  srl,  srl,  srl,  srl,  srl,  srl, /* 3 */
	bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit, /* 4 */
	bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit, /* 5 */
	bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit, /* 6 */
	bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit, /* 7 */
	res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res, /* 8 */
	res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res, /* 9 */
	res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res, /* a */
	res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res, /* b */
	set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set, /* c */
	set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set, /* d */
	set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set, /* e */
	set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set, /* f */
	};
op = *cpuread(cpuPC++);
addcycles(ccbtable[op]);
(*cbtable[op])();
}
void dd(){
static void (*ddtable[0x100])() = {
/*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
	unp, noop, noop, noop, noop, noop, noop, noop, noop,addix, noop, noop, noop, noop, noop, noop, /* 0 */
	unp, noop, noop, noop, noop, noop, noop, noop, noop,addix, noop, noop, noop, noop, noop, noop, /* 1 */
	unp, ldix,linix,incix,inixh,dcixh,ldixh, noop, noop,addix,ldxin,decix,inixl,dcixl,ldixl, noop, /* 2 */
	unp, noop, noop, noop,inixi,deixi,ldixn, noop, noop,addix, noop, noop, noop, noop, noop, noop, /* 3 */
	unp,  unp,  unp,  unp,lrixh,lrixl,ldixi,  unp,  unp,  unp,  unp,  unp,lrixh,lrixl,ldixi,  unp, /* 4 */
	unp,  unp,  unp,  unp,lrixh,lrixl,ldixi,  unp,  unp,  unp,  unp,  unp,lrixh,lrixl,ldixi,  unp, /* 5 */
  lixhr,lixhr,lixhr,lixhr,lrixh,lrixl,ldixi,lixhr,lixlr,lixlr,lixlr,lixlr,lrixh,lrixl,ldixi,lixlr, /* 6 */
  ldiix,ldiix,ldiix,ldiix,ldiix,ldiix,  unp,ldiix,  unp,  unp,  unp,  unp,lrixh,lrixl,ldixi,  unp, /* 7 */
	unp, noop, noop, noop,adixh,adixh,adixi, noop, noop, noop, noop, noop,acixh,acixl,acixi, noop, /* 8 */
	unp, noop, noop, noop,sbixh,sbixl,sbixi, noop, noop, noop, noop, noop,scixh,scixl,scixi, noop, /* 9 */
	unp, noop, noop, noop,anixh,anixl,anixi, noop, noop, noop, noop, noop,xrixh,xrixl,xrixi, noop, /* a */
	unp, noop, noop, noop,orixh,orixl,orixi, noop, noop, noop, noop, noop,cpixh,cpixl,cpixi, noop, /* b */
	unp, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, ddcb, noop, noop, noop, noop, /* c */
	unp, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop,  nop, noop, noop, /* d */
	unp,popix, noop, exix, noop,pusix, noop, noop, noop, jpix, noop, noop, noop,  nop, noop, noop, /* e */
	unp, noop, noop, noop, noop, noop, noop, noop, noop,lspix, noop, noop, noop,  nop, noop, noop, /* f */
	};
op = *cpuread(cpuPC++);
addcycles(cddtable[op]);
(*ddtable[op])();
}
void ed(){
static void (*edtable[0x100])() = {
 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 0 */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 1 */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 2 */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 3 */
	inrc, outc,sbc16,ldidd,  neg, retn,   im,ldina, inrc, outc,adcrp, ldrp,  neg, reti, noop, ldra, /* 4 */
	inrc, outc,sbc16,ldidd,  neg, noop,   im,ldain, inrc, outc,adcrp, ldrp,  neg, noop, noop, ldar, /* 5 */
	inrc, outc,sbc16,ldidd,  neg, noop,   im,  rrd, inrc, outc,adcrp, ldrp,  neg, noop, noop,  rld, /* 6 */
	inrc, outc,sbc16,ldidd,  neg, noop,   im, noop, inrc, outc,adcrp, ldrp,  neg, noop, noop, noop, /* 7 */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 8 */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 9 */
	 ldi,  cpi,  ini, outi, noop, noop, noop, noop,  ldd,  cpd, noop, outd, noop, noop, noop, noop, /* a */
	ldir, cpir, inir, otir, noop, noop, noop, noop, lddr, cpdr, noop, otdr, noop, noop, noop, noop, /* b */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* c */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* d */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* e */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* f */
	};
op = *cpuread(cpuPC++);
addcycles(cedtable[op]);
(*edtable[op])();
}
void fd(){
static void (*fdtable[0x100])() = {
 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
	noop, noop, noop, noop, noop, noop, noop, noop, noop,addiy, noop, noop, noop, noop, noop, unp, /* 0 */
	noop, noop, noop, noop, noop, noop, noop, noop, noop,addiy, noop, noop, noop, noop, noop, unp, /* 1 */
	noop, ldiy,liniy,inciy,iniyh,dciyh,ldiyh, noop, noop,addiy,ldyin,deciy,iniyl,dciyl,ldiyl, unp, /* 2 */
	noop, noop, noop, noop,iniyi,deiyi,ldiyn, noop, noop,addiy, noop, noop, noop, noop, noop, unp, /* 3 */
	 unp,  unp,  unp,  unp,lriyh,lriyl,ldiyi,  unp,  unp,  unp,  unp,  unp,lriyh,lriyl,ldiyi,  unp, /* 4 */
	 unp,  unp,  unp,  unp,lriyh,lriyl,ldiyi,  unp,  unp,  unp,  unp,  unp,lriyh,lriyl,ldiyi,  unp, /* 5 */
   liyhr,liyhr,liyhr,liyhr,liyhr,liyhr,ldiyi,liyhr,liylr,liylr,liylr,liylr,liylr,liylr,ldiyi,liylr, /* 6 */
   ldiiy,ldiiy,ldiiy,ldiiy,ldiiy,ldiiy,  unp,ldiiy,  unp,  unp,  unp,  unp,lriyh,lriyl,ldiyi,  unp, /* 7 */
    noop, noop, noop, noop,adiyh,adiyl,adiyi, noop, noop, noop, noop, noop,aciyh,aciyl,aciyi, unp, /* 8 */
	noop, noop, noop, noop,sbiyh,sbiyl,sbiyi, noop, noop, noop, noop, noop,sciyh,sciyl,sciyi, unp, /* 9 */
	noop, noop, noop, noop,aniyh,aniyl,aniyi, noop, noop, noop, noop, noop,xriyh,xriyl,xriyi, unp, /* a */
	noop, noop, noop, noop,oriyh,oriyl,oriyi, noop, noop, noop, noop, noop,cpiyh,cpiyl,cpiyi, unp, /* b */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, fdcb, unp, unp, unp, unp, /* c */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, unp, unp, unp, unp, /* d */
	noop,popiy, noop, exiy, noop,pusiy, noop, noop, noop, jpiy, noop, noop, unp, unp, unp, unp, /* e */
	noop, noop, noop, noop, noop, noop, noop, noop, noop,lspiy, noop, noop, unp, unp, unp, unp, /* f */
	};
op = *cpuread(cpuPC++);
addcycles(cfdtable[op]);
(*fdtable[op])();
}

void ddcb(){ 	 	 	 	 	 	 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
  static void (*ddcbtable[0x100])() = { noop, noop, noop, noop, noop, noop,rlcix, noop, noop, noop, noop, noop, noop, noop,rrcix, noop, /* 0 */
									    noop, noop, noop, noop, noop, noop, rlix, noop, noop, noop, noop, noop, noop, noop, rrix, noop, /* 1 */
										noop, noop, noop, noop, noop, noop,slaix, noop, noop, noop, noop, noop, noop, noop,sraix, noop, /* 2 */
										noop, noop, noop, noop, noop, noop,sllix, noop, noop, noop, noop, noop, noop, noop,srlix, noop, /* 3 */
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
	displace = *cpuread(cpuPC++);
	op = *cpuread(cpuPC++);
	addcycles(cddcbtable[op]);
	(*ddcbtable[op])();
}
void fdcb(){ 	 	 	 	 	 	 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
  static void (*fdcbtable[0x100])() = { noop, noop, noop, noop, noop, noop,rlciy, noop, noop, noop, noop, noop, noop, noop,rrciy, noop, /* 0 */
									    noop, noop, noop, noop, noop, noop, rliy, noop, noop, noop, noop, noop, noop, noop, rriy, noop, /* 1 */
										noop, noop, noop, noop, noop, noop,slaiy, noop, noop, noop, noop, noop, noop, noop,sraiy, noop, /* 2 */
										noop, noop, noop, noop, noop, noop,slliy, noop, noop, noop, noop, noop, noop, noop,srliy, noop, /* 3 */
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
	displace = *cpuread(cpuPC++);
	op = *cpuread(cpuPC++);
	addcycles(cfdcbtable[op]);
	(*fdcbtable[op])();
}

/***********/
/* OPCODES */
/***********/

/* 8-BIT LOAD GROUP */

void ld()	{ /* LD r,r' */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*r[(op>>3) & 7] = *r[op & 7];
}
void ldim()	{ /* LD r,n */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*r[(op>>3) & 7] = *cpuread(cpuPC++);
}
void ldhlr(){ /* LD (HL),r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	cpuwrite(*cpuHLreg, *r[op & 7]);
}
void lhlim(){ /* LD (HL),n */
	cpuwrite(*cpuHLreg, *cpuread(cpuPC++));
}
void ldixi(){ /* LD r,(IX+d) */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*r[(op >> 3) & 7] = *cpuread(*cpuIXreg + (int8_t)*cpuread(cpuPC++));
}
void ldiyi(){ /* LD r,(IY+d) */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*r[(op >> 3) & 7] = *cpuread(*cpuIYreg + (int8_t)*cpuread(cpuPC++));
}
void ldiix(){ /* LD (IX+d),r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	cpuwrite(*cpuIXreg + (int8_t)*cpuread(cpuPC++), *r[op & 7]);
}
void ldiiy(){ /* LD (IY+d),r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	cpuwrite(*cpuIYreg + (int8_t)*cpuread(cpuPC++), *r[op & 7]);
}
void ldixn(){ /* LD (IX+d),n */
	int8_t offset = *cpuread(cpuPC++);
	uint8_t val = *cpuread(cpuPC++);
	cpuwrite(*cpuIXreg + offset, val);
}
void ldiyn(){ /* LD (IY+d),n */
	int8_t offset = *cpuread(cpuPC++);
	uint8_t val = *cpuread(cpuPC++);
	cpuwrite(*cpuIYreg + offset, val);
}
void ldabc(){ /* LD A,(BC) */
	*cpuAreg = *cpuread(*cpuBCreg);
}
void ldade(){ /* LD A,(DE) */
	*cpuAreg = *cpuread(*cpuDEreg);
}
void ldai(){ /* LD A,(nn) */
	uint16_t address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	*cpuAreg = *cpuread(address);
}
void ldbca(){ /* LD (BC),A */
	cpuwrite(*cpuBCreg, *cpuAreg);
}
void lddea(){ /* LD (DE),A */
	cpuwrite(*cpuDEreg, *cpuAreg);
}
void ldia()	{ /* LD (nn),A */
	uint16_t address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	cpuwrite(address, *cpuAreg);
}
void ldain(){ /* LD A,I */
	*cpuAreg = cpuI;
}
void ldar()	{ /* LD A,R */
	*cpuAreg = cpuR;
}
void ldina(){ /* LD I,A */
	cpuI = *cpuAreg;
}
void ldra()	{ /* LD R,A */
	 cpuR = *cpuAreg;
}


/* 16-BIT LOAD GROUP */

void ld16()	{ /* LD dd,nn */
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP}, tmp;
	tmp = *cpuread(cpuPC++);
	tmp |= ((*cpuread(cpuPC++)) << 8);
	*rp[((op>>4) & 3)] = tmp;
}
void ldix()	{ /* LD IX,nn */
	uint16_t tmp = *cpuread(cpuPC++);
	tmp |= ((*cpuread(cpuPC++)) << 8);
	*cpuIXreg = tmp;
}
void ldiy()	{ /* LD IY,nn */
	uint16_t tmp = *cpuread(cpuPC++);
	tmp |= ((*cpuread(cpuPC++)) << 8);
	*cpuIYreg = tmp;
}
void ldhli(){ /* LD HL,(nn) */
	uint16_t address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	*cpuLreg = *cpuread(address++);
	*cpuHreg = *cpuread(address);
}
void ldrp()	{ /* LD dd,(nn) */
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP};
	uint16_t address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	*rp[(op >> 4) & 0x03] = *cpuread(address++);
	*rp[(op >> 4) & 0x03] |= (*cpuread(address) << 8);
}
void ldxin(){ /* LD IX,(nn) */
	uint16_t address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	*cpuIXlreg = *cpuread(address++);
	*cpuIXhreg = *cpuread(address);
}
void ldyin(){ /* LD IY,(nn) */
	uint16_t address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	*cpuIYlreg = *cpuread(address++);
	*cpuIYhreg = *cpuread(address);
}
void ldihl(){ /* LD (nn),HL */
	uint16_t address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	cpuwrite(address++, *cpuLreg);
	cpuwrite(address, *cpuHreg);
}
void ldidd(){ /* LD (nn),dd */
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP}, address;
	address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	cpuwrite(address++, (*rp[(op >> 4) & 0x03] & 0xff));
	cpuwrite(address, (*rp[(op >> 4) & 0x03] >> 8));
}
void linix(){ /* LD (nn),IX */
	uint16_t address = *cpuread(cpuPC++);
	address |= (*cpuread(cpuPC++) << 8);
	cpuwrite(address++, *cpuIXlreg);
	cpuwrite(address, *cpuIXhreg);
}
void liniy(){ /* LD (nn),IY */
	uint16_t address = *cpuread(cpuPC++);
	address |= (*cpuread(cpuPC++) << 8);
	cpuwrite(address++, *cpuIYlreg);
	cpuwrite(address, *cpuIYhreg);
}
void lsphl(){ /* LD SP,HL */
	cpuSP = *cpuHLreg;
}
void lspix(){ /* LD SP,IX */
	cpuSP = *cpuIXreg;
}
void lspiy(){ /* LD SP,IY */
	cpuSP = *cpuIYreg;
}
void push()	{ /* PUSH qq */
	uint16_t *rp2[4] = {cpuBCreg, cpuDEreg, cpuHLreg, cpuAFreg}, tmp;
	tmp = *rp2[(op>>4) & 3];
	cpuwrite(--cpuSP, ((tmp & 0xff00) >> 8));
	cpuwrite(--cpuSP, ( tmp & 0x00ff));
}
void pusix(){ /* PUSH IX */
	cpuwrite(--cpuSP, *cpuIXhreg);
	cpuwrite(--cpuSP, *cpuIXlreg);
}
void pusiy(){ /* PUSH IY */
	cpuwrite(--cpuSP, *cpuIYhreg);
	cpuwrite(--cpuSP, *cpuIYlreg);
}
void pop()	{ /* POP qq */
	uint16_t *rp2[4] = {cpuBCreg, cpuDEreg, cpuHLreg, cpuAFreg}, tmp;
	tmp = *cpuread(cpuSP++);
	tmp |= ((*cpuread(cpuSP++)) << 8);
	*rp2[(op>>4) & 3] = tmp;
}
void popix(){ /* POP IX */
	uint16_t tmp = *cpuread(cpuSP++);
	tmp |= ((*cpuread(cpuSP++)) << 8);
	*cpuIXreg = tmp;
}
void popiy(){ /* POP IY */
	uint16_t tmp = *cpuread(cpuSP++);
	tmp |= ((*cpuread(cpuSP++)) << 8);
	*cpuIYreg = tmp;
}


/* EXCHANGE, BLOCK TRANSFER, SEARCH GROUP */

void ex()	{ /* EX DE,HL */
	uint16_t tmp = cpuDE;
	cpuDE = cpuHL;
	cpuHL = tmp;
}
void exaf()	{ /* EX AF,AF' */
	uint16_t tmp = cpuAF;
	cpuAF = cpuAFx;
	cpuAFx = tmp;
}
void exx()	{ /* EXX */
	uint16_t tmpBC = cpuBC, tmpDE = cpuDE, tmpHL = cpuHL;
	cpuBC = cpuBCx;
	cpuDE = cpuDEx;
	cpuHL = cpuHLx;
	cpuBCx = tmpBC;
	cpuDEx = tmpDE;
	cpuHLx = tmpHL;
}
void exhl()	{ /* EX (SP),HL */
	uint8_t tmp = *cpuread(cpuSP);
	uint8_t tmp2 = *cpuread(cpuSP+1);
	cpuwrite(cpuSP, *cpuLreg);
	cpuwrite(cpuSP+1, *cpuHreg);
	cpuHL = ((tmp2 << 8) | tmp);
}
void exix()	{ /* EX (SP),IX */
	uint8_t tmp = *cpuread(cpuSP);
	uint8_t tmp2 = *cpuread(cpuSP+1);
	cpuwrite(cpuSP, *cpuIXlreg);
	cpuwrite(cpuSP+1, *cpuIXhreg);
	cpuIX = ((tmp2 << 8) | tmp);
}
void exiy()	{ /* EX (SP),IY */
	uint8_t tmp = *cpuread(cpuSP);
	uint8_t tmp2 = *cpuread(cpuSP+1);
	cpuwrite(cpuSP, *cpuIYlreg);
	cpuwrite(cpuSP+1, *cpuIYhreg);
	cpuIY = ((tmp2 << 8) | tmp);
}
void ldi()	{ /* LDI */
	(*cpuBCreg)--;
	cpuwrite((*cpuDEreg)++, *cpuread((*cpuHLreg)++));
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg = (*cpuBCreg) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
}
void ldir()	{ /* LDIR */
	(*cpuBCreg)--;
	cpuwrite((*cpuDEreg)++, *cpuread((*cpuHLreg)++));
	if (*cpuBCreg){
		cpuPC -= 2;
		addcycles(5);
	}
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg = (*cpuBCreg) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
}
void ldd()	{ /* LDD */
		cpuwrite((*cpuDEreg)--, *cpuread((*cpuHLreg)--));
		(*cpuBCreg)--;
		*cpuFreg &= ~0x10; /* H flag */
		*cpuFreg = (*cpuBCreg) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
		*cpuFreg &= ~0x02; /* N flag */
}
void lddr()	{ /* LDDR */
	(*cpuBCreg)--;
	cpuwrite((*cpuDEreg)--, *cpuread((*cpuHLreg)--));
	if (*cpuBCreg){
		cpuPC -= 2;
		addcycles(5);
	}
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg = (*cpuBCreg) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
}
void cpi()	{ /* CPI */
	uint8_t val = *cpuread((*cpuHLreg)++);
	(*cpuBCreg)--;
	uint16_t res = *cpuAreg - val;
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!res) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((*cpuAreg & 0xf) < (val & 0xf)) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuBCreg) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
}
void cpir()	{ /* CPIR */
	uint8_t val = *cpuread((*cpuHLreg)++);
	(*cpuBCreg)--;
	uint16_t res = *cpuAreg - val;
	if (*cpuBCreg && res){
		cpuPC -= 2;
		addcycles(5);
	}
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!res) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((*cpuAreg & 0xf) < (val & 0xf)) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuBCreg) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
}
void cpd()	{ /* CPD */
	uint8_t val = *cpuread((*cpuHLreg)--);
	(*cpuBCreg)--;
	uint16_t res = *cpuAreg - val;
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!res) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((*cpuAreg & 0xf) < (val & 0xf)) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuBCreg) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
}
void cpdr()	{ /* CPDR */
	uint8_t val = *cpuread((*cpuHLreg)--);
	(*cpuBCreg)--;
	uint16_t res = *cpuAreg - val;
	if (*cpuBCreg && res){
		cpuPC -= 2;
		addcycles(5);
	}
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!res) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((*cpuAreg & 0xf) < (val & 0xf)) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuBCreg) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
}


/* 8-BIT ARITHMETIC GROUP */

void add(uint8_t value)	{
	uint16_t res = *cpuAreg + value;
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((*cpuAreg ^ + value ^ res) & 0x10)  ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (value ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg = (res & 0x100) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}
void addr()	{ /* ADD A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	add(*r[op & 7]);
}
void addi()	{ /* ADD A,n */
	add(*cpuread(cpuPC++));
}
void adixi(){ /* ADD A,(IX+d) */
	int8_t tmp = *cpuread(cpuPC++);
	add(*cpuread(*cpuIXreg + tmp));
}
void adiyi(){ /* ADD A,(IY+d) */
	int8_t tmp = *cpuread(cpuPC++);
	add(*cpuread(*cpuIYreg + tmp));
}
void adc(uint8_t value)	{
	uint16_t res = *cpuAreg + value + (*cpuFreg & 0x01);
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((*cpuAreg ^ + value ^ res) & 0x10)  ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ res) & (value ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg = (res & 0x100) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}
void adcr()	{ /* ADC A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	adc(*r[op & 7]);
}
void adci()	{ /* ADC A,n */
	adc(*cpuread(cpuPC++));
}
void acixi(){ /* ADC A,(IX+d) */
	int8_t tmp = *cpuread(cpuPC++);
	adc(*cpuread(*cpuIXreg + tmp));
}
void aciyi(){ /* ADC A,(IY+d) */
	int8_t tmp = *cpuread(cpuPC++);
	adc(*cpuread(*cpuIYreg + tmp));
}
void sub(uint8_t value)	{
	uint16_t res = *cpuAreg - value;
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((*cpuAreg ^ value ^ res) & 0x10) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ value) & (*cpuAreg ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
	*cpuFreg = (res & 0x100) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}
void subr()	{ /* SUB A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	sub(*r[op & 7]);
}
void subi()	{ /* SUB A,n */
	sub(*cpuread(cpuPC++));
}
void sbixi(){ /* SUB A,(IX+d) */
	int8_t tmp = *cpuread(cpuPC++);
	sub(*cpuread(*cpuIXreg + tmp));
}
void sbiyi(){ /* SUB A,(IY+d) */
	int8_t tmp = *cpuread(cpuPC++);
	sub(*cpuread(*cpuIYreg + tmp));
}
void sbc(uint8_t value)	{
	uint16_t res = *cpuAreg - value - (*cpuFreg & 0x01);
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((*cpuAreg ^ value ^ res) & 0x10) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ value) & (*cpuAreg ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
	*cpuFreg = (res & 0x100) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = res;
}
void sbcr()	{ /* SBC A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	sbc(*r[op & 7]);
}
void sbci()	{ /* SBC A,n */
	sbc(*cpuread(cpuPC++));
}
void scixi(){ /* SBC A,(IX+d) */
	int8_t tmp = *cpuread(cpuPC++);
	sbc(*cpuread(*cpuIXreg + tmp));
}
void sciyi(){ /* SBC A,(IY+d) */
	int8_t tmp = *cpuread(cpuPC++);
	sbc(*cpuread(*cpuIYreg + tmp));
}
void and(uint8_t value)	{
	*cpuAreg &= value;
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg |= 0x10; /* H flag */
	*cpuFreg = parcalc(*cpuAreg) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg &= ~0x01; /* C flag */
}
void andr()	{ /* AND A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	and(*r[op & 7]);
}
void andi()	{ /* AND A,n */
	and(*cpuread(cpuPC++));
}
void anixi(){ /* AND A,(IX+d) */
	int8_t tmp = *cpuread(cpuPC++);
	and(*cpuread(*cpuIXreg + tmp));
}
void aniyi(){ /* AND A,(IY+d) */
	int8_t tmp = *cpuread(cpuPC++);
	and(*cpuread(*cpuIYreg + tmp));
}
void or(uint8_t value){
	*cpuAreg |= value;
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg = parcalc(*cpuAreg) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg &= ~0x01; /* C flag */
}
void orr()	{ /* OR A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	or(*r[op & 7]);
}
void ori()	{ /* OR A,n */
	or(*cpuread(cpuPC++));
}
void orixi(){ /* OR A,(IX+d) */
	int8_t tmp = *cpuread(cpuPC++);
	or(*cpuread(*cpuIXreg + tmp));
}
void oriyi(){ /* OR A,(IY+d) */
	int8_t tmp = *cpuread(cpuPC++);
	or(*cpuread(*cpuIYreg + tmp));
}
void xor(uint8_t value)	{
	*cpuAreg ^= value;
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg = (parcalc(*cpuAreg)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg &= ~0x01; /* C flag */
}
void xorr()	{ /* XOR A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	xor(*r[op & 7]);
}
void xori()	{ /* XOR A,n */
	xor(*cpuread(cpuPC++));
}
void xrixi(){ /* XOR A,(IX+d) */
	int8_t tmp = *cpuread(cpuPC++);
	xor(*cpuread(*cpuIXreg + tmp));
}
void xriyi(){ /* XOR A,(IY+d) */
	int8_t tmp = *cpuread(cpuPC++);
	xor(*cpuread(*cpuIYreg + tmp));
}
void cp(uint8_t value)	{
	uint16_t res = *cpuAreg - value;
	*cpuFreg = (res & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((*cpuAreg ^ value ^ res) & 0x10) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuAreg ^ value) & (*cpuAreg ^ res) & 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
	*cpuFreg = (res & 0x100) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void cpr()	{ /* CP A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	cp(*r[op & 7]);
}
void cpn()	{ /* CP A,n */
	cp(*cpuread(cpuPC++));
}
void cpixi(){ /* CP A,(IX+d) */
	int8_t tmp = *cpuread(cpuPC++);
	cp(*cpuread(*cpuIXreg + tmp));
}
void cpiyi(){ /* CP A,(IY+d) */
	int8_t tmp = *cpuread(cpuPC++);
	cp(*cpuread(*cpuIYreg + tmp));
}
void inc()	{ /* INC r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg}, tmp;
	tmp = (*r[(op>>3) & 7]);
	(*r[(op>>3) & 7])++;
	*cpuFreg = (*r[(op>>3) & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[(op>>3) & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((tmp & 0x0f) == 0x0f) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x7f) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
}
void inchl(){ /* INC (HL) */
	uint8_t tmp = *cpuread(*cpuHLreg);
	uint8_t val = tmp + 1;
	cpuwrite(*cpuHLreg, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((tmp & 0x0f) == 0x0f) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x7f) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
}
void inixi(){ /* INC (IX+d) */
	int8_t val = *cpuread(cpuPC++);
	uint8_t tmp = *cpuread(*cpuIXreg + val);
	cpuwrite(*cpuIXreg + val, tmp + 1);
	*cpuFreg = ((tmp + 1) & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (tmp == 0xff) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((tmp & 0x0f) == 0x0f) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x7f) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
}
void iniyi(){ /* INC (IY+d) */
	int8_t val = *cpuread(cpuPC++);
	uint8_t tmp = *cpuread(*cpuIYreg + val);
	cpuwrite(*cpuIYreg + val, tmp + 1);
	*cpuFreg = ((tmp + 1) & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (tmp == 0xff) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((tmp & 0x0f) == 0x0f) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x7f) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
}
void dec()	{ /* DEC r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg}, tmp;
	tmp = (*r[(op>>3) & 7]);
	(*r[(op>>3) & 7])--;
	*cpuFreg = (*r[(op>>3) & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[(op>>3) & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (!(tmp & 0x0f)) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
}
void dechl(){ /* DEC (HL) */
	uint8_t tmp = *cpuread(*cpuHLreg);
	uint8_t val = tmp - 1;
	cpuwrite(*cpuHLreg, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (!(tmp & 0x0f)) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
}
void deixi(){ /* DEC (IX+d) */
	int8_t val = *cpuread(cpuPC++);
	uint8_t tmp = *cpuread(*cpuIXreg + val);
	cpuwrite(*cpuIXreg + val, tmp - 1);
	*cpuFreg = ((tmp - 1) & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (tmp == 0x01) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((tmp & 0x0f) == 0x00) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
}
void deiyi(){ /* DEC (IY+d) */
	int8_t val = *cpuread(cpuPC++);
	uint8_t tmp = *cpuread(*cpuIYreg + val);
	cpuwrite(*cpuIYreg + val, tmp - 1);
	*cpuFreg = ((tmp - 1) & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (tmp == 0x01) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((tmp & 0x0f) == 0x00) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
}


/* GENERAL PURPOSE OPCODES */

void daa()	{ /* DAA */
	uint8_t tmp = *cpuAreg;
		if((*cpuFreg & 0x10) || ((*cpuAreg & 0x0f) > 9))
			tmp += (*cpuFreg & 0x02) ? -0x06 : 0x06;
		if((*cpuFreg & 0x01) || (*cpuAreg > 0x99))
			tmp += (*cpuFreg & 0x02) ? -0x60 : 0x60;
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!tmp) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((tmp ^ *cpuAreg) & 0x10) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (parcalc(tmp)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = ((*cpuFreg & 0x01) | (*cpuAreg > 0x99)) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuAreg = tmp;
}
void cpl()	{ /* CPL */
	*cpuAreg ^= 0xff;
	*cpuFreg |= 0x10; /* H flag */
	*cpuFreg |= 0x02; /* N flag */
}
void neg()	{ /* NEG */
	uint8_t tmp = *cpuAreg;
	*cpuAreg = (0 - *cpuAreg);
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (tmp & 0xf) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
	*cpuFreg = (tmp) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void ccf()	{ /* CCF */
	*cpuFreg = (*cpuFreg & 0x01) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg ^= 0x01; /* C flag */
}
void scf()	{ /* SCF */
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg |= 0x01; /* C flag */
}
void nop()	{ /* NOP */
}
void halt()	{ /* HALT */
	halted = 1;
	cpuPC--;
}
void di() 	{ /* DI */
	iff1 = iff2 = 0;
	intDelay = 1;
}
void ei() 	{ /* EI */
	iff1 = iff2 = 1;
	intDelay = 1;
}
void im()	{ /* IM */
	uint8_t im[8] = {0, 0, 1, 2, 0, 0, 1, 2};
	iMode = im[(op>>3) & 7];
	if (iMode != 1){
		printf("Unsupported interrupt mode: %i\n",iMode);
	}
}


/* 16-BIT ARITHMETIC GROUP */

void add16(){ /* ADD HL,ss */
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP};
	uint32_t res = *cpuHLreg + *rp[(op >> 4) & 0x03];
	*cpuFreg = (((*cpuHLreg & 0xfff) + (*rp[(op >> 4) & 0x03] & 0xfff)) > 0xfff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg = (res > 0xffff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuHLreg = res;
}
void adcrp(){ /* ADC HL,ss */
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP};
	uint32_t res = *cpuHLreg + *rp[(op >> 4) & 0x03] + (*cpuFreg & 0x01);
	*cpuFreg = (res & 0x8000) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xffff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((*cpuHLreg ^ *rp[(op >> 4) & 0x03] ^ res) & 0x1000) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuHLreg ^ res) & (*rp[(op >> 4) & 0x03] ^ res) & 0x8000) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg = (res & 0x10000) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuHLreg = res;
}
void sbc16(){ /* SBC HL,ss */
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP};
	uint32_t res = *cpuHLreg - *rp[(op >> 4) & 0x03] - (*cpuFreg & 0x01);
	*cpuFreg = (res & 0x8000) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!(res & 0xffff)) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((*cpuHLreg ^ *rp[(op >> 4) & 0x03] ^ res) & 0x1000) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = ((*cpuHLreg ^ *rp[(op >> 4) & 0x03]) & (*cpuHLreg ^ res) & 0x8000) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
	*cpuFreg = (res & 0x10000) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuHLreg = res;
}
void addix(){ /* ADD IX,pp */
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuIXreg, &cpuSP};
	uint32_t res = *cpuIXreg + *rp[(op >> 4) & 0x03];
	*cpuFreg = ((*cpuIXreg & 0xfff) + (*rp[(op >> 4) & 0x03] & 0xfff) > 0xfff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg = (res > 0xffff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuIXreg = res;
}
void addiy(){ /* ADD IY,rr */
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuIYreg, &cpuSP};
	uint32_t res = *cpuIYreg + *rp[(op >> 4) & 0x03];
	*cpuFreg = (((*cpuIYreg & 0xfff) + (*rp[(op >> 4) & 0x03] & 0xfff)) > 0xfff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg = (res > 0xffff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*cpuIYreg = res;
}
void inc16(){ /* INC ss */
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP};
	(*rp[(op>>4) & 3])++;
}
void incix(){ /* INC IX */
	(*cpuIXreg)++;
}
void inciy(){ /* INC IY */
	(*cpuIYreg)++;
}
void dec16(){ /* DEC ss */
	uint16_t *rp[4] = {cpuBCreg, cpuDEreg, cpuHLreg, &cpuSP};
	(*rp[(op>>4) & 3])--;
}
void decix(){ /* DEC IX */
	(*cpuIXreg)--;
}
void deciy(){ /* DEC IY */
	(*cpuIYreg)--;
}


/* ROTATE AND SHIFT GROUP */

void rlca()	{ /* RLCA */
	uint8_t tmp = *cpuAreg;
	*cpuAreg = ((*cpuAreg << 1) | (tmp >> 7));
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rla()	{ /* RLA */
	uint8_t tmp = *cpuAreg;
	*cpuAreg = ((*cpuAreg << 1) | (*cpuFreg & 0x01));
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rrca()	{ /* RRCA */
	uint8_t tmp = *cpuAreg;
	*cpuAreg = (*cpuAreg >> 1);
	*cpuAreg |= (tmp << 7);
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rra()	{ /* RRA */
	uint8_t tmp = *cpuAreg;
	*cpuAreg = ((*cpuAreg >> 1) | (*cpuFreg << 7));
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rlc()	{ /* RLC r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] << 1) | (tmp >> 7));
	*cpuFreg = (*r[op & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[op & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(*r[op & 7]) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rlcix(){ /* RLC (IX+d) */
	uint8_t val = *cpuread(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = ((val << 1) | (tmp >> 7));
	cpuwrite(*cpuIXreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rlciy(){ /* RLC (IY+d) */
	uint8_t val = *cpuread(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = ((val << 1) | (tmp >> 7));
	cpuwrite(*cpuIYreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rl()	{ /* RL r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] << 1) | (*cpuFreg & 0x01));
	*cpuFreg = (*r[op & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[op & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (parcalc(*r[op & 7])) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rlix()	{ /* RL (IX+d) */
	uint8_t val = *cpuread(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = ((val << 1) | (*cpuFreg & 0x01));
	cpuwrite(*cpuIXreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rliy()	{ /* RL (IY+d) */
	uint8_t val = *cpuread(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = ((val << 1) | (*cpuFreg & 0x01));
	cpuwrite(*cpuIYreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rrc()	{ /* RRC r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] >> 1) | (tmp << 7));
	*cpuFreg = (*r[op & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[op & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (parcalc(*r[op & 7])) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rrcix(){ /* RRC (IX+d) */
	uint8_t val = *cpuread(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = ((val >> 1) | (tmp << 7));
	cpuwrite(*cpuIXreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rrciy(){ /* RRC (IY+d) */
	uint8_t val = *cpuread(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = ((val >> 1) | (tmp << 7));
	cpuwrite(*cpuIYreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rr()	{ /* RR r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] >> 1) | (*cpuFreg << 7));
	*cpuFreg = (*r[op & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[op & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (parcalc(*r[op & 7])) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rrix()	{ /* RR (IX+d) */
	uint8_t val = *cpuread(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = ((val >> 1) | (*cpuFreg << 7));
	cpuwrite(*cpuIXreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rriy()	{ /* RR (IY+d) */
	uint8_t val = *cpuread(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = ((val >> 1) | (*cpuFreg << 7));
	cpuwrite(*cpuIYreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void sla()	{ /* SLA r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = (*r[op & 7] << 1);
	*cpuFreg = (*r[op & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[op & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg = (parcalc(*r[op & 7])) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void slaix(){ /* SLA (IX+d) */
	uint8_t val = *cpuread(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = (val << 1);
	cpuwrite(*cpuIXreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void slaiy(){ /* SLA (IY+d) */
	uint8_t val = *cpuread(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = (val << 1);
	cpuwrite(*cpuIYreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void sra()	{ /* SRA r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] >> 1) | (tmp & 0x80));
	*cpuFreg = (*r[op & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[op & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg = (parcalc(*r[op & 7])) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void sraix(){ /* SRA (IX+d) */
	uint8_t val = *cpuread(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = ((val >> 1) | (tmp & 0x80));
	cpuwrite(*cpuIXreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void sraiy(){ /* SRA (IY+d) */
	uint8_t val = *cpuread(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = ((val >> 1) | (tmp & 0x80));
	cpuwrite(*cpuIYreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void sll()	{ /* SLL r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] << 1) | 0x01);
	*cpuFreg = (*r[op & 7] & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*r[op & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg = (parcalc(*r[op & 7])) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void sllix(){ /* SLL (IX+d) */
	uint8_t val = *cpuread(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = ((val << 1) | 0x01);
	cpuwrite(*cpuIXreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void slliy(){ /* SLL (IY+d) */
	uint8_t val = *cpuread(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = ((val << 1) | 0x01);
	cpuwrite(*cpuIYreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void srl()	{ /* SRL r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*cpuFreg = (*r[op & 7] & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*r[op & 7] = (*r[op & 7] >> 1);
	*cpuFreg &= ~0x80; /* S flag */
	*cpuFreg = (!*r[op & 7]) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg = (parcalc(*r[op & 7])) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
}
void srlix(){ /* SRL (IX+d) */
	uint8_t val = *cpuread(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = (val >> 1);
	cpuwrite(*cpuIXreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void srliy(){ /* SRL (IY+d) */
	uint8_t val = *cpuread(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = (val >> 1);
	cpuwrite(*cpuIYreg + displace, val);
	*cpuFreg = (val & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = parcalc(val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (tmp & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void rld()	{ /* RLD */
	uint8_t tmp = *cpuAreg;
	uint8_t val = *cpuread(*cpuHLreg);
	*cpuAreg = ((*cpuAreg & 0xf0) | ((val & 0xf0) >> 4));
	val = (val << 4);
	val |= (tmp & 0x0f);
	cpuwrite(*cpuHLreg, val);
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (parcalc(*cpuAreg)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
}
void rrd()	{ /* RRD */
	uint8_t tmp = *cpuAreg;
	uint8_t val = *cpuread(*cpuHLreg);
	*cpuAreg = ((*cpuAreg & 0xf0) | (val & 0x0f));
	val = (val >> 4);
	val |= (tmp << 4);
	cpuwrite(*cpuHLreg, val);
	*cpuFreg = (*cpuAreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuAreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (parcalc(*cpuAreg)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
}


/* BIT SET, RESET, TEST GROUP */

void bit()	{ /* BIT b,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t val = *r[op & 7] & (1 << ((op >> 3) & 7));
	*cpuFreg = (val && ((op >> 3) & 7) == 7) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg |= 0x10; /* H flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
}
void bitix(){ /* BIT b,(IX+d) */
	uint8_t val = *cpuread(*cpuIXreg + displace);
	val = val & (1 << ((op >> 3) & 7));
	*cpuFreg = (val && ((op >> 3) & 7) == 7) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg |= 0x10; /* H flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
}
void bitiy(){ /* BIT b,(IY+d) */
	uint8_t val = *cpuread(*cpuIYreg + displace);
	val = val & (1 << ((op >> 3) & 7));
	*cpuFreg = (val && ((op >> 3) & 7) == 7) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg |= 0x10; /* H flag */
	*cpuFreg = (!val) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
}
void set()	{ /* SET b,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*r[op & 7] |= (1 << ((op >> 3) & 7));
}
void setix(){ /* SET b,(IX+d) */
	uint8_t tmp = *cpuread(*cpuIXreg + displace) | (1 << ((op >> 3) & 7));
	cpuwrite(*cpuIXreg + displace, tmp);
}
void setiy(){ /* SET b,(IY+d) */
	uint8_t tmp = *cpuread(*cpuIYreg + displace) | (1 << ((op >> 3) & 7));
	cpuwrite(*cpuIYreg + displace, tmp);
}
void res()	{ /* RES b,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	*r[op & 7] &= ~(1 << ((op >> 3) & 7));
}
void resix(){ /* RES b,(IX+d) */
	uint8_t tmp = *cpuread(*cpuIXreg + displace) & ~(1 << ((op >> 3) & 7));
	cpuwrite(*cpuIXreg + displace, tmp);
}
void resiy(){ /* RES b,(IY+d) */
	uint8_t tmp = *cpuread(*cpuIYreg + displace) & ~(1 << ((op >> 3) & 7));
	cpuwrite(*cpuIYreg + displace, tmp);
}


/* JUMP GROUP */

void jp()	{ /* JP nn */
	uint16_t address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	cpuPC = address;
}
void jpc()	{ /* JP cc,nn */
	uint8_t cc[8] = {!(*cpuFreg & 0x40), (*cpuFreg & 0x40), !(*cpuFreg & 0x01), (*cpuFreg & 0x01), !(*cpuFreg & 0x04), (*cpuFreg & 0x04), !(*cpuFreg & 0x80), (*cpuFreg & 0x80)};
	uint16_t address = *cpuread(cpuPC++);
	address |= ((*cpuread(cpuPC++)) << 8);
	if(cc[(op>>3) & 7])
		cpuPC = address;
}
void jr()	{ /* JR e */
	cpuPC += ((int8_t)*cpuread(cpuPC) + 1);
}
void jrc()	{ /* JR cc,e */
	uint8_t cc[4] = {!(*cpuFreg & 0x40), (*cpuFreg & 0x40), !(*cpuFreg & 0x01), (*cpuFreg & 0x01)};
	if(cc[((op>>3) & 7) - 4]) {
		jr();
		addcycles(5);
	}
	else
		cpuPC++;
}
void jphl()	{ /* JP (HL) */
	cpuPC = *cpuHLreg;
}
void jpix()	{ /* JP (IX) */
	cpuPC = *cpuIXreg;
}
void jpiy()	{ /* JP (IY) */
	cpuPC = *cpuIYreg;
}
void djnz()	{ /* DJNZ,e */
	(*cpuBreg)--;
	if(*cpuBreg) {
		cpuPC += ((int8_t)*cpuread(cpuPC) + 1);
		addcycles(5);
	}
	else
		cpuPC++;
}


/* CALL AND RETURN GROUP */

void call()	{ /* CALL nn */
	uint16_t address = *cpuread(cpuPC++);
	address |= (*cpuread(cpuPC++) << 8);
	cpuwrite(--cpuSP, ((cpuPC & 0xFF00) >> 8));
	cpuwrite(--cpuSP, (cpuPC & 0x00FF));
	cpuPC = address;
}
void callc(){ /* CALL cc,nn */
	uint8_t cc[8] = {!(*cpuFreg & 0x40), (*cpuFreg & 0x40), !(*cpuFreg & 0x01), (*cpuFreg & 0x01), !(*cpuFreg & 0x04), (*cpuFreg & 0x04), !(*cpuFreg & 0x80), (*cpuFreg & 0x80)};
	uint16_t address = *cpuread(cpuPC++);
	address |= (*cpuread(cpuPC++) << 8);
	if(cc[(op>>3) & 7]) {
		cpuwrite(--cpuSP, ((cpuPC & 0xFF00) >> 8));
		cpuwrite(--cpuSP, (cpuPC & 0x00FF));
		cpuPC = address;
		addcycles(7);
	}
}
void ret()	{ /* RET */
	uint16_t address = *cpuread(cpuSP++);
	address |= ((*cpuread(cpuSP++)) << 8);
	cpuPC = address;
}
void retc()	{ /* RET cc */
	uint8_t cc[8] = {!(*cpuFreg & 0x40), (*cpuFreg & 0x40), !(*cpuFreg & 0x01), (*cpuFreg & 0x01), !(*cpuFreg & 0x04), (*cpuFreg & 0x04), !(*cpuFreg & 0x80), (*cpuFreg & 0x80)};
	if (cc[(op >> 3) & 7]){
		ret();
		addcycles(6);
	}
}
void reti()	{ /* RETI */
	/* TODO: incomplete */
	uint16_t address = *cpuread(cpuSP++);
	address |= ((*cpuread(cpuSP++)) << 8);
	cpuPC = address;
}
void retn()	{ /* RETN */
	/* TODO: incomplete */
	uint16_t address = *cpuread(cpuSP++);
	address |= ((*cpuread(cpuSP++)) << 8);
	cpuPC = address;
	iff1 = iff2;
}
void rst()	{ /* RST */
	cpuwrite(--cpuSP, ((cpuPC & 0xff00) >> 8));
	cpuwrite(--cpuSP, ( cpuPC & 0x00ff));
	cpuPC = (op & 0x38);
}


/* INPUT AND OUTPUT GROUP */

void in()	{ /* IN A,(n) */
	uint8_t reg = *cpuread(cpuPC++);
	*cpuAreg = read_cpu_register(reg);
}
void inrc()	{ /* IN r,(C) */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	uint8_t tmp = read_cpu_register(*cpuCreg);
	*r[(op >> 3) & 7] = tmp;
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!tmp) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg &= ~0x10; /* H flag */
	*cpuFreg = parcalc(tmp) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg &= ~0x02; /* N flag */
}
void ini()	{ /* INI */
	uint8_t tmp = read_cpu_register(*cpuCreg);
	cpuwrite(*cpuHLreg, tmp);
	(*cpuBreg)--; /* byte counter */
	(*cpuHLreg)++;
	uint16_t k = (((*cpuCreg + 1) & 0xff) + tmp);
	*cpuFreg = (*cpuBreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuBreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (k > 0xff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (parcalc((k & 7) ^ *cpuBreg)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x02): (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (k > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void inir()	{ /* INIR */
	uint8_t tmp = read_cpu_register(*cpuCreg);
	cpuwrite(*cpuHLreg, tmp);
(*cpuBreg)--; /* byte counter */
(*cpuHLreg)++;
if (*cpuBreg){
	cpuPC -= 2;
	addcycles(5);
}
uint16_t k = (((*cpuCreg + 1) & 0xff) + tmp);
*cpuFreg = (*cpuBreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
*cpuFreg = (!*cpuBreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
*cpuFreg = (k > 0xff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
*cpuFreg = (parcalc((k & 7) ^ *cpuBreg)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x02): (*cpuFreg & ~0x02); /* N flag */
*cpuFreg = (k > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void out()	{ /* OUT (n),A */
	uint8_t reg = *cpuread(cpuPC++);
	write_cpu_register(reg, *cpuAreg);
}
void outc()	{ /* OUT (C),r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, cpuread(*cpuHLreg), cpuAreg};
	write_cpu_register(*cpuCreg, *r[(op >> 3) & 7]);
}
void outi()	{ /* OUTI */
	uint8_t tmp = *cpuread(*cpuHLreg); /* to be written to port */
	(*cpuBreg)--; /* byte counter */
	write_cpu_register(*cpuCreg, tmp);
	(*cpuHLreg)++;
	uint16_t k = (*cpuLreg + tmp);
	*cpuFreg = (*cpuBreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuBreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (k > 0xff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (parcalc((k & 7) ^ *cpuBreg)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x02): (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (k > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void otir()	{ /* OTIR */
uint8_t tmp = *cpuread(*cpuHLreg); /* to be written to port */
(*cpuBreg)--; /* byte counter */
write_cpu_register(*cpuCreg, tmp);
(*cpuHLreg)++;
if (*cpuBreg){
	cpuPC -= 2;
	addcycles(5);
}
uint16_t k = (*cpuLreg + tmp);
*cpuFreg = (*cpuBreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
*cpuFreg = (!*cpuBreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
*cpuFreg = (k > 0xff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
*cpuFreg = (parcalc((k & 7) ^ *cpuBreg)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x02): (*cpuFreg & ~0x02); /* N flag */
*cpuFreg = (k > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void outd()	{ /* OUTD */
	uint8_t tmp = *cpuread(*cpuHLreg); /* to be written to port */
	(*cpuBreg)--; /* byte counter */
	write_cpu_register(*cpuCreg, tmp);
	(*cpuHLreg)--;
	*cpuFreg = (!*cpuBreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
	uint16_t k = (*cpuLreg + tmp);
	*cpuFreg = (*cpuBreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuBreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (k > 0xff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (parcalc((k & 7) ^ *cpuBreg)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x02): (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (k > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}
void otdr()	{ /* OUTD */
	uint8_t tmp = *cpuread(*cpuHLreg); /* to be written to port */
	(*cpuBreg)--; /* byte counter */
	write_cpu_register(*cpuCreg, tmp);
	(*cpuHLreg)--;
	if (*cpuBreg){
		cpuPC -= 2;
		addcycles(5);
	}
	*cpuFreg = (!*cpuBreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (*cpuFreg | 0x02); /* N flag */
	uint16_t k = (*cpuLreg + tmp);
	*cpuFreg = (*cpuBreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuBreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (k > 0xff) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (parcalc((k & 7) ^ *cpuBreg)) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (tmp & 0x80) ? (*cpuFreg | 0x02): (*cpuFreg & ~0x02); /* N flag */
	*cpuFreg = (k > 0xff) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
}


/* UNDOCUMENTED OPCODES */

void ldixh(){ /* LD IXh,n */
	*cpuIXhreg = *cpuread(cpuPC++);
}
void ldiyh(){ /* LD IYh,n */
	*cpuIYhreg = *cpuread(cpuPC++);
}
void ldixl(){ /* LD IXl,n */
	*cpuIXlreg = *cpuread(cpuPC++);
}
void ldiyl(){ /* LD IYl,n */
	*cpuIYlreg = *cpuread(cpuPC++);
}
void lrixh(){ /* LD r,IXh */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIXhreg, cpuIXlreg, 0, cpuAreg};
	*r[(op>>3) & 7] = *cpuIXhreg;
}
void lrixl(){ /* LD r,IXl */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIXhreg, cpuIXlreg, 0, cpuAreg};
	*r[(op>>3) & 7] = *cpuIXlreg;
}
void lriyh(){ /* LD r,IYh */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIXhreg, cpuIXlreg, 0, cpuAreg};
	*r[(op>>3) & 7] = *cpuIYhreg;
}
void lriyl(){ /* LD r,IYl */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIXhreg, cpuIXlreg, 0, cpuAreg};
	*r[(op>>3) & 7] = *cpuIYlreg;
}
void lixhr(){ /* LD ixh,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIXhreg, cpuIXlreg, 0, cpuAreg};
	*cpuIXhreg = *r[op & 0x07];
}
void lixlr(){ /* LD ixl,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIXhreg, cpuIXlreg, 0, cpuAreg};
	*cpuIXlreg = *r[op & 0x07];
}
void liyhr(){ /* LD iyh,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIYhreg, cpuIYlreg, 0, cpuAreg};
	*cpuIYhreg = *r[op & 0x07];
}
void liylr(){ /* LD iyl,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIYhreg, cpuIYlreg, 0, cpuAreg};
	*cpuIYlreg = *r[op & 0x07];
}
void adixh(){ /* ADD A,IXh */
	add(*cpuIXhreg);
}
void adixl(){ /* ADD A,IXl */
	add(*cpuIXlreg);
}
void adiyh(){ /* ADD A,IYh */
	add(*cpuIYhreg);
}
void adiyl(){ /* ADD A,IYl */
	add(*cpuIYlreg);
}
void acixh(){ /* ADD A,IXh */
	adc(*cpuIXhreg);
}
void acixl(){ /* ADC A,IXl */
	adc(*cpuIXlreg);
}
void aciyh(){ /* ADC A,IYh */
	adc(*cpuIYhreg);
}
void aciyl(){ /* ADC A,IYl */
	adc(*cpuIYlreg);
}
void sbixh()	{ /* SUB A,IXh */
	sub(*cpuIXhreg);
}
void sbixl()	{ /* SUB A,IXl */
	sub(*cpuIXlreg);
}
void sbiyh()	{ /* SUB A,IYh */
	sub(*cpuIYhreg);
}
void sbiyl()	{ /* SUB A,IYl */
	sub(*cpuIYlreg);
}
void scixh(){ /* SBC A,IXh */
	sbc(*cpuIXhreg);
}
void scixl(){ /* SBC A,IXl */
	sbc(*cpuIXlreg);
}
void sciyh(){ /* SBC A,IYh */
	sbc(*cpuIYhreg);
}
void sciyl(){ /* SBC A,IYl */
	sbc(*cpuIYlreg);
}
void anixh(){ /* AND A,IXh */
	and(*cpuIXhreg);
}
void anixl(){ /* AND A,IXl */
	and(*cpuIXlreg);
}
void aniyh(){ /* AND A,IYh */
	and(*cpuIYhreg);
}
void aniyl(){ /* AND A,IYl */
	and(*cpuIYlreg);
}
void orixh(){ /* OR A,IXh */
	or(*cpuIXhreg);
}
void orixl(){ /* OR A,IXl */
	or(*cpuIXlreg);
}
void oriyh(){ /* OR A,IYh */
	or(*cpuIYhreg);
}
void oriyl(){ /* OR A,IYl */
	or(*cpuIYlreg);
}
void xrixh(){ /* XOR A,IXh */
	xor(*cpuIXhreg);
}
void xrixl(){ /* XOR A,IXl */
	xor(*cpuIXlreg);
}
void xriyh(){ /* XOR A,IYh */
	xor(*cpuIYhreg);
}
void xriyl(){ /* XOR A,IYl */
	xor(*cpuIYlreg);
}
void cpixh(){ /* CP A,IXh */
	cp(*cpuIXhreg);
}
void cpixl(){ /* CP A,IXl */
	cp(*cpuIXlreg);
}
void cpiyh(){ /* CP A,IYh */
	cp(*cpuIYhreg);
}
void cpiyl(){ /* CP A,IYl */
	cp(*cpuIYlreg);
}
void inixh()	{ /* INC IXh */
	uint8_t tmp = *cpuIXhreg;
	(*cpuIXhreg)++;
	*cpuFreg = (*cpuIXhreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuIXhreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((tmp & 0x0f) == 0x0f) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x7f) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
}
void iniyh()	{ /* INC IYh */
	uint8_t tmp = *cpuIYhreg;
	(*cpuIYhreg)++;
	*cpuFreg = (*cpuIYhreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuIYhreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((tmp & 0x0f) == 0x0f) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x7f) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
}
void inixl()	{ /* INC IXl */
	uint8_t tmp = *cpuIXlreg;
	(*cpuIXlreg)++;
	*cpuFreg = (*cpuIXlreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuIXlreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((tmp & 0x0f) == 0x0f) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x7f) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
}
void iniyl()	{ /* INC IYl */
	uint8_t tmp = *cpuIYlreg;
	(*cpuIYlreg)--;
	*cpuFreg = (*cpuIYlreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuIYlreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = ((tmp & 0x0f) == 0x0f) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x7f) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg = (*cpuFreg & ~0x02); /* N flag */
}
void dcixh()	{ /* DEC IXh */
	uint8_t tmp = *cpuIXhreg;
	(*cpuIXhreg)--;
	*cpuFreg = (*cpuIXhreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuIXhreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (!(tmp & 0x0f)) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
}
void dciyh()	{ /* DEC IYh */
	uint8_t tmp = *cpuIYhreg;
	(*cpuIYhreg)--;
	*cpuFreg = (*cpuIYhreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuIYhreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (!(tmp & 0x0f)) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
}
void dcixl()	{ /* DEC IXl */
	uint8_t tmp = *cpuIXlreg;
	(*cpuIXlreg)--;
	*cpuFreg = (*cpuIXlreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuIXlreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (!(tmp & 0x0f)) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
}
void dciyl()	{ /* DEC IYl */
	uint8_t tmp = *cpuIYlreg;
	(*cpuIYlreg)--;
	*cpuFreg = (*cpuIYlreg & 0x80) ? (*cpuFreg | 0x80) : (*cpuFreg & ~0x80); /* S flag */
	*cpuFreg = (!*cpuIYlreg) ? (*cpuFreg | 0x40) : (*cpuFreg & ~0x40); /* Z flag */
	*cpuFreg = (!(tmp & 0x0f)) ? (*cpuFreg | 0x10) : (*cpuFreg & ~0x10); /* H flag */
	*cpuFreg = (tmp == 0x80) ? (*cpuFreg | 0x04) : (*cpuFreg & ~0x04); /* P/V flag */
	*cpuFreg |= 0x02; /* N flag */
}

void unp(){ /* unprefix a prefixed opcode */
	cpuPC--;
}
void noop(){
printf("Illegal opcode: %02x %02x %02x %02x\n",*cpuread(cpuPC-4),*cpuread(cpuPC-3),*cpuread(cpuPC-2),op);
exit(1);}

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
		/* should return upper 8 bits */
		value = hCounter;
		break;
	case 0x80: /* Read VDP Data Port */
		value = read_vdp_data();
		break;
	case 0x81: /* Read VDP Control Port */
		value = statusFlags;
		statusFlags = 0; /* clear all flags when read */
		controlFlag = 0;
		lineInt = 0;
		irqPulled = 0;
		break;
	case 0xc0:
		value = ioPort1;
		break;
	case 0xc1:
		value = ioPort2;
		break;
	}
	return value;
}

void write_cpu_register(uint8_t reg, uint_fast8_t value) {
	switch (reg & 0xc1){
	case 0x00:
		memory_control(value);
		break;
	case 0x01:
		ioControl = value;
		ioPort2 = (ioPort2 & 0x7f) | ((value & 0x80) ^ (((region == 0) && (!(ioControl & 0x08))) ? 0x80 : 0));
		ioPort2 = (ioPort2 & 0xbf) | (((value & 0x20) << 1) ^ (((region == 0)  && (!(ioControl & 0x02))) ? 0x40 : 0));
		break;
	case 0x40:
	case 0x41:
	/*	printf("Writing to SN76489 PSG: %02x\n",reg);*/
		break;
	case 0x80:
		write_vdp_data(value);
		break;
	case 0x81:
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
	else if (address < 0xc000 && address >= 0x8000)
		value = read_bank2(address);
	else if (address < 0x8000 && address >= 0x4000)
		value = read_bank1(address);
	else if (address < 0x4000)
		value = read_bank0(address);
	return value;
}

/* TODO: use cpuwrite for all memory changes to catch register writes */
void cpuwrite(uint16_t address, uint_fast8_t value) {
	if (address >= 0xc000) /* writing to RAM */
		cpuRam[address & 0x1fff] = value;
	else if (address < 0xc000 && address >= 0x8000 && (bramReg & 0x8))
		bank[address >> 14][address & 0x3fff] = value;
	if(address >=0xfff8){
		switch(address & 0xf){
		case 0xc:
			bramReg = value;
			break;
		case 0xd:
			fcr[0] = (value & currentRom->mask);
			break;
		case 0xe:
			fcr[1] = (value & currentRom->mask);
			break;
		case 0xf:
			fcr[2] = (value & currentRom->mask);
			break;
		}
		banking();
	}
}

void power_reset () {
halted = 0;
iff1 = iff2 = cpuPC = iMode = cpuI = cpuR = 0;
cpuAF = cpuBC = cpuDE = cpuHL = cpuIX = cpuIY = 0xffff;
cpuSP = 0xdff0; /* TODO: hack to bypass BIOS - should be 0xffff? */
/* TODO: platform specific assignment */
cpuAreg = (uint8_t *)&cpuAF+1;
cpuFreg = (uint8_t *)&cpuAF;
cpuBreg = (uint8_t *)&cpuBC+1;
cpuCreg = (uint8_t *)&cpuBC;
cpuDreg = (uint8_t *)&cpuDE+1;
cpuEreg = (uint8_t *)&cpuDE;
cpuHreg = (uint8_t *)&cpuHL+1;
cpuLreg = (uint8_t *)&cpuHL;
cpuIXhreg = (uint8_t *)&cpuIX+1;
cpuIXlreg = (uint8_t *)&cpuIX;
cpuIYhreg = (uint8_t *)&cpuIY+1;
cpuIYlreg = (uint8_t *)&cpuIY;
cpuAFreg = (uint16_t *)&cpuAF;
cpuBCreg = (uint16_t *)&cpuBC;
cpuDEreg = (uint16_t *)&cpuDE;
cpuHLreg = (uint16_t *)&cpuHL;
cpuIXreg = (uint16_t *)&cpuIX;
cpuIYreg = (uint16_t *)&cpuIY;
ioPort1 = ioPort2 = 0xff;
}

void interrupt_polling() {
/* mode 1: set cpuPC to 0x38 */
}

void synchronize(uint_fast8_t x) {

}

uint8_t parcalc(uint8_t val){
	uint8_t par = ((val>>7)&1)+((val>>6)&1)+((val>>5)&1)+((val>>4)&1)+((val>>3)&1)+((val>>2)&1)+((val>>1)&1)+(val&1);
	return !(par % 2);
}

void addcycles(uint8_t val){
	vdp_wait += (val * 3);
	cpucc += val;
}
