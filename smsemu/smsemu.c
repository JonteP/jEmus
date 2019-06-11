#include <stdio.h>
#include "cartridge.h"
#include "z80.h"
#include "my_sdl.h"

char *romName;

int main() {
	init_sdl();
	init_time();
	romName = "/home/jonas/games/sms_test/zexsms/zexdoc.sms";
	load_rom(romName);
	power_reset();
	while (quit == 0) {
		opdecode();
	}
	close_sdl();
	close_rom();
}
