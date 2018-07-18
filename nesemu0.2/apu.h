#ifndef APU_H_
#define APU_H_

#include <stdint.h>
extern uint8_t apuStatus, apuFrameCounter, pulse1Length, pulse2Length, waitBuffer,
		pulse1Control, pulse2Control, sweep1Divide, sweep1Reload, sweep1Shift, sweep1,
		sweep2Divide, sweep2Reload, sweep2Shift, sweep2, env1Start, env2Start, envNoiseStart,
		env1Divide, env2Divide, envNoiseDivide, triLength, triLinear, triLinReload, triControl,
		noiseLength, noiseMode, noiseControl, dmcOutput, dmcControl, dmcInt, frameInt;
extern int8_t pulse1Duty, pulse2Duty;
extern uint16_t pulse1Timer, pulse2Timer, pulse2SampleCounter,
				triTimer, triTemp, noiseShift, noiseTimer, noiseTable[0x10],
				rateTable[0x10], dmcRate, dmcTemp, dmcAddress, dmcCurAdd, dmcLength, dmcBytesLeft, apucc;
extern int16_t pulse1Temp, pulse2Temp;
extern uint8_t lengthTable[0x20], frameCounter;

extern inline void run_apu(uint16_t), init_sounds(void), output_sound(void), dmc_fill_buffer(void), quarter_frame(void), half_frame(void);

#endif /* APU_H_ */
