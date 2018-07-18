#include "apu.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "globals.h"
#include "nestools.h"
#include "SDL.h"

#define FRAME_COUNT 7456
#define CHANNELS 1
#define SAMPLES_PER_SEC 48000
#define BUFFER_SIZE 8192
#define CPU_CLOCK 1789773
#define SAMPLE_RATIO (CPU_CLOCK / SAMPLES_PER_SEC)

static inline void do_irq();

uint8_t apuStatus, apuFrameCounter, frameCounter = 0, pulse1Length = 0, waitBuffer = 0,
		pulse2Length = 0, pulse1Control = 0, pulse2Control = 0, pulse1Mute = 0,
		sweep1Divide = 0, sweep1Counter = 0, sweep1Reload = 0,
		env1Start = 0, env2Start = 0, envNoiseStart = 0, env1Divide = 0, env2Divide = 0, envNoiseDivide = 0,
		env1Decay = 0, env2Decay = 0, envNoiseDecay = 0,
		sweep1Shift = 0, sweep1 = 0, sweep2Divide = 0, sweep2Counter = 0, sweep2Reload = 0,
		sweep2Shift = 0, sweep2 = 0, triLength = 0, triLinear = 0,
		triLinReload = 0, triControl, noiseControl, noiseLength = 0, noiseMode = 0,
		dmcOutput = 0, dmcControl = 0, dmcBitsLeft = 8, dmcSilence = 1,
		dmcShift = 0, dmcBuffer = 0, dmcInt = 0, frameInt = 0;
int8_t pulse1Duty = 0, pulse2Duty = 0, triSeq = 0, triBuff = 0;
uint16_t pulse1Timer = 0, pulse2Timer = 0,
		pulseSampleCounter = 0, tmpcounter = 0, pulseQueueCounter = 0,
		triTimer = 0, triTemp, noiseShift, noiseTimer, noiseTemp, dmcTemp,
		dmcRate = 0, dmcAddress, dmcCurAdd, dmcLength = 0, dmcBytesLeft = 0;
int16_t pulse1Temp = 0, pulse2Temp = 0;
uint16_t pulseBuffer[BUFFER_SIZE] = {0};
uint16_t apucc = 0;

uint16_t tmpval1 = 0, tmpval2 = 0, tmpvalTri = 0, tmpcnt = 0, tmpvalNoise = 0, tmpvalDmc = 0;

uint8_t lengthTable[0x20] = { 10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14,
		12, 26, 14, 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32,
		30 };

uint8_t dutySequence[4][8] = { { 0, 0, 0, 0, 0, 0, 0, 1 },
							   { 0, 0, 0, 0, 0, 0, 1, 1 },
							   { 0, 0, 0, 0, 1, 1, 1, 1 },
							   { 1, 1, 1, 1, 1, 1, 0, 0 } };
uint8_t triSequence[0x20] = { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
uint16_t noiseTable[0x10] = { 4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068};
uint16_t rateTable[0x10] = { 428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106,  84,  72,  54 };
uint16_t frameClock[5] = {7457, 14913, 22371, 29829, 37281};
uint16_t frameReset[2] = {29830, 37282};
SDL_AudioSpec AudioSettings = {0};
int BytesPerSample = sizeof(int16_t) * CHANNELS;


void run_apu(uint16_t ntimes) { /* apu cycle times */
	while (ntimes) {
		if (apucc == frameClock[frameCounter]) {
			if ((!(apuFrameCounter&0x80)) || ((apuFrameCounter&0x80) && frameCounter != 3)) {
				quarter_frame ();
			}
			if ((apuFrameCounter & 0x80) && (!frameCounter || frameCounter == 2)) {
				half_frame();
		  } else if ((!(apuFrameCounter & 0x80)) && (frameCounter%2)) {
				half_frame();
		  }
			frameCounter++;
		}
		if ((!(apuFrameCounter&0xc0)) && apucc >= 29828 && apucc <= 29830)
			frameInt = 1;
		if (apucc == frameReset[(apuFrameCounter & 0x80)>>7]) {
			frameCounter = 0;
			apucc = 0;
		}

		if (pulse1Length && (apuStatus&1)) {
			if (pulse1Temp <= 0x7ff && pulse1Temp >= 8) {
				tmpval1 += (pulse1Control&0x10) ? (dutySequence[(pulse1Control>>6)&3][pulse1Duty>>1] * pulse1Control&0xf) : (dutySequence[(pulse1Control>>6)&3][pulse1Duty>>1] * env1Decay);
			} else
				tmpval1 += 0;
			if (pulse1Temp < 0) {
				pulse1Temp = pulse1Timer;
				pulse1Duty--;
				if (pulse1Duty < 0)
					pulse1Duty = 15;
			}
			pulse1Temp--;
		} else
			tmpval1 += 0;
		if (pulse2Length && (apuStatus&2)) {
			if (pulse2Temp <= 0x7ff && pulse2Temp >= 8) {
				tmpval2 += (pulse2Control&0x10) ? (dutySequence[(pulse2Control>>6)&3][pulse2Duty>>1] * pulse2Control&0xf) : (dutySequence[(pulse2Control>>6)&3][pulse2Duty>>1] * env2Decay);
			} else
				tmpval2 += 0;
			if (pulse2Temp < 0) {
				pulse2Temp = pulse2Timer;
				pulse2Duty--;
				if (pulse2Duty < 0)
					pulse2Duty = 15;
			}
			pulse2Temp--;
		} else
			tmpval2 += 0;
		if (triLength && triLinear && (apuStatus&4)) {
			if (triLength>=2 && triLinear>=2) {
				tmpvalTri += triSequence[triSeq];
				triBuff = triSequence[triSeq];
			} else
				tmpvalTri += triBuff;
			if (!triTemp) {
				triTemp = triTimer;
				triSeq--;
				if (triSeq < 0)
					triSeq = 31;
			}
			triTemp--;
		} else
			tmpvalTri += triBuff;
		if (noiseLength && (apuStatus&8)) {
			if (!(noiseShift&1)) {
				tmpvalNoise += (noiseControl&0x10) ? (noiseControl&0xf) : envNoiseDecay;
			} else
				tmpvalNoise += 0;
			if (!noiseTemp) {
				noiseTemp = noiseTimer;
				noiseShift = ((noiseShift>>1) | ((noiseMode ? ((noiseShift&1) ^ ((noiseShift>>1)&1)) : ((noiseShift&1) ^ ((noiseShift>>1)&1)))<<14));
			}
			if (cpucc%2)
				noiseTemp--;
		} else
			tmpvalNoise += 0;

		dmc_fill_buffer();
		tmpvalDmc += dmcOutput;
		if (dmcInt || frameInt)
			do_irq();
		if (dmcBytesLeft) {
			/*		}
					 else
						tmpvalDmc += 0;*/
			if (!dmcTemp) {
				dmcTemp = dmcRate;
				if (!dmcSilence) {
					if (!((dmcOutput + ((dmcShift&1) ? 2 : -2)) & 0x80))
						dmcOutput += ((dmcShift&1) ? 2 : -2);
				}
				dmcShift = (dmcShift>>1);
				dmcBitsLeft--;
				if (!dmcBitsLeft) {
					dmcBitsLeft = 8;
					if (!dmcBuffer)
						dmcSilence = 1;
					else {
						dmcSilence = 0;
						dmcShift = dmcBuffer;
						dmcBuffer = 0;
					}
				}
			}
			dmcTemp--;
		}

		tmpcnt++;
		if (tmpcnt==SAMPLE_RATIO) {
			pulseBuffer[pulseSampleCounter] = ((tmpvalDmc/SAMPLE_RATIO)*250) + ((tmpvalNoise/SAMPLE_RATIO)*125) + ((tmpvalTri/SAMPLE_RATIO)*500) + ((tmpval1/SAMPLE_RATIO)*500) + ((tmpval2/SAMPLE_RATIO)*500);
			tmpcnt = 0;
			tmpval1 = 0;
			tmpval2 = 0;
			tmpvalTri = 0;
			tmpvalNoise = 0;
			tmpvalDmc = 0;
			pulseSampleCounter++;
			if ((pulseSampleCounter >= (BUFFER_SIZE>>1)) && !waitBuffer)
				waitBuffer = 1;
			if (pulseSampleCounter >= BUFFER_SIZE) {
				pulseSampleCounter = 0;
			}
		}
		ntimes--;
		apu_wait--;
		apucc++;
	}
}

void half_frame () {
	if (pulse1Length) {
		if (!((pulse1Control>>5)&1)) {
			pulse1Length--;
		}
	}
	if (pulse2Length) {
		if (!((pulse2Control>>5)&1))
			pulse2Length--;
	}
	if (triLength) {
		if (!((triControl>>7)&1))
			triLength--;
	}
	if (noiseLength) {
		if (!((noiseControl>>5)&1))
			noiseLength--;
	}
	if (!sweep1Counter || sweep1Reload) {
		sweep1Counter = sweep1Divide;
		sweep1Reload = 0;
		if (sweep1&0x80) {
			pulse1Timer += (sweep1&8) ? -((pulse1Timer>>sweep1Shift)-1) : (pulse1Timer>>sweep1Shift);
		}

	} else
		sweep1Counter--;
	if (!sweep2Counter || sweep2Reload) {
		sweep2Counter = sweep2Divide;
		sweep2Reload = 0;
		if (sweep2&0x80) {
			pulse2Timer += (sweep2&8) ? -(pulse2Timer>>sweep2Shift) : (pulse2Timer>>sweep2Shift);
		}

	} else
		sweep2Counter--;
}

void dmc_fill_buffer () {
if ((!dmcBuffer) && dmcBytesLeft) {
	/* TODO: stall CPU */
	dmcBuffer = cpu[dmcCurAdd];
	if (dmcCurAdd == 0xffff)
		dmcCurAdd = 0x8000;
	else dmcCurAdd++;
	dmcBytesLeft--;
	if (!dmcBytesLeft) {
		if (dmcControl&0x40) {
			dmcBytesLeft = dmcLength;
			dmcCurAdd = dmcAddress;
		} else if (dmcControl&0x80)
			dmcInt = 1;
	}
}
}

void do_irq () {
	if (!(flag&4)) {
		cpu[sp--] = ((pc) & 0xff00) >> 8;
		cpu[sp--] = ((pc) & 0xff);
		cpu[sp--] = flag;
		pc = (cpu[irq + 1] << 8) + cpu[irq];
		bitset(&flag, 1, 2); /* set I flag */
	}
}

void quarter_frame () {
	if (env1Start) {
		env1Start = 0;
		env1Decay = 15;
		env1Divide = (pulse1Control&0xf);
	} else {
		if (!env1Divide) {
			env1Divide = (pulse1Control&0xf);
				if (!env1Decay && (pulse1Control&0x20)) {
					env1Decay = 15;
				} else if (env1Decay)
					env1Decay--;
		} else
			env1Divide--;
	}
	if (env2Start) {
		env2Start = 0;
		env2Decay = 15;
		env2Divide = (pulse2Control&0xf);
	} else {
		if (!env2Divide) {
			env2Divide = (pulse2Control&0xf);
				if (!env2Decay && (pulse2Control&0x20)) {
					env2Decay = 15;
				} else if (env2Decay)
					env2Decay--;
		} else
			env2Divide--;
	}
	if (envNoiseStart) {
		envNoiseStart = 0;
		envNoiseDecay = 15;
		envNoiseDivide = (noiseControl&0xf);
	} else {
		if (!envNoiseDivide) {
			envNoiseDivide = (noiseControl&0xf);
				if (!envNoiseDecay && (noiseControl&0x20)) {
					envNoiseDecay = 15;
				} else if (envNoiseDecay)
					envNoiseDecay--;
		} else
			envNoiseDivide--;
	}

	if (triLinReload)
		triLinear = (triControl&0x7f);
	else if (triLinear)
		triLinear--;
	if (!(triControl&0x80))
		triLinReload = 0;
}

void init_sounds () {
	AudioSettings.freq = SAMPLES_PER_SEC;
	AudioSettings.format = AUDIO_U16LSB;
	AudioSettings.channels = CHANNELS;
	AudioSettings.callback = NULL;
	AudioSettings.samples = BUFFER_SIZE>>3;
	SDL_OpenAudio(&AudioSettings, 0);
	SDL_PauseAudio(0);
	SDL_ClearQueuedAudio(1);
}

void output_sound () {
	uint16_t outBuffer = (BUFFER_SIZE>>1) - (SDL_GetQueuedAudioSize(1)>>1);
	uint16_t *SoundBuffer = malloc(outBuffer*sizeof(uint16_t));
/*	for(int outSampleIndex = 0;outSampleIndex < BufferSize;++outSampleIndex) {
	    for (int inSampleIndex = 0;inSampleIndex < 40;++inSampleIndex) {
	    	tmpcounter++;
	    	if (tmpcounter > BUFFER_SIZE)
	    		tmpcounter = 0;
	    	tmpval += pulse2Buffer[tmpcounter];
	    }
	    SoundBuffer[outSampleIndex] = ((tmpval / 40)*256);
		fprintf(logfile,"%i\n",SoundBuffer[outSampleIndex]);
	    tmpval = 0;
	} */
	for(int i=0;i<outBuffer;++i) {
		SoundBuffer[i] = pulseBuffer[pulseQueueCounter];
		pulseQueueCounter++;
		if (pulseQueueCounter >= BUFFER_SIZE)
			pulseQueueCounter = 0;
	}

/*	printf("%i\t%i\t%i\n",pulseSampleCounter,pulseQueueCounter,SDL_GetQueuedAudioSize(1)); */
	SDL_QueueAudio(1, SoundBuffer, (outBuffer<<1));
	free(SoundBuffer);
}
/*
double createLowpass (final int order, final double fc, double fs) {
    double cutoff = fc / fs;
    double fir[order + 1];
    double factor = 2.0 * cutoff;
    int half = order >> 1;
    for(int i = 0; i < fir.length; i++)
    {
        fir[i] = factor * sinc(factor * (i - half));
    }
    return fir;
}

double sinc(double x)
{
    if(x != 0)
    {
        double xpi = Math.PI * x;
        return Math.sin(xpi) / xpi;
    }
    return 1.0;
}

double[] windowBlackman(double fir)
{
    int m = fir.length - 1;
    for(int i = 0; i < fir.length; i++)
    {
        fir[i] *= 0.42
            - 0.5 * Math.cos(2.0 * Math.PI * i / m)
            + 0.08 * Math.cos(4.0 * Math.PI * i / m);
    }
    return fir;
}
*/
