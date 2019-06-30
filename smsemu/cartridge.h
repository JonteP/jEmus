/*
 * cartridge.h
 *
 *  Created on: May 25, 2019
 *      Author: jonas
 */

#ifndef CARTRIDGE_H_
#define CARTRIDGE_H_
#include <stdint.h>

struct RomFile {
	uint8_t *rom;
	uint8_t mask;
};

extern uint8_t fcr[3], *bank[3], bRam[0x8000], memControl, bramReg;
extern struct RomFile cartRom, *currentRom;

void init_slots(), close_rom(), memory_control(uint8_t), banking(void);
uint8_t * (*read_bank0)(uint16_t), * (*read_bank1)(uint16_t),  * (*read_bank2)(uint16_t);

#endif /* CARTRIDGE_H_ */
