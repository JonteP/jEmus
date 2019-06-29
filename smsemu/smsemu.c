#include "smsemu.h"
#include <stdio.h>
#include <stdint.h>
#include "cartridge.h"
#include "z80.h"
#include "my_sdl.h"

/* Compatibility:
 * -Sonic 2 - wrong priorities(?) - hangs after title
 * -wonder boy monster world - wrong priorities
 * -Golden axe (U/E) - black screen
 * -Fire & Forget II - glitch line in sky?
 * -Golvellius (U/E) - hangs on overworld (walked up one screen)
 */
/* TODO:
 * -BIOS emulation
 * -reset - difference between hard and soft?
 * -handle memory control reg writes
 * -vertical scrolling bar
 * -overscan mask (always on now?)
 * -sound (normal)
 * -FM sound
 * -special peripherals
 * -console versions
 * -video modes
 * -sprite-bg priorities may be incorrect
 * -sprites are clipped behind bg on rightmost column
 * -backup RAM save support
 * -correct timing
 */
char *romName;
uint8_t quit = 0, ioPort1, ioPort2, ioControl, region;
/* trace zexdoc.log,0,noloop,{tracelog "%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,",pc,(af&ffd7),bc,de,hl,ix,iy,sp}*/
int main() {
	region = 1;
	init_sdl();
	init_time();
	romName = "/home/jonas/Desktop/sms/unsorted/Europe/Renegade (Europe).sms";
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
