#include <stdio.h>
#include "cartridge.h"
#include "z80.h"
#include "my_sdl.h"

char *romName;

int main() {
	init_sdl();
	init_time();
	romName = "/home/jonas/games/roms/sms/everdrive/Japan/Comical Machine Gun Joe (Japan).sms";
	load_rom(romName);
	power_reset();
	while (1) {
		opdecode();
	}
	close_sdl();
}
