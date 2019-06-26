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
extern windowHandle handleMain, handleNametable;
extern uint_fast8_t nametableActive, patternActive, paletteActive, isPaused, stateSave, stateLoad;

SDL_Window *windowMain, *windowNametable;
SDL_Renderer *renderMain, *renderNametable;
SDL_Texture *textureMain, *textureNametable;
SDL_Surface *surfaceMain, *surfaceNametable;
SDL_Event event;
SDL_Color colors[64];

void render_frame(), init_sdl(void), close_sdl(void), init_sounds(void), output_sound(void), destroy_handle (windowHandle *), io_handle(void), init_time(void);
windowHandle create_handle (char *, int, int, int, int, int, int, int, int);

#endif /* MY_SDL_H_ */
