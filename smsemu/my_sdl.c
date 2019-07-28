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
SDL_Texture *whiteboard;
SDL_Rect SrcR, TrgR, MenuR;
uint_fast8_t isPaused = 0, fullscreen = 0, stateSave = 0, stateLoad = 0, vsync = 0, throttle = 1, showMenu = 0;
sdlSettings *currentSettings;
float frameTime, fps;
int clockRate;

static inline void render_window (windowHandle *, uint32_t *), idle_time(float), create_handle (windowHandle *), draw_menu(void);
static inline float diff_time(struct timespec *, struct timespec *);

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
	if((whiteboard = SDL_CreateTexture(currentSettings->window.rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, currentSettings->window.winWidth, currentSettings->window.winHeight)) == NULL){
		printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);}
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
	if((handle->rend = SDL_CreateRenderer(handle->win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE)) ==NULL){
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

struct timespec throttleClock, startClock, endClock;
int frameCounter;
void init_time (float time)
{
	frameCounter = 0;
	clock_gettime(CLOCK_MONOTONIC, &startClock);
	clock_getres(CLOCK_MONOTONIC, &throttleClock);
	clock_gettime(CLOCK_MONOTONIC, &throttleClock);
	throttleClock.tv_nsec += time;
	throttleClock.tv_sec += throttleClock.tv_nsec / 1000000000;
	throttleClock.tv_nsec %= 1000000000;
}
float xfps;
void idle_time (float time)
{
	frameCounter++;
	if(frameCounter == 60){
		clock_gettime(CLOCK_MONOTONIC, &endClock);
		xfps = (float)((frameCounter / diff_time(&startClock, &endClock) + xfps) * .5);
		if(!throttle){
			fps = xfps;
			printf("%f\n",fps);
			set_timings(2);
		}
		frameCounter = 0;
		clock_gettime(CLOCK_MONOTONIC, &startClock);
	}
	if(throttle){
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &throttleClock, NULL);
	throttleClock.tv_nsec += time;
	throttleClock.tv_sec += throttleClock.tv_nsec / 1000000000;
	throttleClock.tv_nsec %= 1000000000;
	}
}
float diff_time(struct timespec *start, struct timespec *end){
	float temp;
	if ((end->tv_nsec-start->tv_nsec)<0) {
		temp = end->tv_sec - start->tv_sec - 1;
		temp += (1 + ((float)(end->tv_nsec-start->tv_nsec) / 1000000000));
	} else {
		temp = end->tv_sec - start->tv_sec;
		temp += ((float)(end->tv_nsec-start->tv_nsec) / 1000000000);
	}
	return temp;
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

void render_window (windowHandle * handle, uint32_t * buffer)
{
	SrcR.x = handle->xClip;
	SrcR.y = handle->yClip;
	SrcR.w = handle->screenWidth - (handle->xClip << 1);
	SrcR.h = handle->screenHeight - (handle->yClip << 1);
	TrgR.x = 240;
	TrgR.y = 0;
	TrgR.w = 1440;
	TrgR.h = 1080;
	MenuR.w = 100;
	MenuR.x = (handle->winWidth - (handle->winWidth >> 1));
	MenuR.h = 100;
	MenuR.x = (handle->winHeight - (handle->winHeight >> 1));
	if(SDL_UpdateTexture(handle->tex, NULL, buffer, handle->screenWidth * sizeof(uint32_t))){
		printf("SDL_UpdateTexture failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	if (fullscreen)
		SDL_RenderCopy(handle->rend, handle->tex, &SrcR, &TrgR);
	else{
		SDL_SetRenderTarget(handle->rend, whiteboard);
		SDL_RenderCopy(handle->rend, handle->tex, &SrcR, NULL);
		if(showMenu)
			draw_menu();
	}
	SDL_SetRenderTarget(handle->rend, NULL);
	SDL_RenderCopy(handle->rend, whiteboard, NULL, NULL);
	SDL_RenderPresent(handle->rend);
	SDL_RenderClear(handle->rend);
}

void draw_menu(){
    SDL_RenderDrawRect(currentSettings->window.rend,&MenuR);
    SDL_SetRenderDrawColor(currentSettings->window.rend, 0xFF, 0x00, 0x00, 0x00);
    SDL_RenderFillRect(currentSettings->window.rend, &MenuR);
}

void output_sound()
{
	if (!throttle)
		SDL_ClearQueuedAudio(1);
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
				throttle ^= 1;
				if (throttle)
					init_time(frameTime);
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
					currentSettings->window.tex = SDL_CreateTexture(currentSettings->window.rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, currentSettings->window.screenWidth, currentSettings->window.screenHeight);
					fps = current.refresh_rate;
					set_timings(2);
				}
				else
				{
					SDL_DestroyRenderer(currentSettings->window.rend);
					currentSettings->window.rend = SDL_CreateRenderer(currentSettings->window.win, -1, SDL_RENDERER_ACCELERATED);
					currentSettings->window.tex = SDL_CreateTexture(currentSettings->window.rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, currentSettings->window.screenWidth, currentSettings->window.screenHeight);
					set_timings(1);
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
			case SDL_SCANCODE_Q:
				showMenu ^= 1;
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
