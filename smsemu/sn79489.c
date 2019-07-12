/* HARDWARE NOTES
 * -there are different hardware implementations with differences...
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
float sampleBuffer[BUFFER_SIZE] = {0};
float sampleRate, originalSampleRate, sample = 0;
float cpuClock;
float volume_table[16]={ .32767, .26028, .20675, .16422, .13045, .10362, .08231, .06568,
    .05193, .04125, .03277, .02603, .02067, .01642, .01304, 0 };
uint8_t vol0, vol1, vol2, vol3, noise, currentReg, pol0, pol1, pol2, polNoise;
float output0, output1, output2, outputNoise;
uint16_t tone0, tone1, tone2, tone0counter, tone1counter, tone2counter, noiseCounter, noiseShifter, noiseReload;
int audioCycles = 0, audioAccum = 0, sampleCounter = 0, sCounter = 0;
static inline int parity(int);

void init_sn79489(int freq){
	vol0 = vol1 = vol2 = vol3 = 0xf;
	tone0 = tone1 = tone2 = noiseReload = pol0 = pol1 = pol2 = 0;
	output0 = output1 = output2 = outputNoise = 0;
	originalSampleRate = sampleRate = (float)currentMachine->masterClock /(15 * 16 * freq);
	noiseShifter = 0x8000;
}

void write_sn79489(uint8_t value){
if(value & 0x80){ /* LATCH/DATA */
	currentReg = (value & 0x70);
	switch(currentReg){
	case 0x00: /* Tone 0 	*/
		tone0 = ((tone0 & 0x3f0) | (value & 0xf));
		break;
	case 0x10: /* Volume 0 	*/
		vol0 = (value & 0xf);
		break;
	case 0x20: /* Tone 1 	*/
		tone1 = ((tone1 & 0x3f0) | (value & 0xf));
		break;
	case 0x30: /* Volume 1 	*/
		vol1 = (value & 0xf);
		break;
	case 0x40: /* Tone 2 	*/
		tone2 = ((tone2 & 0x3f0) | (value & 0xf));
		break;
	case 0x50: /* Volume 2 	*/
		vol2 = (value & 0xf);
		break;
	case 0x60: /* Noise 	*/
		noise = (value & 0xf);
		noiseShifter = 0x8000;
		switch(noise & 0x03){
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
			noiseReload = tone2;
			break;
		}
		break;
	case 0x70: /* Volume 3 	*/
		vol3 = (value & 0xf);
		break;
	}
}
else{ /* DATA */
	switch(currentReg){
	case 0x00: /* Tone 0 	*/
		tone0 = ((tone0 & 0xf) | ((value & 0x3f) << 4));
		break;
	case 0x10: /* Volume 0 	*/
		vol0 = (value & 0xf);
		break;
	case 0x20: /* Tone 1 	*/
		tone1 = ((tone1 & 0xf) | ((value & 0x3f) << 4));
		break;
	case 0x30: /* Volume 1 	*/
		vol1 = (value & 0xf);
		break;
	case 0x40: /* Tone 2 	*/
		tone2 = ((tone2 & 0xf) | ((value & 0x3f) << 4));
		break;
	case 0x50: /* Volume 2 	*/
		vol2 = (value & 0xf);
		break;
	case 0x60: /* Noise 	*/
		noise = (value & 0xf);
		noiseShifter = 0x8000;
		switch(noise & 0x03){
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
			noiseReload = tone2;
			break;
		}
		break;
	case 0x70: /* Volume 3 	*/
		vol3 = (value & 0xf);
		break;
	}
}
}

void run_sn79489(){
while(audioCycles){
	if(tone0counter){
		tone0counter--;
		if(!tone0counter){
			tone0counter = tone0;
			pol0 ^= 1;
		}
		output0 = (pol0 ? volume_table[vol0] : 0);
	} else{
		tone0counter = tone0;
	}
	if(tone1counter){
		tone1counter--;
		if(!tone1counter){
			tone1counter = tone1;
			pol1 ^= 1;
		}
		output1 = (pol1 ? volume_table[vol1] : 0);
	} else{
		tone1counter = tone1;
	}
	if(tone2counter){
		tone2counter--;
		if(!tone2counter){
			tone2counter = tone2;
			pol2 ^= 1;
		}
		output2 = (pol2 ? volume_table[vol2] : 0);
	} else{
		tone2counter = tone2;
	}
	if(noiseCounter){
		noiseCounter--;
		if(!noiseCounter){
			polNoise ^= 1;
			noiseCounter = noiseReload;
			if(polNoise){
				outputNoise = ((noiseShifter & 0x01) ? volume_table[vol3] : 0);
				noiseShifter = ((noiseShifter >> 1) | (((noise & 0x04) ? parity(noiseShifter & 0x09) : (noiseShifter & 0x01)) << 15));
			}
		}
	}
	else{
		noiseCounter = noiseReload;
	}
	sample += ((output0 + output1 + output2 + outputNoise) / (4*volume_table[0])); /* TODO: less hackish */
	sCounter++;
	if(sCounter == (int)sampleRate){
		sampleBuffer[sampleCounter] = (float)(sample / sCounter);
		sampleCounter++;
		if(sampleCounter == BUFFER_SIZE)
			output_sound();
		sampleRate = originalSampleRate + sampleRate - sCounter;
		sample = 0;
		sCounter = 0;
	}
	audioCycles--;
}
}

int parity(int val) {
    val^=val>>8;
    val^=val>>4;
    val^=val>>2;
    val^=val>>1;
    return val&1;
};
