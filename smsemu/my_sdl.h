#ifndef MY_SDL_H_
#define MY_SDL_H_

#include <stdint.h>
#include "SDL.h"

#define MAX_MENU_ITEMS			10
#define MAX_MENU_ITEM_LENGTH	200

typedef void (*io_function)();
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
typedef struct menuItem menuItem;
struct menuItem {
	int length;
	char name[MAX_MENU_ITEMS][MAX_MENU_ITEM_LENGTH];
	uint8_t orientation;
	int xOffset[MAX_MENU_ITEMS];
	int yOffset[MAX_MENU_ITEMS];
	int width;
	int height;
	int margin;
	menuItem *parent;
	io_function ioFunction;
};
extern uint_fast8_t isPaused, stateSave, stateLoad;
extern float frameTime, fps;
extern int clockRate;

void render_frame(), init_sdl(sdlSettings*), close_sdl(void), init_sounds(void), output_sound(void), destroy_handle (windowHandle *), init_time(float);

#endif /* MY_SDL_H_ */
