#include <stdint.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>	/* malloc; exit; free */
#include <string.h>	/* memcpy */
#include <unistd.h>
#include "globals.h"
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

uint8_t quit = 0, ctrb = 0, ctrb2 = 0, ctr1 = 0, ctr2 = 0;
uint8_t openBus;
uint16_t ppu_wait = 0, apu_wait = 0;
FILE *logfile;

int main() {
	char *romName = "/home/jonas/eclipse-workspace/g101/ifight.nes";
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
	}

	fclose(logfile);
	close_rom();
	close_sdl();
}
