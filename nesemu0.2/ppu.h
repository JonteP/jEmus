#ifndef PPU_H_
#define PPU_H_
#include <stdint.h>

#define WWIDTH 640 /*1280 */
#define WHEIGHT 480 /*1024 */
#define SWIDTH 256
#define SHEIGHT 240
#define WPOSX 100
#define WPOSY 100
#define FRAMETIME 16639

void init_time(void), write_ppu_register(uint16_t, uint8_t);
uint8_t read_ppu_register(uint16_t), ppu_read(uint16_t);
void run_ppu(uint16_t);

extern uint8_t *chrSlot[0x8], oam[0x100],
			   frameBuffer[SHEIGHT][SWIDTH], nameBuffer[SHEIGHT][SWIDTH<<1], patternBuffer[SWIDTH>>1][SWIDTH], paletteBuffer[SWIDTH>>4][SWIDTH>>1];
extern uint8_t throttle, ppuOamAddress;
extern int16_t ppudot, scanline;
extern uint32_t nmiFlipFlop, frame;

#endif
