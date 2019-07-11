#include "smsemu.h"
#include <stdio.h>
#include <stdint.h>
#include "cartridge.h"
#include "z80.h"
#include "my_sdl.h"
#include "vdp.h"
#include "sn79489.h"

/* Compatibility:
 * -Golvellius (U/E) - hangs on overworld (walked up one screen)
 * -Space Harrier (J) - black screen after game start
 * -the circuit - glitchy status bar
 */
/* TODO:
 * -sound (normal) - noise channel
 * -FM sound
 * -special peripherals (lightgun, sports pad, paddle)
 * -vdp versions
 * -backup RAM save support
 * -dot renderer with proper timing
 * -remaining z80 opcodes - verify cycle counting
 * -correct readback value for h counter
 * -port access behavior differs between consoles (open bus)
 * -are sprites clipped on top row? - why?
 */
char *cartFile, *cardFile, *expFile, *biosFile;
uint8_t quit = 0, ioPort1, ioPort2, ioControl, region;
struct machine ntsc_us={"mpr-10052.ic2",53693175,NTSC,EXPORT}, pal={"mpr-10052.ic2",53203424,PAL,EXPORT}, ntsc_jp={"mpr-11124.ic2",53693175,NTSC,JAPAN}, *currentMachine;
float frameTime, fps;
sdlSettings settings;
int main() {
	currentMode = &ntsc192;
	currentMachine = &ntsc_us;

	settings.renderQuality = "0";
	settings.ctable = (uint8_t*) malloc(0xc0 * sizeof(uint8_t));
	for (int i = 0; i < 0x40; i++) {
		settings.ctable[i*3]     = (((i & 0x03) << 6) | ((i & 0x03) << 4) | ((i & 0x03) << 2) | (i & 0x03));
		settings.ctable[(i*3)+1] = (((i & 0x0c) << 4) | ((i & 0x0c) << 2) | ((i & 0x0c) >> 2) | (i & 0x0c));
		settings.ctable[(i*3)+2] = (((i & 0x30) << 2) | ((i & 0x30) >> 4) | ((i & 0x30) >> 2) | (i & 0x30));
	}
	settings.audioFrequency = 48000;
	init_sdl(&settings);

	init_vdp();
	init_sn79489(settings.audioFrequency);
	fps = (float)currentMachine->masterClock/(currentMode->fullheight * currentMode->fullwidth * 10);
	printf("Running at %.02f fps\n",fps);
	frameTime = (float)((1/fps) * 1000000000);
	init_time(frameTime);

	biosFile = malloc(strlen(currentMachine->bios) + 6);
	sprintf(biosFile, "bios/%s",currentMachine->bios);
	cartFile = "/home/jonas/Desktop/sms/unsorted/Europe/Alex Kidd in Shinobi World (USA, Europe).sms";
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
	free(settings.ctable);
	close_sdl();
	close_rom();
	close_vdp();
}

/* trace zexdoc.log,0,noloop,{tracelog "%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,",pc,(af&ffd7),bc,de,hl,ix,iy,sp}*/
