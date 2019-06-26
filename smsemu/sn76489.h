/*
 * sn76489.h
 *
 *  Created on: Jun 24, 2019
 *      Author: jonas
 */

#ifndef SN76489_H_
#define SN76489_H_

#define CHANNELS 1
#define BUFFER_SIZE (8192)
#define CPU_CLOCK 1789773 /*1789773 (official?); 1786831*/

extern float fps, cpuClock;
extern const int samplesPerSecond;
extern float sampleBuffer[BUFFER_SIZE], sampleRate, originalSampleRate;

#endif /* SN76489_H_ */
