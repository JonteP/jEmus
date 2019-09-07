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
 */
/* TODO:
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
uint8_t sms_read_z80_register(uint8_t), * sms_read_z80_memory(uint16_t);
void sms_write_z80_register(uint8_t, uint8_t), sms_write_z80_memory(uint16_t, uint8_t), sms_addcycles(uint8_t), sms_synchronize(int);
struct machine ntsc_us={"mpr-10052.ic2",NTSC_MASTER,NTSC,EXPORT,VDP_1}, pal1={"mpr-10052.ic2",PAL_MASTER,PAL,EXPORT,VDP_1}, pal2={"mpr-12808.ic2",PAL_MASTER,PAL,EXPORT,VDP_2}, ntsc_jp={"mpr-11124.ic2",NTSC_MASTER,NTSC,JAPAN,VDP_1}, *currentMachine;
int vdpCyclesToRun = 0;
sdlSettings settings;
FILE *logfile;

int main(){
	logfile = fopen("/home/jonas/git/logfile.txt","w");
	if (logfile==NULL){
		printf("Error: Could not create logfile\n");
		exit(1);
	}

	//hook up CPU
	read_z80_memory = &sms_read_z80_memory;
	write_z80_memory = &sms_write_z80_memory;
	read_z80_register = &sms_read_z80_register;
	write_z80_register = &sms_write_z80_register;
	addcycles = &sms_addcycles;
	synchronize = &sms_synchronize;

	strcpy(cartFile, "/home/jonas/Desktop/sms/unsorted/Other/Sonic The Hedgehog (FM v1.02).sms");
	currentMachine = &ntsc_jp;
	init_sdl(&settings);
	reset_emulation();

	while (quit == 0){
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
	ioPort1 = ioPort2 = 0xff;
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
	sn79489_mute = 0;
	ym2413_mute = 1;
	settings.audioFrequency = 48000;
	settings.channels = 1;
	settings.audioBufferSize = 2048;
	init_sn79489(settings.audioBufferSize);
	init_ym2413(settings.audioBufferSize);
	init_sdl_audio();
}

// Z80 interfacing instructions:
uint8_t * sms_read_z80_memory(uint16_t address){
	switch(address & 0xc000){
	case 0x0000:
		return read_bank0(address);
	case 0x4000:
		return read_bank1(address);
	case 0x8000:
		return read_bank2(address);
	case 0xc000:
		return read_bank3(address);
	}
	return 0;
}

void sms_write_z80_memory(uint16_t address, uint_fast8_t value){
	if (address >= 0xc000) /* writing to RAM */
		systemRam[address & 0x1fff] = value;
	else if (address < 0xc000 && address >= 0x8000 && (bramReg & 0x8)){
		bank[address >> 14][address & VRAM_MASK] = value;
	}
	if(address == 0x0000 && currentRom->mapper == CODEMASTERS){
		fcr[0] = (value & currentRom->mask);
		banking();
	}
	if(address == 0x4000 && currentRom->mapper == CODEMASTERS){
		fcr[1] = (value & currentRom->mask);
		banking();
	}
	if(address == 0x8000 && currentRom->mapper == CODEMASTERS){
		fcr[2] = (value & currentRom->mask);
		banking();
	}
	if(address >=0xfff8 && currentRom->mapper == SEGA){
		switch(address & 0xf){
		case 0xc:
			bramReg = value;
			if(bramReg & 0x8)
				printf("Using cartridge RAM\n");
			break;
		case 0xd:
			fcr[0] = (value & currentRom->mask);
			break;
		case 0xe:
			fcr[1] = (value & currentRom->mask);
			break;
		case 0xf:
			fcr[2] = (value & currentRom->mask);
			break;
		}
		banking();
	}
}

uint8_t sms_read_z80_register(uint8_t reg){
	switch (reg & 0xc1){
	case 0x00:
	case 0x01:
		return 0xff; /* TODO: this is sms2 behavior only */
	case 0x40: /* Read VDP V Counter */
		return (currentMode->vcount[vCounter] & 0xff);
	case 0x41: /* Read VDP H Counter */
		return hCounter;
	case 0x80: /* Read VDP Data Port */
		return read_vdp_data();
	case 0x81: ;/* Read VDP Control Port */
		uint8_t value = statusFlags;
		statusFlags = controlFlag = lineInt = irqPulled = 0;
		return value;
	case 0xc0:
		/* reads from F2 detects FM */
		if(reg == 0xf2 && currentMachine->region == JAPAN)
			return muteControl; /* TODO: should include csync counter as well */
		else if(ioEnabled)
			return ioPort1;
		else
			return 0xff;
	case 0xc1:
		if(ioEnabled)
			return ioPort2;
		else
			return 0xff;
	}
	return 0;
}
void sms_write_z80_register(uint8_t reg, uint8_t value){
	switch (reg & 0xc1){
	case 0x00:
		memory_control(value);
		break;
	case 0x01:
		iocontrol_write(value);
		break;
	case 0x40:
	case 0x41:
		write_sn79489(value);
		break;
	case 0x80:
		write_vdp_data(value);
		break;
	case 0x81:
		write_vdp_control(value);
		break;
	case 0xc0: /* YM2413 access; Keyboard support? */
		if(reg == 0xf0 && currentMachine->region == JAPAN)
			write_ym2413_register(value);
		else if(reg == 0xf2 && currentMachine->region == JAPAN){
			muteControl = (value & 0x03);
			switch(muteControl){
			case 0:
				sn79489_mute = 0;
				ym2413_mute = 1;
				break;
			case 1:
				sn79489_mute = 1;
				ym2413_mute = 0;
				break;
			case 2:
				sn79489_mute = 1;
				ym2413_mute = 1;
				break;
			case 3:
				sn79489_mute = 0;
				ym2413_mute = 0;
				break;
			}
		}
		break;
	case 0xc1:
		if(reg == 0xf1 && currentMachine->region == JAPAN)
			write_ym2413_data(value);
		break;
	}
}
void sms_addcycles(uint8_t val){
	vdpCyclesToRun += (val * VDP_CLOCK_RATIO);
	psgAccumulatedCycles += val;
	fmAccumulatedCycles += val;
	while(psgAccumulatedCycles > PSG_CLOCK_RATIO){
		audioCyclesToRun++;
		psgAccumulatedCycles -= PSG_CLOCK_RATIO;
	}
	while(fmAccumulatedCycles > FM_CLOCK_RATIO){
		fmCyclesToRun++;
		fmAccumulatedCycles -= FM_CLOCK_RATIO;
	}
}
void sms_synchronize(int cycles){
	run_vdp(vdpCyclesToRun - (cycles * VDP_CLOCK_RATIO));
	vdpCyclesToRun = cycles * VDP_CLOCK_RATIO;
	run_sn79489();
	run_ym2413();
}
/* trace zexdoc.log,0,noloop,{tracelog "%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,",pc,(af&ffd7),bc,de,hl,ix,iy,sp}*/
/*	logfile = fopen("/home/jonas/git/logfile.txt","w");
	if (logfile==NULL){
		printf("Error: Could not create logfile\n");
		exit(1);
	}*/
