#include "smsemu.h"
#include <stdio.h>
#include <stdint.h>
#include "cartridge.h"
#include "z80.h"
#include "my_sdl.h"
#include "vdp.h"

/* Compatibility:
 * -Golvellius (U/E) - hangs on overworld (walked up one screen)
 */
/* TODO:
 * -overscan mask (always on now?)
 * -sound (normal)
 * -FM sound
 * -special peripherals (lightgun, sports pad, paddle)
 * -console versions
 * -video modes
 * -backup RAM save support
 * -dot renderer
 * -correct timing
 * -remaining z80 opcodes
 *
 * 1 struct for machine, 1 struct for video mode?
 *
 *machine:
 *-region
 *-pal/ntsc
 *-pal/ntsc
 *-bios
 *-form factor
 *
 * video mode:
 * -lines of active display
 * -bottom border onset
 * -bottom blanking onset
 * -vertical blanking onset
 * -top blanking offset
 * -top border offset
 * -total lines
 * -v counter change value
 */
char *cartFile, *cardFile, *expFile, *biosFile;
uint8_t quit = 0, ioPort1, ioPort2, ioControl, region;
/* trace zexdoc.log,0,noloop,{tracelog "%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,",pc,(af&ffd7),bc,de,hl,ix,iy,sp}*/
int main() {
	region = 0;
	init_sdl();
	init_time();
	currentMode = &ntsc192;
	biosFile = "/home/jonas/mame/roms/smsj/mpr-11124.ic2";
	cartFile = "/home/jonas/Desktop/sms/unsorted/Japan/Alex Kidd no Miracle World.sms";
	/*romName = "/home/jonas/games/sms_test/zexsms/zexdoc.sms";*/
	/*romName = "/home/jonas/mame/roms/sms/mpr-11124.ic2";*/
	init_slots();
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
