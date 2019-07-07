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
uint8_t vol0, vol1, vol2, vol3, noise, currentReg;
uint16_t tone0, tone1, tone2, tone0counter, tone1counter, tone2counter, noiseCounter;

void init_sn79489(){
	vol0 = vol1 = vol2 = vol3 = 0xf;
	tone0 = tone1 = tone2 = noise = 0;
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
