#include "smsemu.h"
#include <stdio.h>
#include <stdint.h>
#include "cartridge.h"
#include "z80.h"
#include "my_sdl.h"

/* Compatibility: */
/* -Phantasy star (JP) - black screen
 * -Sonic 2 - black screen
 * spellcaster (US) - resets when starting game
 * R-Type (J) - messed up graphics
 */
/* TODO:
 * -BIOS emulation
 * -pause button (NMI)
 * -reset
 * -handle memory control reg writes
 * -vertical scrolling bar
 * -overscan mask (always on now?)
 * -sound (normal)
 * -FM sound
 * -special peripherals
 * -console versions
 * -video modes
 * -sprite-bg priorities may be incorrect
 */
char *romName;
uint8_t quit = 0, ioPort1, ioPort2, ioControl, region;
/* trace zexdoc.log,0,noloop,{tracelog "%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,",pc,(af&ffd7),bc,de,hl,ix,iy,sp}*/
int main() {
	region = 0;
	init_sdl();
	init_time();
	romName = "/home/jonas/Desktop/sms/unsorted/Japan/R-Type.sms";
	/*romName = "/home/jonas/games/sms_test/zexsms/zexdoc.sms";*/
	/*romName = "/home/jonas/mame/roms/sms/mpr-11124.ic2";*/
	load_rom(romName);
	logfile = fopen("/home/jonas/git/logfile.txt","w");
	if (logfile==NULL){
		printf("Error: Could not create logfile\n");
		exit(1);
	}

	power_reset();
	while (quit == 0) {
		opdecode();
	}
	fclose(logfile);
	close_sdl();
	close_rom();
}
