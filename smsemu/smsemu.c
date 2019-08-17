#include "smsemu.h"
#include <stdio.h>
#include <stdint.h>
#include "cartridge.h"
#include "z80.h"
#include "my_sdl.h"
#include "vdp.h"
#include "sn79489.h"

/* Compatibility:
 * zool - hangs at game start (interrupts?) discussion here: http://www.smspower.org/forums/9366-IRQAndIperiodNightmare
 * california games - skater sprite is garbage
 * the ninja - sprites sometimes get stuck
 * psycho fox - hangs when pausing
 * loretta no shouzou - corrupt graphics
 * f-16 corrupt graphics - video mode regression? (sprites seem fine)
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
 * -randomize startup vcounter? - some game rely on "random" R reg values: http://www.smspower.org/forums/11329-ImpossibleMissionAndTheAbuseOfTheRRegister#87153
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
void iocontrol_write(uint8_t value){
	/* TH pin function: http://www.smspower.org/forums/16535-HowDoesTHWorkOnTheJapaneseSMS#96462 */
	uint8_t old = (ioControl & (IOCONTROL_PORTA_TH_LEVEL | IOCONTROL_PORTB_TH_LEVEL));
	ioControl = value;
	/* Maybe should be flipped at readback instead? */
	ioPort1 = ((ioPort1 & ~IO1_PORTA_TR) | (((ioControl & IOCONTROL_PORTA_TR_LEVEL) << 1) ^ (((currentMachine->region == JAPAN) && (!(ioControl & IOCONTROL_PORTA_TR_DIRECTION))) ? IO1_PORTA_TR : 0)));
	ioPort2 = ((ioPort2 & ~IO2_PORTB_TR) | (((ioControl & IOCONTROL_PORTB_TR_LEVEL) >> 3) ^ (((currentMachine->region == JAPAN) && (!(ioControl & IOCONTROL_PORTB_TR_DIRECTION))) ? IO2_PORTB_TR : 0)));
	ioPort2 = ((ioPort2 & ~IO2_PORTB_TH) | ((ioControl & IOCONTROL_PORTB_TH_LEVEL) 		  ^ (((currentMachine->region == JAPAN) && (!(ioControl & IOCONTROL_PORTB_TH_DIRECTION))) ? IO2_PORTB_TH : 0)));
	ioPort2 = ((ioPort2 & ~IO2_PORTA_TH) | (((ioControl & IOCONTROL_PORTA_TH_LEVEL) << 1) ^ (((currentMachine->region == JAPAN) && (!(ioControl & IOCONTROL_PORTA_TH_DIRECTION))) ? IO2_PORTA_TH : 0)));
	latch_hcounter(old ^ (ioControl & (IOCONTROL_PORTA_TH_LEVEL | IOCONTROL_PORTB_TH_LEVEL)) ^ old);
}
/* trace zexdoc.log,0,noloop,{tracelog "%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,",pc,(af&ffd7),bc,de,hl,ix,iy,sp}*/
