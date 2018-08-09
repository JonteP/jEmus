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

void init_time(void);
void run_ppu(uint16_t);

/* PPU registers */
uint8_t ppuController, ppuMask, ppuData;

extern uint8_t *chrSlot[0x8], nameTableA[0x400], nameTableB[0x400], palette[0x20], oam[0x100],
			   frameBuffer[SHEIGHT][SWIDTH], nameBuffer[SHEIGHT][SWIDTH<<1], patternBuffer[SWIDTH>>1][SWIDTH], paletteBuffer[SWIDTH>>4][SWIDTH>>1];
uint8_t vblankSuppressed, vblank_period, ppuStatusNmi, ppuStatusSpriteZero, ppuStatusOverflow;
extern uint8_t throttle, ppuOamAddress, ppuW, ppuX;
extern int16_t scanline;
extern uint16_t ppuV, ppuT;
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
