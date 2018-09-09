#include "my_sdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h> 	/* clock */
#include <unistd.h> /* usleep */
#include "ppu.h"
#include "apu.h"
#include "globals.h"
#include "6502.h"

SDL_AudioSpec AudioSettings = {0};
uint8_t nametableActive = 0, patternActive = 0, paletteActive = 0, isPaused = 0, fullscreen = 0;
uint16_t pulseQueueCounter = 0;

					/*       00      |      01      |      02      |      03      |        */
					/*  R  | G  | B  | R  | G  | B  | R  | G  | B  | R  | G  | B  |        */
uint8_t colarray[] = { 124, 124, 124,   0,   0, 252,   0,   0, 188,  68,  40, 188, /* 0x00 */
					   148,   0, 132, 168,   0,  32, 168,  16,   0, 136,  20,   0, /* 0x04 */
					    80,  48,   0,   0, 120,   0,   0, 104,   0,   0,  88,   0, /* 0x08 */
						 0,  64,  88,   0,   0,   0,   0,   0,   0,   0,   0,   0, /* 0x0c */
					   188, 188, 188,   0, 120, 248,   0,  88, 248, 104,  68, 252, /* 0x10 */
					   216,   0, 204, 228,   0,  88, 248,  56,   0, 228,  92,  16, /* 0x14 */
					   172, 124,   0,   0, 184,   0,   0, 168,   0,   0, 168,  68, /* 0x18 */
					     0, 136, 136,   0,   0,   0,   0,   0,   0,   0,   0,   0, /* 0x1c */
					   248, 248, 248,  60, 188, 252, 104, 136, 252, 152, 120, 248, /* 0x20 */
					   248, 120, 248, 248,  88, 152, 248, 120,  88, 252, 160,  68, /* 0x24 */
					   248, 184,   0, 184, 248,  24,  88, 216,  84,  88, 248, 152, /* 0x28 */
					     0, 232, 216, 120, 120, 120,   0,   0,   0,   0,   0,   0, /* 0x2c */
					   252, 252, 252, 164, 228, 252, 184, 184, 248, 216, 184, 248, /* 0x30 */
					   248, 184, 248, 248, 164, 192, 240, 208, 176, 252, 224, 168, /* 0x34 */
					   248, 216, 120, 216, 248, 120, 184, 248, 184, 184, 248, 216, /* 0x38 */
					     0, 252, 252, 248, 216, 248,   0,   0,   0,   0,   0,   0  /* 0x3c */
};

					 /*       00      |      01      |      02      |      03      |        */
					 /*  R  | G  | B  | R  | G  | B  | R  | G  | B  | R  | G  | B  |        */
uint8_t colblargg[] = {  84,  84,  84,   0,  30, 116,   8,  16, 144,  48,   0, 136,
					     68,   0, 100,  92,   0,  48,  84,   4,   0,  60,  24,   0,
						 32,  42,   0,   8,  58,   0,   0,  64,   0,   0,  60,   0,
						  0,  50,  60,   0,   0,   0,   0,   0,   0,   0,   0,   0,
						152, 150, 152,   8,  76, 196,  48,  50, 236,  92,  30, 228,
						136,  20, 176, 160,  20, 100, 152,  34,  32, 120,  60,   0,
						 84,  90,   0,  40, 114,   0,   8, 124,   0,   0, 118,  40,
						  0, 102, 120,   0,   0,   0,   0,   0,   0,   0,   0,   0,
						236, 238, 236,  76, 154, 236, 120, 124, 236, 176,  98, 236,
						228,  84, 236, 236,  88, 180, 236, 106, 100, 212, 136,  32,
						160, 170,   0, 116, 196,   0,  76, 208,  32,  56, 204, 108,
						 56, 180, 204,  60,  60,  60,   0,   0,   0,   0,   0,   0,
						236, 238, 236, 168, 204, 236, 188, 188, 236, 212, 178, 236,
						236, 174, 236, 236, 174, 212, 236, 180, 176, 228, 196, 144,
						204, 210, 120, 180, 222, 120, 168, 226, 144, 152, 226, 180,
						160, 214, 228, 160, 162, 160,   0,   0,   0,   0,   0,   0 };

windowHandle handleMain, handleNametable, handlePattern, handlePalette;

static inline void render_window (windowHandle, uint8_t *), idle_time();

void init_sdl() {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"2");
	for (int i = 0; i < 64; i++) {
		colors[i].r = colblargg[i * 3];
		colors[i].g = colblargg[i * 3 + 1];
		colors[i].b = colblargg[i * 3 + 2];
	}
	handleMain = create_handle ("jNes", 100, 100, WWIDTH, WHEIGHT, SWIDTH, SHEIGHT);
	AudioSettings.freq = SAMPLES_PER_SEC;
	AudioSettings.format = AUDIO_F32;
	AudioSettings.channels = CHANNELS;
	AudioSettings.callback = NULL;
	AudioSettings.samples = BUFFER_SIZE>>1;
	SDL_OpenAudio(&AudioSettings, 0);
	SDL_PauseAudio(0);
	SDL_ClearQueuedAudio(1);
}

windowHandle create_handle (char * name, int wpx, int wpy, int ww, int wh, int sw, int sh) {
	windowHandle handle;
	handle.name = name;
	handle.winXPosition = wpx;
	handle.winYPosition = wpy;
	handle.winWidth = ww;
	handle.winHeight = wh;
	handle.screenWidth = sw;
	handle.screenHeight = sh;
	handle.win = SDL_CreateWindow(handle.name, handle.winXPosition, handle.winYPosition, handle.winWidth, handle.winHeight, SDL_WINDOW_RESIZABLE);
	handle.rend = SDL_CreateRenderer(handle.win, -1, SDL_RENDERER_ACCELERATED);
	return handle;
}

void destroy_handle (windowHandle * handle) {
	SDL_DestroyRenderer(handle->rend);
	SDL_DestroyWindow(handle->win);
}

void close_sdl () {
	destroy_handle (&handleMain);
	SDL_ClearQueuedAudio(1);
	SDL_CloseAudio();
	SDL_Quit();
	exit(EXIT_SUCCESS);
}

struct timespec xClock;
void init_time ()
{
	clock_getres(CLOCK_MONOTONIC, &xClock);
	clock_gettime(CLOCK_MONOTONIC, &xClock);
	xClock.tv_nsec += FRAMETIME;
	xClock.tv_sec += xClock.tv_nsec / 1000000000;
	xClock.tv_nsec %= 1000000000;
}

void idle_time ()
{
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &xClock, NULL);
	xClock.tv_nsec += FRAMETIME;
	xClock.tv_sec += xClock.tv_nsec / 1000000000;
	xClock.tv_nsec %= 1000000000;
}

void render_frame()
{
	io_handle();
	if (nametableActive)
		draw_nametable();
	if (patternActive)
		draw_pattern();
	if (paletteActive)
		draw_palette();
	render_window (handleMain, (void *)frameBuffer);
	if (nametableActive)
		render_window (handleNametable, (void *)nameBuffer);
	if (patternActive)
		render_window (handlePattern, (void *)patternBuffer);
	if (paletteActive)
		render_window (handlePalette, (void *)paletteBuffer);
	if (throttle)
	{
		idle_time();
	}
	while (isPaused)
	{
		render_frame();
	}
}

void render_window (windowHandle handle, uint8_t * buffer)
{
	SDL_Rect SrcR;
	  SrcR.x = 0;
	  SrcR.y = 8;
	  SrcR.w = 256;
	  SrcR.h = 224;

	SDL_Texture *tex;
	SDL_Surface *surf;
	surf = SDL_CreateRGBSurfaceFrom(buffer, handle.screenWidth, handle.screenHeight, 8, handle.screenWidth, 0, 0, 0, 0);
	SDL_SetPaletteColors(surf->format->palette, colors, 0, 64);
	tex = SDL_CreateTextureFromSurface(handle.rend, surf);
	SDL_RenderCopy(handle.rend, tex, &SrcR, NULL);
	SDL_DestroyTexture(tex);
	SDL_RenderPresent(handle.rend);
	SDL_RenderClear(handle.rend);
	SDL_FreeSurface(surf);
}

void output_sound()
{
	int outBuffer;
	float *SoundBuffer;
	uint32_t a = SDL_GetQueuedAudioSize(1);
	if (BUFFER_SIZE > (a/(sizeof(float))))
	{
		if (sampleCounter <= (BUFFER_SIZE - (a/(sizeof(float)))))
			outBuffer = sampleCounter;
		else
			outBuffer = (sampleCounter - (BUFFER_SIZE - (a/(sizeof(float)))));
		SoundBuffer = malloc(outBuffer*sizeof(float));
		for(int i=0;i<outBuffer;i++) {
			SoundBuffer[i] = sampleBuffer[i];
		}

	int audioError = SDL_QueueAudio(1, SoundBuffer, (outBuffer<<2));
	if (audioError)
		printf("SDL_QueueAduio failed: %s\n", SDL_GetError());
	free(SoundBuffer);
	}
	else
		printf("skip\n");
	sampleCounter = 0;
}

void io_handle()
{
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		/* Keyboard event */
		/* Pass the event data onto PrintKeyInfo() */
		case SDL_KEYDOWN:
			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_ESCAPE:
				printf("Quitting\n");
				quit = 1;
				isPaused = 0;
				break;
			case SDL_SCANCODE_F3:
				printf("Resetting\n");
				rstFlag = SOFT_RESET;
				isPaused = 0;
				break;
			case SDL_SCANCODE_F10:
				throttle ^= 1;
				if (throttle)
					init_time();
				break;
			case SDL_SCANCODE_F11:
				fullscreen ^= 1;
				if (fullscreen)
					SDL_SetWindowFullscreen(handleMain.win, SDL_WINDOW_FULLSCREEN);
				else if (!fullscreen)
					SDL_SetWindowFullscreen(handleMain.win, 0);
				break;
			case SDL_SCANCODE_P:
				isPaused ^= 1;
				break;
			case SDL_SCANCODE_N:
				nametableActive ^= 1;
				if (nametableActive)
					handleNametable = create_handle ("Nametable", 1000, 100, WWIDTH, WHEIGHT>>1, SWIDTH<<1, SHEIGHT);
				else if (!nametableActive)
					destroy_handle (&handleNametable);
				break;
			case SDL_SCANCODE_O:
				patternActive ^= 1;
				if (patternActive)
					handlePattern = create_handle ("Pattern table", 1000, 500, WWIDTH, WHEIGHT>>1, SWIDTH, SWIDTH>>1);
				else if (!patternActive)
					destroy_handle (&handlePattern);
				break;
			case SDL_SCANCODE_C:
				paletteActive ^= 1;
				if (paletteActive)
					handlePalette = create_handle ("Palette", 1000, 1000, WWIDTH>>1, WHEIGHT>>4, SWIDTH>>1, SWIDTH>>4);
				else if (!paletteActive)
					destroy_handle (&handlePalette);
				break;
			case SDL_SCANCODE_UP:
				bitset(&ctr1, 1, 4);
				break;
			case SDL_SCANCODE_DOWN:
				bitset(&ctr1, 1, 5);
				break;
			case SDL_SCANCODE_LEFT:
				bitset(&ctr1, 1, 6);
				break;
			case SDL_SCANCODE_RIGHT:
				bitset(&ctr1, 1, 7);
				break;
			case SDL_SCANCODE_RETURN:
				bitset(&ctr1, 1, 3);
				break;
			case SDL_SCANCODE_BACKSPACE:
				bitset(&ctr1, 1, 2);
				break;
			case SDL_SCANCODE_Z:
				bitset(&ctr1, 1, 1);
				break;
			case SDL_SCANCODE_X:
				bitset(&ctr1, 1, 0);
				break;
			case SDL_SCANCODE_I:
				bitset(&ctr2, 1, 4);
				break;
			case SDL_SCANCODE_K:
				bitset(&ctr2, 1, 5);
				break;
			case SDL_SCANCODE_J:
				bitset(&ctr2, 1, 6);
				break;
			case SDL_SCANCODE_L:
				bitset(&ctr2, 1, 7);
				break;
			case SDL_SCANCODE_1:
				bitset(&ctr2, 1, 3);
				break;
			case SDL_SCANCODE_2:
				bitset(&ctr2, 1, 2);
				break;
			case SDL_SCANCODE_A:
				bitset(&ctr2, 1, 1);
				break;
			case SDL_SCANCODE_S:
				bitset(&ctr2, 1, 0);
				break;
			default:
				break;
			}
			break;
		case SDL_KEYUP:
			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_UP:
				bitset(&ctr1, 0, 4);
				break;
			case SDL_SCANCODE_DOWN:
				bitset(&ctr1, 0, 5);
				break;
			case SDL_SCANCODE_LEFT:
				bitset(&ctr1, 0, 6);
				break;
			case SDL_SCANCODE_RIGHT:
				bitset(&ctr1, 0, 7);
				break;
			case SDL_SCANCODE_RETURN:
				bitset(&ctr1, 0, 3);
				break;
			case SDL_SCANCODE_BACKSPACE:
				bitset(&ctr1, 0, 2);
				break;
			case SDL_SCANCODE_Z:
				bitset(&ctr1, 0, 1);
				break;
			case SDL_SCANCODE_X:
				bitset(&ctr1, 0, 0);
				break;
			case SDL_SCANCODE_I:
				bitset(&ctr2, 0, 4);
				break;
			case SDL_SCANCODE_K:
				bitset(&ctr2, 0, 5);
				break;
			case SDL_SCANCODE_J:
				bitset(&ctr2, 0, 6);
				break;
			case SDL_SCANCODE_L:
				bitset(&ctr2, 0, 7);
				break;
			case SDL_SCANCODE_1:
				bitset(&ctr2, 0, 3);
				break;
			case SDL_SCANCODE_2:
				bitset(&ctr2, 0, 2);
				break;
			case SDL_SCANCODE_A:
				bitset(&ctr2, 0, 1);
				break;
			case SDL_SCANCODE_S:
				bitset(&ctr2, 0, 0);
				break;
			default:
				break;
			}
			break;
			/* SDL_QUIT event (window close) */
		case SDL_QUIT:
			break;
		default:
			break;
		}
	}
}
