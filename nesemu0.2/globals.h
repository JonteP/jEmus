#include <stdint.h>

extern uint8_t a, x, y, flag, vval, rw, oamaddr, mirrmode, w, vraminc, s, ctrb, ctrb2,
			ctr1, ctr2, nmi_output, nmiset, nmi_disable, isvblank, spritezero, ppureg,
			vbuff, spriteof, vblank_wait, isnmi, vblank_period, mapper;
extern uint8_t mm1_shift, mm1_buff, wram, prg_bank, chr_bank, prg_size, chr_size;
extern uint8_t *attrib, *bpattern, *spattern, *name, *prg, *chr;
extern uint8_t cpu[0x10000], oam[0x100], vram[0x4000];
extern uint16_t pc, sp, addr, tmp, nmi, paddr, namev, namet, scrollx, scanline, cpu_wait, ppudot;
extern uint32_t ppucc, cpucc, frame;
extern int psize, csize;
