/* HARDWARE NOTES
 * -in the Master System, Game Gear and Mega Drive, this is integrated in the VDP
 * -the Game Gear uses a modified version with stereo support, accessed via the 7 first z80 ports
 *
 * TODO:
 * -some writes may reset other regs..
 * -possibly use lookup tables for speed
 * -output should be unsigned, with a highpass filter applied (makes difference for samples)
 * (see discussion: https://forums.nesdev.com/viewtopic.php?f=23&t=15562)
 * sn76489.c
 *
 */

#include "sn79489.h"
#include <stdio.h>
#include <stdint.h>
#include "my_sdl.h"
#include "smsemu.h"
float *sampleBuffer;
float sampleRate, originalSampleRate, sample = 0;
float volume_table[16]={ .32767, .26028, .20675, .16422, .13045, .10362, .08231, .06568,
    .05193, .04125, .03277, .02603, .02067, .01642, .01304, 0 };
uint8_t noiseVolume, noiseRegister, currentReg, noisePhase;
float noiseOutput;
uint16_t noiseCounter, noiseShifter, noiseReload;
int audioCyclesToRun = 0, accumulatedCycles = 0, sampleCounter = 0, sCounter = 0, bufferSize;
struct ToneChannel tone0, tone1, tone2;
static inline int parity(int);
static inline void run_tone_channel(struct ToneChannel *);

void init_sn79489(int buffer){
	bufferSize = buffer;
	sampleBuffer = (float *)malloc(bufferSize * sizeof(float));
}

void reset_sn79489(){
	tone0.volume = tone1.volume = tone2.volume = noiseVolume = 0xf;
	tone0.reg = tone1.reg = tone2.reg = noiseReload = tone0.phase = tone1.phase = tone2.phase = 0;
	tone0.output = tone1.output = tone2.output = noiseOutput = 0;
	noiseShifter = 0x8000;
}

void close_sn79489(){
	free(sampleBuffer);
}

void write_sn79489(uint8_t value){
	if(value & 0x80)
		currentReg = (value & 0x70);
	switch(currentReg){
	case 0x00: /* Tone 0 	*/
		if(value & 0x80)
			tone0.reg = ((tone0.reg & 0x3f0) | (value & 0xf));
		else
			tone0.reg = ((tone0.reg & 0xf) | ((value & 0x3f) << 4));
		break;
	case 0x10: /* Volume 0 	*/
		tone0.volume = (value & 0xf);
		break;
	case 0x20: /* Tone 1 	*/
		if(value & 0x80)
			tone1.reg = ((tone1.reg & 0x3f0) | (value & 0xf));
		else
			tone1.reg = ((tone1.reg & 0xf) | ((value & 0x3f) << 4));
		break;
	case 0x30: /* Volume 1 	*/
		tone1.volume = (value & 0xf);
		break;
	case 0x40: /* Tone 2 	*/
		if(value & 0x80)
			tone2.reg = ((tone2.reg & 0x3f0) | (value & 0xf));
		else
			tone2.reg = ((tone2.reg & 0xf) | ((value & 0x3f) << 4));
		break;
	case 0x50: /* Volume 2 	*/
		tone2.volume = (value & 0xf);
		break;
	case 0x60: /* Noise 	*/
		noiseRegister = (value & 0xf);
		noiseShifter = 0x8000;
		switch(noiseRegister & 0x03){
		case 0:
			noiseReload = 0x10;
			break;
		case 1:
			noiseReload = 0x20;
			break;
		case 2:
			noiseReload = 0x40;
			break;
		case 3:
			noiseReload = tone2.reg;
			break;
		}
		break;
	case 0x70: /* Volume 3 	*/
		noiseVolume = (value & 0xf);
		break;
	}
}

void run_tone_channel(struct ToneChannel *channel){
	if(channel->counter){
		channel->counter--;
		if(!channel->counter){
			channel->counter = channel->reg;
			channel->phase ^= 1;
		}
		channel->output = (channel->phase ? volume_table[channel->volume] : 0);
	} else{
		channel->counter = channel->reg;
	}
}

void run_sn79489(){
while(audioCyclesToRun){
	run_tone_channel(&tone0);
	run_tone_channel(&tone1);
	run_tone_channel(&tone2);
	if(noiseCounter){
		noiseCounter--;
		if(!noiseCounter){
			noisePhase ^= 1;
			noiseCounter = noiseReload;
			if(noisePhase){
				noiseOutput = ((noiseShifter & 0x01) ? volume_table[noiseVolume] : 0);
				noiseShifter = ((noiseShifter >> 1) | (((noiseRegister & 0x04) ? parity(noiseShifter & 0x09) : (noiseShifter & 0x01)) << 15));
			}
		}
	}
	else{
		noiseCounter = noiseReload;
	}
	sample += ((tone0.output + tone1.output + tone2.output + noiseOutput) / (4*volume_table[0])); /* TODO: less hackish */
	sCounter++;
	if(sCounter == (int)sampleRate){
		sampleBuffer[sampleCounter] = (float)(sample / sCounter);
		sampleCounter++;
		if(sampleCounter == bufferSize)
			output_sound();
		sampleRate = originalSampleRate + sampleRate - sCounter;
		sample = 0;
		sCounter = 0;
	}
	audioCyclesToRun--;
}
}

int parity(int val) {
    val^=val>>8;
    val^=val>>4;
    val^=val>>2;
    val^=val>>1;
    return val&1;
};
