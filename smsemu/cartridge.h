/*
 * cartridge.h
 *
 *  Created on: May 25, 2019
 *      Author: jonas
 */

#ifndef CARTRIDGE_H_
#define CARTRIDGE_H_
#include <stdint.h>
#include "sha.h"
#include "parser.h"

typedef enum _mapper{
	GENERIC,
	SEGA,
	CODEMASTERS,
	KOREA
} Mapper;

struct RomFile {
	uint8_t *rom;
	uint8_t mask;
	xmlChar *sha1;
	Mapper mapper;
	uint8_t battery;
};

extern uint8_t fcr[3], *bank[3], bRam[0x8000], memControl, bramReg, cpuRam[0x2000], ioEnabled;
Mapper mapper;
extern struct RomFile cartRom, *currentRom;

struct RomFile load_rom(char *);
void init_slots(), close_rom(), memory_control(uint8_t);
uint8_t * (*read_bank0)(uint16_t),
		* (*read_bank1)(uint16_t),
		* (*read_bank2)(uint16_t),
		* (*read_bank3)(uint16_t);
void (*banking)(void);
#endif /* CARTRIDGE_H_ */
