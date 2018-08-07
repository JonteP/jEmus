/*
 * my_sdl.h
 *
 *  Created on: Aug 5, 2018
 *      Author: jonas
 */

#ifndef MY_SDL_H_
#define MY_SDL_H_

#include <stdint.h>
#include "SDL.h"

typedef struct windowHandle_ {
	SDL_Window *win;
	SDL_Renderer *rend;
	char *name;
	int winXPosition;
	int winYPosition;
	int winWidth;
	int winHeight;
	int screenWidth;
	int screenHeight;
} windowHandle;
extern windowHandle handleMain, handleNametable;
extern uint8_t nametableActive, patternActive, paletteActive;

SDL_Window *windowMain, *windowNametable;
SDL_Renderer *renderMain, *renderNametable;
SDL_Texture *textureMain, *textureNametable;
SDL_Surface *surfaceMain, *surfaceNametable;
SDL_Event event;
SDL_Color colors[64];

void render_frame(), init_graphs(void), close_sdl(void), init_sounds(void), output_sound(void), destroy_handle (windowHandle *), io_handle(void);
windowHandle create_handle (char *, int, int, int, int, int, int);

#endif /* MY_SDL_H_ */
