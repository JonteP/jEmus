#ifndef APU_H_
#define APU_H_

#define CHANNELS 1
#define SAMPLES_PER_SEC 48000
#define BUFFER_SIZE (8192)
#define CPU_CLOCK 1786831 /*1789773*/
#define SAMPLE_RATIO (CPU_CLOCK / SAMPLES_PER_SEC)

#include <stdint.h>
extern uint8_t apuStatus, apuFrameCounter, pulse1Length, pulse2Length,
		pulse1Control, pulse2Control, sweep1Divide, sweep1Reload, sweep1Shift, sweep1,
		sweep2Divide, sweep2Reload, sweep2Shift, sweep2, env1Start, env2Start, envNoiseStart,
		env1Divide, env2Divide, envNoiseDivide, triLength, triLinear, triLinReload, triControl,
		noiseLength, noiseMode, noiseControl, dmcOutput, dmcControl, dmcInt, frameInt, frameWriteDelay, frameWrite, dmcRestart, dmcBitsLeft, dmcSilence;
extern int8_t pulse1Duty, pulse2Duty;
extern uint16_t pulse2Timer, pulse1Timer, triTimer, triTemp, noiseShift, noiseTimer, noiseTable[0x10],
				rateTable[0x10], dmcRate, dmcTemp, dmcAddress, dmcCurAdd, dmcLength, dmcBytesLeft, framecc, sampleCounter;
extern int16_t pulse1Temp, pulse2Temp;
extern uint8_t lengthTable[0x20], frameCounter;
extern uint32_t frameIrqDelay, apucc, frameIrqTime;
extern float sampleBuffer[BUFFER_SIZE];
void run_apu(uint16_t), dmc_fill_buffer(void), quarter_frame(void), half_frame(void);

#endif /* APU_H_ */
