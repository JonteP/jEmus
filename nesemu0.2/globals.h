#ifndef GLOBALS_H_
#define GLOBALS_H_
#include <stdint.h>

extern uint8_t  a, x, y, flag, oamaddr, mirrmode, ctrb, ctrb2, ctr1, ctr2, nmi_output, nmi_allow, isvblank, spritezero,
			    spriteof, vblank_wait, isnmi, vblank_period, mapper;
extern uint8_t  *bpattern, *spattern, *prg, *chr, cpu[0x10000], oam[0x100], vram[0x4000], quit;
extern uint16_t pc, sp, nmi, paddr, namev, namet, scrollx, cpu_wait, rst;
extern uint32_t ppucc, cpucc;
extern int psize, csize;

#endif
