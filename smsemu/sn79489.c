/*
 * sn76489.c
 *
 *  Created on: Jun 24, 2019
 *      Author: jonas
 */

#include "sn79489.h"
#include <stdio.h>
#include <stdint.h>
const int samplesPerSecond = 48000;
float sampleBuffer[BUFFER_SIZE] = {0};
float sampleRate, originalSampleRate;
float cpuClock;
uint8_t vol0, vol1, vol2, vol3, noise, currentReg, pol0, pol1, pol2;
int8_t output0, output1, output2;
uint16_t tone0, tone1, tone2, tone0counter, tone1counter, tone2counter, noiseCounter;
int audioCycles = 0, audioAccum = 0, sampleCounter = 0, sample = 0;

void init_sn79489(){
	vol0 = vol1 = vol2 = vol3 = 0xf;
	tone0 = tone1 = tone2 = noise = pol0 = pol1 = pol2 = 0;
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
		output0 = ((1 - (pol0 << 1)) * vol0);
	} else{
		tone0counter = tone0;
		output0 = vol0;
	}
	audioCycles--;
	sample += output0;
	sampleCounter++;
}
}
