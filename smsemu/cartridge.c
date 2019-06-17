#include "cartridge.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>	/* malloc; exit */
#include <string.h>

uint8_t *rom, fcr[3] = {0, 1, 2}, *bank[3];
FILE *romFile;
int rsize;

void load_rom(char *r)
{
	romFile = fopen(r, "r");
	if(romFile == NULL){
		printf("Could not open ROM file\n");
		exit(1);
	}
	fseek(romFile, 0L, SEEK_END);
	rsize = ftell(romFile);
	rewind(romFile);
	rom = malloc(rsize);
	fread(rom, rsize, 1, romFile);
	/* patch */
/*	*(rom+0x85) = 0x13;
	*(rom+0x86) = 1;*/
	bank[0] = rom + (0x4000 * fcr[0]);
	bank[1] = rom + (0x4000 * fcr[1]);
	bank[2] = rom + (0x4000 * fcr[2]);
}

void close_rom()
{
	free(rom);
}