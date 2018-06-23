#ifndef PPU_H_
#define PPU_H_
#include <stdint.h>

#define WWIDTH 1280
#define WHEIGHT 1024
#define SWIDTH 256
#define SHEIGHT 240
#define WPOSX 100
#define WPOSY 100
#define FRAMETIME 16667

extern inline void init_graphs(int, int, int, int, int, int), init_time(void);
extern inline void run_ppu(uint16_t), kill_graphs(void);

uint8_t vblankSuppressed;
uint32_t nmiIsTriggered;

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
