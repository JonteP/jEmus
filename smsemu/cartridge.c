#include "cartridge.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>	/* malloc; exit */
#include <string.h>

uint8_t *rom, fcr[3] = {0, 1, 2}, *bank[3], bRam[0x8000], bankmask;
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
	/* *(rom+0x85) = 0x25;
	*(rom+0x86) = 1;*/
	bankmask = ((rsize >> 14) - 1);
	bank[0] = rom + ((fcr[0] & bankmask) << 14);
	bank[1] = rom + ((fcr[1] & bankmask) << 14);
	bank[2] = rom + ((fcr[2] & bankmask) << 14);
}

void close_rom()
{
	free(rom);
}
