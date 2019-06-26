#include "smsemu.h"
#include <stdio.h>
#include <stdint.h>
#include "cartridge.h"
#include "z80.h"
#include "my_sdl.h"

char *romName;
uint8_t quit = 0, ioPort1, ioPort2;
/* trace zexdoc.log,0,noloop,{tracelog "%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,",pc,(af&ffd7),bc,de,hl,ix,iy,sp}*/
int main() {
	init_sdl();
	init_time();
	romName = "/home/jonas/Desktop/sms/unsorted/USA/World Grand Prix.sms";
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
