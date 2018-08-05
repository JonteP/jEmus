#ifndef PPU_H_
#define PPU_H_
#include <stdint.h>

#define WWIDTH 800 /*1280 */
#define WHEIGHT 600 /*1024 */
#define SWIDTH 256
#define SHEIGHT 240
#define WPOSX 100
#define WPOSY 100
#define FRAMETIME 16667

extern inline void init_time(void);
extern inline void run_ppu(uint16_t);

/* PPU registers */
uint8_t ppuController, ppuMask, ppuData;

uint8_t *chrSlot[0x8], nameTableA[0x400], nameTableB[0x400], frameBuffer[SHEIGHT][SWIDTH], nameBuffer[SHEIGHT][SWIDTH<<1];
uint8_t vblankSuppressed;
extern uint8_t throttle;
extern int16_t scanline;
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
