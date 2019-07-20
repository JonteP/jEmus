/* Good source for text boxes: https://gamedev.stackexchange.com/questions/140294/what-is-the-most-efficient-way-to-render-a-textbox-in-c-sdl2
 * Good source for file browsing: https://stackoverflow.com/questions/612097/how-can-i-get-the-list-of-files-in-a-directory-using-c-or-c
 */
#include "my_sdl.h"
#include "SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h> 	/* clock */
#include <unistd.h> /* usleep */
#include "smsemu.h"
#include "sn79489.h"
#include "z80.h"

SDL_AudioSpec wantedAudioSettings, audioSettings;
SDL_Event event;
SDL_DisplayMode current;
uint_fast8_t isPaused = 0, fullscreen = 0, stateSave = 0, stateLoad = 0, vsync = 0;
sdlSettings *currentSettings;


static inline void render_window (windowHandle *, void *), idle_time(float), update_texture(windowHandle *, uint_fast8_t *), create_handle (windowHandle *);

void init_sdl(sdlSettings *settings) {
	currentSettings = settings;
	SDL_version ver;
	SDL_GetVersion(&ver);
	printf("Running SDL version: %d.%d.%d\n",ver.major,ver.minor,ver.patch);
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
	{
		printf("SDL_Init failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,currentSettings->renderQuality);
	create_handle (&currentSettings->window);
	SDL_ShowWindow(currentSettings->window.win);
	wantedAudioSettings.freq = currentSettings->audioFrequency;
	wantedAudioSettings.format = AUDIO_F32;
	wantedAudioSettings.channels = currentSettings->channels;
	wantedAudioSettings.callback = NULL;
	wantedAudioSettings.samples = currentSettings->audioBufferSize>>1; /* Must be less than buffer size to prevent increasing lag... */
	if (SDL_OpenAudio(&wantedAudioSettings, &audioSettings) < 0)
	    SDL_Log("Failed to open audio: %s", SDL_GetError());
	else if (audioSettings.format != wantedAudioSettings.format)
	    SDL_Log("The desired audio format is not available.");
	SDL_PauseAudio(0);
	SDL_ClearQueuedAudio(1);
}

void create_handle (windowHandle *handle) {
	/* TODO: add clipping options here */
	if((handle->win = SDL_CreateWindow(handle->name, handle->winXPosition, handle->winYPosition, handle->winWidth, handle->winHeight, SDL_WINDOW_RESIZABLE)) == NULL){
		printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);}
	if((handle->rend = SDL_CreateRenderer(handle->win, -1, SDL_RENDERER_ACCELERATED)) ==NULL){
		printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);}
	if((handle->tex = SDL_CreateTexture(handle->rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, handle->screenWidth, handle->screenHeight)) == NULL){
		printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);}
	handle->windowID = SDL_GetWindowID(handle->win);
	SDL_HideWindow(handle->win);
}

void destroy_handle (windowHandle * handle) {
	SDL_DestroyTexture(currentSettings->window.tex);
	SDL_DestroyRenderer(currentSettings->window.rend);
	SDL_DestroyWindow(currentSettings->window.win);
}

void close_sdl() {
	destroy_handle (&currentSettings->window);
	SDL_ClearQueuedAudio(1);
	SDL_CloseAudio();
	SDL_Quit();
}

struct timespec xClock;
void init_time (float time)
{
	clock_getres(CLOCK_MONOTONIC, &xClock);
	clock_gettime(CLOCK_MONOTONIC, &xClock);
	xClock.tv_nsec += time;
	xClock.tv_sec += xClock.tv_nsec / 1000000000;
	xClock.tv_nsec %= 1000000000;
}

void idle_time (float time)
{
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &xClock, NULL);
	xClock.tv_nsec += time;
	xClock.tv_sec += xClock.tv_nsec / 1000000000;
	xClock.tv_nsec %= 1000000000;
}

void render_frame()
{
	render_window (&currentSettings->window, screenBuffer);
	idle_time(frameTime);
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
    		color = (currentSettings->ctable + ((*(buffer + row * handle->screenWidth + col)) * 3));
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
{
	if (SDL_QueueAudio(1, sampleBuffer, (sampleCounter<<2)))
		printf("SDL_QueueAduio failed: %s\n", SDL_GetError());
	sampleCounter = 0;
}

void io_handle()
{
	while (SDL_PollEvent(&event)) {
		if (event.window.windowID == currentSettings->window.windowID)
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
					SDL_GetWindowDisplayMode(currentSettings->window.win, &mode);
					mode.w = 1920;
					mode.h = 1080;
					mode.refresh_rate = 60;
					SDL_SetWindowDisplayMode(currentSettings->window.win, &mode);
					SDL_SetWindowFullscreen(currentSettings->window.win, SDL_WINDOW_FULLSCREEN);
				    SDL_ShowCursor(SDL_DISABLE);
					SDL_SetWindowGrab(currentSettings->window.win, SDL_TRUE);
				}
				else if (!fullscreen)
				{
					SDL_SetWindowFullscreen(currentSettings->window.win, 0);
				    SDL_ShowCursor(SDL_ENABLE);
					SDL_SetWindowGrab(currentSettings->window.win, SDL_FALSE);
				}
				break;
			case SDL_SCANCODE_F12:
				vsync ^= 1;
				if (vsync)
				{
					SDL_GetCurrentDisplayMode(0, &current);
					SDL_DestroyRenderer(currentSettings->window.rend);
					currentSettings->window.rend = SDL_CreateRenderer(currentSettings->window.win, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
					currentSettings->window.tex = SDL_CreateTexture(currentSettings->window.rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 240);
					fps = current.refresh_rate;
					frameTime = ((1/fps) * 1000000000);
				}
				else
				{
					SDL_DestroyRenderer(currentSettings->window.rend);
					currentSettings->window.rend = SDL_CreateRenderer(currentSettings->window.win, -1, SDL_RENDERER_ACCELERATED);
					currentSettings->window.tex = SDL_CreateTexture(currentSettings->window.rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 240);
					fps = 60;
					frameTime = ((1/fps) * 1000000000);
				}
				break;
			case SDL_SCANCODE_P:
				if (!(event.key.repeat))
					isPaused ^= 1;
				break;
			case SDL_SCANCODE_UP:
				ioPort1 &= ~IO1_PORTA_UP;
				break;
			case SDL_SCANCODE_DOWN:
				ioPort1 &= ~IO1_PORTA_DOWN;
				break;
			case SDL_SCANCODE_LEFT:
				ioPort1 &= ~IO1_PORTA_LEFT;
				break;
			case SDL_SCANCODE_RIGHT:
				ioPort1 &= ~IO1_PORTA_RIGHT;
				break;
			case SDL_SCANCODE_RETURN:
				nmiPulled = 1;
				break;
			case SDL_SCANCODE_BACKSPACE:
				ioPort2 &= ~IO2_RESET;
				break;
			case SDL_SCANCODE_Z:
				ioPort1 &= ~IO1_PORTA_TL;
				break;
			case SDL_SCANCODE_X:
				ioPort1 &= ~IO1_PORTA_TR;
				break;
			case SDL_SCANCODE_I:
				ioPort1 &= ~IO1_PORTB_UP;
				break;
			case SDL_SCANCODE_K:
				ioPort1 &= ~IO1_PORTB_DOWN;
				break;
			case SDL_SCANCODE_J:
				ioPort2 &= ~IO2_PORTB_LEFT;
				break;
			case SDL_SCANCODE_L:
				ioPort2 &= ~IO2_PORTB_RIGHT;
				break;
			case SDL_SCANCODE_A:
				ioPort2 &= ~IO2_PORTB_TL;
				break;
			case SDL_SCANCODE_S:
				ioPort2 &= ~IO2_PORTB_TR;
				break;
			default:
				break;
			}
			break;
		case SDL_KEYUP:
			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_UP:
				ioPort1 |= IO1_PORTA_UP;
				break;
			case SDL_SCANCODE_DOWN:
				ioPort1 |= IO1_PORTA_DOWN;
				break;
			case SDL_SCANCODE_LEFT:
				ioPort1 |= IO1_PORTA_LEFT;
				break;
			case SDL_SCANCODE_RIGHT:
				ioPort1 |= IO1_PORTA_RIGHT;
				break;
			case SDL_SCANCODE_BACKSPACE:
				ioPort2 |= IO2_RESET;
				break;
			case SDL_SCANCODE_Z:
				ioPort1 |= IO1_PORTA_TL;
				break;
			case SDL_SCANCODE_X:
				ioPort1 |= IO1_PORTA_TR;
				break;
			case SDL_SCANCODE_I:
				ioPort1 |= IO1_PORTB_UP;
				break;
			case SDL_SCANCODE_K:
				ioPort1 |= IO1_PORTB_DOWN;
				break;
			case SDL_SCANCODE_J:
				ioPort2 |= IO2_PORTB_LEFT;
				break;
			case SDL_SCANCODE_L:
				ioPort2 |= IO2_PORTB_RIGHT;
				break;
			case SDL_SCANCODE_A:
				ioPort2 |= IO2_PORTB_TL;
				break;
			case SDL_SCANCODE_S:
				ioPort2 |= IO2_PORTB_TR;
				break;
			default:
				break;
			}
			break;
		case SDL_QUIT:
			break;
		default:
			break;
		}
		}
	}
}
