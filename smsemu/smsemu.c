#include <stdio.h>
#include "cartridge.h"
#include "z80.h"

char *romName;

int main() {

	romName = "/home/jonas/games/roms/sms/everdrive/Japan/Ghost House (Japan).sms";
	load_rom(romName);
	power_reset();
	while (1) {
		opdecode();
	}
}
