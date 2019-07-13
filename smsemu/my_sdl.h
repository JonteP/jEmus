#ifndef MY_SDL_H_
#define MY_SDL_H_

#include <stdint.h>
#include "SDL.h"

typedef struct windowHandle_ {
	SDL_Window *win;
	SDL_Renderer *rend;
	SDL_Texture *tex;
	char *name;
	int winXPosition;
	int winYPosition;
	int winWidth;
	int winHeight;
	int screenWidth;
	int screenHeight;
	int windowID;
	int visible;
	int xClip;
	int yClip;
} windowHandle;
typedef struct sdlSettings {
	const char* renderQuality;
	uint8_t* ctable;
	int audioFrequency;
	int channels;
	int audioBufferSize;
	windowHandle window;
} sdlSettings;
extern uint_fast8_t isPaused, stateSave, stateLoad;

void render_frame(), init_sdl(sdlSettings*), close_sdl(void), init_sounds(void), output_sound(void), destroy_handle (windowHandle *), io_handle(void), init_time(float);

#endif /* MY_SDL_H_ */
