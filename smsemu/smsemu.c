#include "smsemu.h"
#include <stdio.h>
#include <stdint.h>
#include "cartridge.h"
#include "z80.h"
#include "my_sdl.h"
#include "vdp.h"
#include "sn79489.h"

/* Compatibility:
 */
/* TODO:
 * -FM sound
 * -special peripherals (lightgun, sports pad, paddle (https://www.raphnet.net/electronique/sms_paddle/index_en.php))
 * -vdp versions
 * -remaining vdp video modes
 * -dot renderer with proper timing
 * -remaining z80 opcodes - verify cycle counting
 * -vdp / z80 timing
 * -port access behavior differs between consoles (open bus)
 */

char *cartFile, *cardFile, *expFile, *biosFile;
uint8_t quit = 0, ioPort1, ioPort2, ioControl, region;
struct machine ntsc_us={"mpr-12808.ic2",53693175,NTSC,EXPORT}, pal={"mpr-10052.ic2",53203424,PAL,EXPORT}, ntsc_jp={"mpr-11124.ic2",53693175,NTSC,JAPAN}, *currentMachine;
sdlSettings settings;
int main() {
	currentMachine = &ntsc_jp;
	init_vdp();
	settings.ctable = smsColor;
	settings.renderQuality = "1";
	settings.audioFrequency = 48000;
	settings.channels = 1;
	settings.audioBufferSize = 2048;
	settings.window.name = "smsEmu";
	settings.window.screenHeight = currentMode->height;
	settings.window.screenWidth = currentMode->width;
	settings.window.visible = 1;
	settings.window.winHeight = 600;
	settings.window.winWidth = 800;
	settings.window.winXPosition = 100;
	settings.window.winYPosition = 100;
	settings.window.xClip = 0;
	settings.window.yClip = 0;
	init_sdl(&settings);

	init_sn79489(settings.audioBufferSize);
	biosFile = malloc(strlen(currentMachine->bios) + 6);
	sprintf(biosFile, "bios/%s",currentMachine->bios);
	cartFile = "/home/jonas/Desktop/sms/sms_test/vdptest/VDPTEST.sms";
	init_slots();
	logfile = fopen("/home/jonas/git/logfile.txt","w");
	if (logfile==NULL){
		printf("Error: Could not create logfile\n");
		exit(1);
	}
	power_reset();
	set_timings(1);
	while (quit == 0) {
		opdecode();
	}
	fclose(logfile);
	free(biosFile);
	close_sdl();
	close_rom();
	close_vdp();
	close_sn79489();
}
void set_timings(uint8_t mode){
	if(mode == 1){ /* set FPS */
		clockRate = currentMachine->masterClock;
		fps = (float)clockRate/(currentMode->fullheight * currentMode->fullwidth * 10);
	}
	else if(mode == 2){ /* set clock */
		clockRate = (currentMode->fullheight * currentMode->fullwidth * 10 * fps);
	}
	frameTime = (float)((1/fps) * 1000000000);
	init_time(frameTime);
	originalSampleRate = sampleRate = (float)clockRate /(15 * 16 * settings.audioFrequency);
	sCounter = 0;
}
/* trace zexdoc.log,0,noloop,{tracelog "%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,",pc,(af&ffd7),bc,de,hl,ix,iy,sp}*/
