#ifndef PPU_H_
#define PPU_H_
#include <stdint.h>

#define WWIDTH 320
#define WHEIGHT 240
#define SWIDTH 256
#define SHEIGHT 240
#define WPOSX 100
#define WPOSY 100
#define FRAMETIME 16667

extern inline void init_graphs(int, int, int, int, int, int), init_time();
extern inline void run_ppu(uint16_t), kill_graphs();

#endif

/*PPU globals
 * spritezero
 * isvblank
 * namev
 * oamaddr
 * nmi_allow
 * ppucc
 * oam
 * spriteof
 * bpattern
 * spattern
 * scrollx
 * vblank_period
 */
