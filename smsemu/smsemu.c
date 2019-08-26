#include "smsemu.h"
#include <stdio.h>
#include <stdint.h>
#include "cartridge.h"
#include "z80.h"
#include "my_sdl.h"
#include "vdp.h"
#include "sn79489.h"
#include "ym2413.h"

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
static inline void init_video(void), init_audio(void);
char cartFile[PATH_MAX], cardFile[PATH_MAX], expFile[PATH_MAX], biosFile[PATH_MAX];
uint8_t quit = 0, ioPort1, ioPort2, ioControl, region, reset = 0;
struct machine ntsc_us={"mpr-10052.ic2",NTSC_MASTER,NTSC,EXPORT,VDP_1}, pal1={"mpr-10052.ic2",PAL_MASTER,PAL,EXPORT,VDP_1}, pal2={"mpr-12808.ic2",PAL_MASTER,PAL,EXPORT,VDP_2}, ntsc_jp={"mpr-11124.ic2",NTSC_MASTER,NTSC,JAPAN,VDP_1}, *currentMachine;
sdlSettings settings;
FILE *logfile;

int main() {

	logfile = fopen("/home/jonas/git/logfile.txt","w");
		if (logfile==NULL){
			printf("Error: Could not create logfile\n");
			exit(1);
		}
	currentMachine = &ntsc_jp;
	init_sdl(&settings);
	reset_emulation();

	while (quit == 0) {
		run_z80();
		if(reset){
			reset = 0;
			reset_emulation();
		}
	}
	fclose(logfile);
	close_sdl();
	close_rom();
	close_vdp();
	close_sn79489();
}

void reset_emulation(){
	init_video();
	init_audio();
	sprintf(biosFile, "bios/%s",currentMachine->bios);
	init_slots();
	power_reset();
	set_timings(1);
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
	set_timings_sn79489(PSG_CLOCK_DIV * settings.audioFrequency, clockRate);
	set_timings_ym2413(FM_CLOCK_DIV * settings.audioFrequency, clockRate);
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

void machine_menu_option(int option){
	switch(option & 0xf){
	case 1:
		currentMachine = &ntsc_jp;
		break;
	case 2:
		currentMachine = &ntsc_us;
		break;
	case 3:
		currentMachine = &pal1;
		break;
	case 4:
		currentMachine = &pal2;
		break;
	}
	init_video();
	set_timings(1);
	init_audio();
	toggle_menu();
}

void init_video(){
	default_video_mode();
	settings.renderQuality = "1";
	settings.ctable = smsColor;
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
	init_vdp();
	init_sdl_video();
}

void init_audio(){
	settings.audioFrequency = 48000;
	settings.channels = 1;
	settings.audioBufferSize = 2048;
	init_sn79489(settings.audioBufferSize);
	init_ym2413(settings.audioBufferSize);
	init_sdl_audio();
}

/* trace zexdoc.log,0,noloop,{tracelog "%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,",pc,(af&ffd7),bc,de,hl,ix,iy,sp}*/
/*	logfile = fopen("/home/jonas/git/logfile.txt","w");
	if (logfile==NULL){
		printf("Error: Could not create logfile\n");
		exit(1);
	}*/
