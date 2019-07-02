#include "cartridge.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>	/* malloc; exit */
#include <string.h>
#include "smsemu.h"

static inline struct RomFile load_rom(char *);
static inline uint8_t * read0(uint16_t), * read1(uint16_t), * read2(uint16_t), * empty(void);
uint8_t fcr[3] = {0, 1, 2}, *bank[3], bRam[0x8000], memControl, bramReg = 0;
struct RomFile cartRom, cardRom, biosRom, expRom, *currentRom;
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
	if(!(memControl & 0x80))
		currentRom = &expRom;
	else if(!(memControl & 0x40))
		currentRom = &cartRom;
	else if(!(memControl & 0x20))
		currentRom = &cardRom;
	else if(!(memControl & 0x08))
		currentRom = &biosRom;
	banking();
}

void banking(){
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
uint8_t * empty(){
	uint8_t *value = malloc(sizeof *value);
	/* TODO: different read back value on sms 1 */
	*value=0xff;
	return value;
	free (value);
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
