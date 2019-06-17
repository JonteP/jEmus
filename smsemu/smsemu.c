#include <stdio.h>
#include "cartridge.h"
#include "z80.h"
#include "my_sdl.h"

char *romName;
/* trace zexdoc.log,0,noloop,{tracelog "%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,",pc,(af&ffd7),bc,de,hl,ix,iy,sp}*/
int main() {
	init_sdl();
	init_time();
	romName = "/home/jonas/games/roms/sms/everdrive/60hz/Golvellius (USA, Europe).sms";
/*	romName = "/home/jonas/games/sms_test/zexsms/zexdoc.sms";*/
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
