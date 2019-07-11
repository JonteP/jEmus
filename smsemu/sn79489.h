/*
 * sn76489.h
 *
 *  Created on: Jun 24, 2019
 *      Author: jonas
 */

#ifndef SN79489_H_
#define SN79489_H_

#include <stdio.h>
#include <stdint.h>

#define CHANNELS 1
#define BUFFER_SIZE (8192)
#define CPU_CLOCK 1789773 /*1789773 (official?); 1786831*/

void init_sn79489(int), write_sn79489(uint8_t), run_sn79489(void);

extern float fps, cpuClock;
extern const int samplesPerSecond;
extern float sampleBuffer[BUFFER_SIZE], sampleRate, originalSampleRate;
extern int audioCycles, audioAccum, sampleCounter;

#endif /* SN79489_H_ */
