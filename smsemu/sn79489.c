/*
 * sn76489.c
 *
 *  Created on: Jun 24, 2019
 *      Author: jonas
 */

#include "sn76489.h"
const int samplesPerSecond = 48000;
float sampleBuffer[BUFFER_SIZE] = {0};
float sampleRate, originalSampleRate;
float fps = 60, cpuClock;
