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

void init_sn79489(int), reset_sn79489(void), close_sn79489(void), write_sn79489(uint8_t), run_sn79489(void);

struct ToneChannel {
	uint16_t reg;
	uint16_t counter;
	uint8_t phase;
	uint8_t volume;
	float output;
};

extern float fps;
extern const int samplesPerSecond;
extern float *sampleBuffer, sampleRate, originalSampleRate;
extern int audioCyclesToRun, accumulatedCycles, sampleCounter, sCounter;

#endif /* SN79489_H_ */
