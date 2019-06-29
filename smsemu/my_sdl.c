#include "my_sdl.h"
#include "SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h> 	/* clock */
#include <unistd.h> /* usleep */
#include "smsemu.h"
#include "vdp.h"
#include "sn76489.h"
#include "z80.h"

SDL_AudioSpec AudioSettings = {0};
SDL_DisplayMode current;
uint_fast8_t nametableActive = 0, patternActive = 0, paletteActive = 0, isPaused = 0, fullscreen = 0, stateSave = 0, stateLoad = 0, vsync = 0;
uint16_t pulseQueueCounter = 0;
float frameTime;
uint_fast8_t coltable[0xc0];

windowHandle handleMain, handleNametable, handlePattern, handlePalette;

static inline void render_window (windowHandle *, void *), idle_time(), update_texture(windowHandle *, uint_fast8_t *);

void init_sdl() {
	SDL_version ver;
	SDL_GetVersion(&ver);
	printf("Running SDL version: %d.%d.%d\n",ver.major,ver.minor,ver.patch);
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
	{
		printf("SDL_Init failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
	for (int i = 0; i < 0x40; i++) {
		coltable[i*3] = (((i & 0x03) << 6) | ((i & 0x03) << 4) | ((i & 0x03) << 2) | (i & 0x03));
		coltable[(i*3)+1] = (((i & 0x0c) << 4) | ((i & 0x0c) << 2) | ((i & 0x0c) >> 2) | (i & 0xc0));
		coltable[(i*3)+2] = (((i & 0x30) << 2) | ((i & 0x30) >> 4) | ((i & 0x30) >> 2) | (i & 0x30));
	}
	handleMain = create_handle ("jNes", 100, 100, WWIDTH<<1, WHEIGHT<<1, SWIDTH, SHEIGHT, 0, 0);
	handleNametable = create_handle ("Nametable", 1000, 100, WWIDTH<<1, WHEIGHT<<1, SWIDTH<<1, SHEIGHT<<1,0,0);
	handlePattern = create_handle ("Pattern", 1000, 100, WWIDTH<<1, WHEIGHT<<1, SWIDTH<<1, SHEIGHT<<1,0,0);
	handlePalette = create_handle ("Palette", 1000, 100, WWIDTH<<1, WHEIGHT<<1, SWIDTH<<1, SHEIGHT<<1,0,0);
	SDL_ShowWindow(handleMain.win);
	handleMain.visible = 1;
	AudioSettings.freq = samplesPerSecond;
	AudioSettings.format = AUDIO_F32;
	AudioSettings.channels = CHANNELS;
	AudioSettings.callback = NULL;
	AudioSettings.samples = BUFFER_SIZE>>3;
	SDL_OpenAudio(&AudioSettings, 0);
	SDL_PauseAudio(0);
	SDL_ClearQueuedAudio(1);
/*	originalSampleRate = cpuClock/samplesPerSecond;
	sampleRate = originalSampleRate;*/
	frameTime = ((1/fps) * 1000000000);
}

windowHandle create_handle (char * name, int wpx, int wpy, int ww, int wh, int sw, int sh, int xx, int yy) {
	/* TODO: add clipping options here */
	windowHandle handle;
	handle.name = name;
	handle.winXPosition = wpx;
	handle.winYPosition = wpy;
	handle.winWidth = ww;
	handle.winHeight = wh;
	handle.screenWidth = sw;
	handle.screenHeight = sh;
	handle.xClip = xx;
	handle.yClip = yy;
	handle.win = SDL_CreateWindow(handle.name, handle.winXPosition, handle.winYPosition, handle.winWidth, handle.winHeight, SDL_WINDOW_RESIZABLE);
	if (handle.win == NULL)
	{
		printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	handle.rend = SDL_CreateRenderer(handle.win, -1, SDL_RENDERER_ACCELERATED);
	if (handle.rend == NULL)
	{
		printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	handle.tex = SDL_CreateTexture(handle.rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, handle.screenWidth, handle.screenHeight);
	if (handle.tex == NULL)
	{
		printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	handle.windowID = SDL_GetWindowID(handle.win);
	SDL_HideWindow(handle.win);
	handle.visible = 0;
	return handle;
}

void destroy_handle (windowHandle * handle) {
	SDL_DestroyTexture(handle->tex);
	SDL_DestroyRenderer(handle->rend);
	SDL_DestroyWindow(handle->win);
}

void close_sdl () {
	destroy_handle (&handleMain);
	destroy_handle (&handleNametable);
	destroy_handle (&handlePattern);
	destroy_handle (&handlePalette);
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
	xClock.tv_nsec += frameTime;
	xClock.tv_sec += xClock.tv_nsec / 1000000000;
	xClock.tv_nsec %= 1000000000;
}

void idle_time ()
{
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &xClock, NULL);
	xClock.tv_nsec += frameTime;
	xClock.tv_sec += xClock.tv_nsec / 1000000000;
	xClock.tv_nsec %= 1000000000;
}

void render_frame()
{

	render_window (&handleMain, screenBuffer);
	idle_time();
	io_handle();
	while (isPaused)
	{
		render_frame();
	}
}

void update_texture(windowHandle *handle, uint_fast8_t * buffer)
{
	uint_fast8_t * color;
	Uint32 *dst;
	int row, col;
	void *pixels;
	int pitch;
	int texError = SDL_LockTexture(handle->tex, NULL, &pixels, &pitch);
	if (texError)
	{
		printf("SDL_LockTexture failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

   	for (row = 0; row < handle->screenHeight; ++row)
   	{
   		dst = (Uint32*)((Uint8*)pixels + row * pitch);
		for (col = 0; col < handle->screenWidth; ++col)
		{
    		color = coltable + ((*(buffer + row * handle->screenWidth + col)) * 3);
    		*dst++ = (0xFF000000|(color[0]<<16)|(color[1]<<8)|color[2]);
		}
    }
    SDL_UnlockTexture(handle->tex);
}



void render_window (windowHandle * handle, void * buffer)
{
	SDL_Rect SrcR;
	SrcR.x = handle->xClip;
	SrcR.y = handle->yClip;
	SrcR.w = handle->screenWidth - (handle->xClip << 1);
	SrcR.h = handle->screenHeight - (handle->yClip << 1);
	SDL_Rect TrgR;
	TrgR.x = 240;
	TrgR.y = 0;
	TrgR.w = 1440;
	TrgR.h = 1080;
	update_texture(handle, buffer);
	if (fullscreen)
		SDL_RenderCopy(handle->rend, handle->tex, &SrcR, &TrgR);
	else
		SDL_RenderCopy(handle->rend, handle->tex, &SrcR, NULL);
	SDL_RenderPresent(handle->rend);
	SDL_RenderClear(handle->rend);
}

void output_sound()
{/*
	if (!throttle)
		SDL_ClearQueuedAudio(1);
	int audioError = SDL_QueueAudio(1, sampleBuffer, (sampleCounter<<2));
	if (audioError)
		printf("SDL_QueueAduio failed: %s\n", SDL_GetError());
	sampleCounter = 0;*/
}

void io_handle()
{
	while (SDL_PollEvent(&event)) {
		if (event.window.windowID == handleMain.windowID)
		{
		switch (event.type) {
		/* Pass the event data onto PrintKeyInfo() */
		case SDL_KEYDOWN:
			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_ESCAPE:
				printf("Quitting\n");
				quit = 1;
				isPaused = 0;
				break;
			case SDL_SCANCODE_F1:
				printf("Saving state\n");
				stateSave = 1;
				break;
			case SDL_SCANCODE_F2:
				printf("Loading state\n");
				stateLoad = 1;
				break;
			case SDL_SCANCODE_F3:
				printf("Resetting\n");
				reset = 1;
				isPaused = 0;
				break;
			case SDL_SCANCODE_F10:
			/*	throttle ^= 1;
				if (throttle)
					init_time();*/
				break;
			case SDL_SCANCODE_F11:
				fullscreen ^= 1;
				if (fullscreen)
				{
					SDL_DisplayMode mode;
					SDL_GetWindowDisplayMode(handleMain.win, &mode);
					mode.w = 1920;
					mode.h = 1080;
					mode.refresh_rate = 60;
					SDL_SetWindowDisplayMode(handleMain.win, &mode);
					SDL_SetWindowFullscreen(handleMain.win, SDL_WINDOW_FULLSCREEN);
				    SDL_ShowCursor(SDL_DISABLE);
					SDL_SetWindowGrab(handleMain.win, SDL_TRUE);
				}
				else if (!fullscreen)
				{
					SDL_SetWindowFullscreen(handleMain.win, 0);
				    SDL_ShowCursor(SDL_ENABLE);
					SDL_SetWindowGrab(handleMain.win, SDL_FALSE);
				}
				break;
			case SDL_SCANCODE_F12:
				vsync ^= 1;
				if (vsync)
				{
					SDL_GetCurrentDisplayMode(0, &current);
					SDL_DestroyRenderer(handleMain.rend);
					handleMain.rend = SDL_CreateRenderer(handleMain.win, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
					handleMain.tex = SDL_CreateTexture(handleMain.rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 240);
					fps = current.refresh_rate;
					frameTime = ((1/fps) * 1000000000);
				/*	cpuClock = cyclesPerFrame * fps;*/
				}
				else
				{
					SDL_DestroyRenderer(handleMain.rend);
					handleMain.rend = SDL_CreateRenderer(handleMain.win, -1, SDL_RENDERER_ACCELERATED);
					handleMain.tex = SDL_CreateTexture(handleMain.rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 240);
					fps = 60;
					frameTime = ((1/fps) * 1000000000);
				/*	cpuClock = originalCpuClock;*/
				}
				break;
			case SDL_SCANCODE_P:
				if (!(event.key.repeat))
					isPaused ^= 1;
				break;
			case SDL_SCANCODE_F5:
				handleNametable.visible ^= 1;
				if (handleNametable.visible)
				{
					SDL_ShowWindow(handleNametable.win);
					SDL_RaiseWindow(handleMain.win);
				}
				else
					SDL_HideWindow(handleNametable.win);
				break;
			case SDL_SCANCODE_F6:
				handlePattern.visible ^= 1;
				if (handlePattern.visible)
				{
					SDL_ShowWindow(handlePattern.win);
					SDL_RaiseWindow(handleMain.win);
				}
				else
					SDL_HideWindow(handlePattern.win);
				break;
			case SDL_SCANCODE_F4:
				handlePalette.visible ^= 1;
				if (handlePalette.visible)
				{
					SDL_ShowWindow(handlePalette.win);
					SDL_RaiseWindow(handleMain.win);
				}

				else
					SDL_HideWindow(handlePalette.win);
				break;
			case SDL_SCANCODE_UP:
				ioPort1 &= ~0x01;
				break;
			case SDL_SCANCODE_DOWN:
				ioPort1 &= ~0x02;
				break;
			case SDL_SCANCODE_LEFT:
				ioPort1 &= ~0x04;
				break;
			case SDL_SCANCODE_RIGHT:
				ioPort1 &= ~0x08;
				break;
			case SDL_SCANCODE_RETURN:
				nmiPulled = 1;
				break;
			case SDL_SCANCODE_BACKSPACE:
				break;
			case SDL_SCANCODE_Z:
				ioPort1 &= ~0x10;
				break;
			case SDL_SCANCODE_X:
				ioPort1 &= ~0x20;
				break;
			/*case SDL_SCANCODE_I:
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
				break;*/
			default:
				break;
			}
			break;
		case SDL_KEYUP:
			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_UP:
				ioPort1 |= 0x01;
				break;
			case SDL_SCANCODE_DOWN:
				ioPort1 |= 0x02;
				break;
			case SDL_SCANCODE_LEFT:
				ioPort1 |= 0x04;
				break;
			case SDL_SCANCODE_RIGHT:
				ioPort1 |= 0x08;
				break;
			case SDL_SCANCODE_RETURN:
				break;
			case SDL_SCANCODE_BACKSPACE:
				break;
			case SDL_SCANCODE_Z:
				ioPort1 |= 0x10;
				break;
			case SDL_SCANCODE_X:
				ioPort1 |= 0x20;
				break;		/*	case SDL_SCANCODE_I:
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
				break;*/
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
}
