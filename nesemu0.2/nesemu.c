#include "globals.h"
#include <stdint.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>	/* malloc; exit; free */
#include <string.h>	/* memcpy */
#include <unistd.h>
#include "ppu.h"
#include "apu.h"
#include "6502.h"
#include "cartridge.h"
#include "my_sdl.h"
#include "mapper.h"

/* TODO:
 *
 * -FIR low pass filtering of sound
 * -fix sound buffering (if broken)
 * -better sync handling, needs to read ppu one cycle earlier
 *
 * Features:
 * -file load routines
 * -oam viewer
 * -save states
 */

uint_fast8_t quit = 0, ctrb = 0, ctrb2 = 0, ctr1 = 0, ctr2 = 0;
uint_fast8_t openBus;
uint16_t ppu_wait = 0, apu_wait = 0;
FILE *logfile;
char *romName = "/home/jonas/eclipse-workspace/mmc1/metroid.nes";

int main() {
	load_rom(romName);

	logfile = fopen("/home/jonas/eclipse-workspace/logfile.txt","w");
	if (logfile==NULL)
		printf("Error: Could not create logfile\n");

	rstFlag = HARD_RESET;
	init_sdl();
	init_time();

	while (quit == 0)
	{
		opdecode();
		synchronize(0);
		if (mapperInt)
			irqPulled = 1;
		if (stateLoad)
		{
			stateLoad = 0;
			load_state();
		}
		else if (stateSave)
		{
			stateSave = 0;
			save_state();
		}
	}

	fclose(logfile);
	close_rom();
	close_sdl();
}

/* TODO:
 * -save mirroring mode
 * -save state between cpu instructions
 * -save slot banks
 */
void save_state()
{
	char *stateName = strdup(romName);
	sprintf(stateName+strlen(stateName)-3,"sta");
	FILE *stateFile = fopen(stateName, "w");
	fwrite(prgSlot,sizeof(prgSlot),1,stateFile);
	fwrite(prgBank,sizeof(prgBank),1,stateFile);
	fwrite(cpuRam,sizeof(cpuRam),1,stateFile);
	fwrite(&cpuA,sizeof(cpuA),1,stateFile);
	fwrite(&cpuX,sizeof(cpuX),1,stateFile);
	fwrite(&cpuY,sizeof(cpuY),1,stateFile);
	fwrite(&cpuP,sizeof(cpuP),1,stateFile);
	fwrite(&cpuS,sizeof(cpuS),1,stateFile);
	fwrite(&cpuPC,sizeof(cpuPC),1,stateFile);
	fwrite(chrSlot,sizeof(chrSlot),1,stateFile);
	fwrite(chrBank,sizeof(chrBank),1,stateFile);
	fwrite(oam,sizeof(oam),1,stateFile);
	fwrite(nameTableA,sizeof(nameTableA),1,stateFile);
	fwrite(nameTableB,sizeof(nameTableB),1,stateFile);
	fwrite(palette,sizeof(palette),1,stateFile);
	fwrite(&ppudot,sizeof(ppudot),1,stateFile);
	fwrite(&scanline,sizeof(scanline),1,stateFile);
	fwrite(&ppuW,sizeof(ppuW),1,stateFile);
	fwrite(&ppuX,sizeof(ppuX),1,stateFile);
	fwrite(&ppuT,sizeof(ppuT),1,stateFile);
	fwrite(&ppuV,sizeof(ppuV),1,stateFile);
	fwrite(&cart.mirroring,sizeof(cart.mirroring),1,stateFile);
	if (cart.wramSize)
		fwrite(wram,cart.wramSize,1,stateFile);
	else if (cart.bwramSize)
		fwrite(bwram,cart.bwramSize,1,stateFile);
	if (cart.cramSize)
		fwrite(chr,cart.cramSize,1,stateFile);
	fclose(stateFile);
}

void load_state()
{
	char *stateName = strdup(romName);
	sprintf(stateName+strlen(stateName)-3,"sta");
	FILE *stateFile = fopen(stateName, "r");
	fread(prgSlot,sizeof(prgSlot),1,stateFile);
	fread(prgBank,sizeof(prgBank),1,stateFile);
	fread(cpuRam,sizeof(cpuRam),1,stateFile);
	fread(&cpuA,sizeof(cpuA),1,stateFile);
	fread(&cpuX,sizeof(cpuX),1,stateFile);
	fread(&cpuY,sizeof(cpuY),1,stateFile);
	fread(&cpuP,sizeof(cpuP),1,stateFile);
	fread(&cpuS,sizeof(cpuS),1,stateFile);
	fread(&cpuPC,sizeof(cpuPC),1,stateFile);
	fread(chrSlot,sizeof(chrSlot),1,stateFile);
	fread(chrBank,sizeof(chrBank),1,stateFile);
	fread(oam,sizeof(oam),1,stateFile);
	fread(nameTableA,sizeof(nameTableA),1,stateFile);
	fread(nameTableB,sizeof(nameTableB),1,stateFile);
	fread(palette,sizeof(palette),1,stateFile);
	fread(&ppudot,sizeof(ppudot),1,stateFile);
	fread(&scanline,sizeof(scanline),1,stateFile);
	fread(&ppuW,sizeof(ppuW),1,stateFile);
	fread(&ppuX,sizeof(ppuX),1,stateFile);
	fread(&ppuT,sizeof(ppuT),1,stateFile);
	fread(&ppuV,sizeof(ppuV),1,stateFile);
	fread(&cart.mirroring,sizeof(cart.mirroring),1,stateFile);
	if (cart.wramSize)
		fread(wram,cart.wramSize,1,stateFile);
	else if (cart.bwramSize)
		fread(bwram,cart.bwramSize,1,stateFile);
	if (cart.cramSize)
		fread(chr,cart.cramSize,1,stateFile);
	fclose(stateFile);
}
