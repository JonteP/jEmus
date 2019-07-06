#include "cartridge.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>	/* malloc; exit */
#include <string.h>
#include "parser.h"
#include "tree.h"
#include "sha.h"
#include "smsemu.h"

static inline struct RomFile load_rom(char *);
static inline uint8_t * read0(uint16_t), * read1(uint16_t), * read2(uint16_t), * empty(uint16_t);
static inline void generic_mapper(), sega_mapper(), codemasters_mapper();
uint8_t fcr[3], *bank[3], bRam[0x8000], memControl, bramReg = 0, returnValue[1]={0};
struct RomFile cartRom, cardRom, biosRom, expRom, *currentRom;
Mapper mapper = SEGA;
int rsize;

void init_slots()
{
	biosRom = load_rom(biosFile);
	cartRom = load_rom(cartFile);
	cardRom = load_rom(cardFile);
	expRom = load_rom(expFile);
	memory_control(0xe7);
}

void memory_control(uint8_t value){
	/* TODO: wram + ioport enable/disable */
	memControl = value;
	if(!(memControl & 0x80)){
		currentRom = &expRom;
		banking = &generic_mapper;
	}
	else if(!(memControl & 0x40)){
		currentRom = &cartRom;
		if(mapper){
			banking = &codemasters_mapper;
			fcr[0] = 0;
			fcr[1] = 1;
			fcr[2] = 0;
		}
		else{
			banking = &sega_mapper;
			fcr[0] = 0;
			fcr[1] = 1;
			fcr[2] = 2;
		}
	}
	else if(!(memControl & 0x20)){
		currentRom = &cardRom;
		banking = &generic_mapper;
	}
	else if(!(memControl & 0x08)){
		currentRom = &biosRom;
		banking = &generic_mapper;
	}
	banking();
}

void generic_mapper(){
	bank[0] = currentRom->rom;
	bank[1] = currentRom->rom + 0x4000;
	bank[2] = currentRom->rom + 0x8000;
	if(currentRom->rom == NULL){
		read_bank0 = &empty;
		read_bank1 = &empty;
		read_bank2 = &empty;
	}
	else{
		read_bank0 = &read0;
		read_bank1 = &read1;
		read_bank2 = &read2;
	}
}

void sega_mapper(){
	/* TODO: bank shifting */
	bank[0] = currentRom->rom + ((fcr[0] & currentRom->mask) << 14);
	bank[1] = currentRom->rom + ((fcr[1] & currentRom->mask) << 14);
	bank[2] = (bramReg & 0x8) ? (bRam + ((bramReg & 0x4) << 12)) : currentRom->rom + ((fcr[2] & currentRom->mask) << 14);
	if(currentRom->rom == NULL){
		read_bank0 = &empty;
		read_bank1 = &empty;
		read_bank2 = &empty;
	}
	else{
		read_bank0 = &read0;
		read_bank1 = &read1;
		read_bank2 = &read2;
	}
}

void codemasters_mapper(){
	/* TODO: RAM mapping */
	bank[0] = currentRom->rom;
	bank[1] = currentRom->rom + 0x4000;
	bank[2] = currentRom->rom + ((fcr[2] & currentRom->mask) << 14);
	if(currentRom->rom == NULL){
		read_bank0 = &empty;
		read_bank1 = &empty;
		read_bank2 = &empty;
	}
	else{
		read_bank0 = &read0;
		read_bank1 = &read1;
		read_bank2 = &read2;
	}
}

uint8_t * empty(uint16_t address){
	/* TODO: different read back value on sms 1 */
	returnValue[0]=0xff;
	return returnValue;
}
uint8_t * read0(uint16_t address){
	uint8_t *value;
	if (address < 0x400)
		value = (currentRom->rom + (address & 0x3ff));
	else
		value = &bank[0][address & 0x3fff];
	return value;
}
uint8_t * read1(uint16_t address){
	uint8_t *value;
		value = &bank[1][address & 0x3fff];
	return value;
}
uint8_t * read2(uint16_t address){
	uint8_t *value;
		value = &bank[2][address & 0x3fff];
	return value;
}

struct RomFile load_rom(char *r){
	FILE *rfile = fopen(r, "r");
	if(rfile == NULL){
		struct RomFile output = { NULL };
		return output;
	}
	fseek(rfile, 0L, SEEK_END);
	rsize = ftell(rfile);
	rewind(rfile);
	uint8_t *tmpRom = malloc(rsize);
	fread(tmpRom, rsize, 1, rfile);
	/* patch */
	/* *(rom+0x85) = 0x25;
	*(rom+0x86) = 1;*/
	uint8_t mask = ((rsize >> 14) - 1);
	struct RomFile output = { tmpRom, mask };
	return output;
}
void close_rom()
{
	if(cartRom.rom != NULL)
		free(cartRom.rom);
	if(biosRom.rom != NULL)
		free(biosRom.rom);
	if(cardRom.rom != NULL)
		free(cardRom.rom);
	if(expRom.rom != NULL)
		free(expRom.rom);
}
