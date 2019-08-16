/* Good source for text boxes: https://gamedev.stackexchange.com/questions/140294/what-is-the-most-efficient-way-to-render-a-textbox-in-c-sdl2
 * Good source for file browsing: https://stackoverflow.com/questions/612097/how-can-i-get-the-list-of-files-in-a-directory-using-c-or-c
 */
#include "my_sdl.h"
#include "SDL.h"
#include "SDL_ttf.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <bsd/string.h>
#include <time.h> 	/* clock */
#include <unistd.h> /* usleep */
#include <dirent.h>
#include <sys/stat.h>
#include "smsemu.h"
#include "sn79489.h"
#include "z80.h"
#include "cartridge.h"

SDL_AudioSpec wantedAudioSettings, audioSettings;
SDL_Event event;
SDL_DisplayMode current;
SDL_Texture *whiteboard;
TTF_Font* Sans;
SDL_Color menuTextColor = {0xff, 0xff, 0xff, 0x00};
uint8_t menuBgColor[4] = {0x00, 0x00, 0x00, 0x00};
uint8_t menuActiveColor[4] = {0x80, 0x80, 0x80, 0x00};
SDL_Rect SrcR, TrgR;
uint_fast8_t isPaused = 0, fullscreen = 0, stateSave = 0, stateLoad = 0, vsync = 0, throttle = 1, showMenu = 0;
sdlSettings *currentSettings;
menuItem mainMenu, fileMenu, graphicsMenu, machineMenu, audioMenu, fileList, *currentMenu;
io_function io_func;
uint8_t currentMenuColumn = 0, currentMenuRow = 0, oldMenuRow = 0, menuFontSize = 24, filesLeft = 0;
DIR *currentDir;
char *defaultDir = "./", workDir[PATH_MAX];
int fileListOffset = 0, oldFileListOffset = 0;
float frameTime, fps;
int clockRate;

static inline void render_window (windowHandle *, uint32_t *), idle_time(float), create_handle (windowHandle *), draw_menu(menuItem *), set_menu(void), get_menu_size(menuItem *, int, int), create_menu(void), call_menu_option(void);
static inline void option_fullscreen(void), option_quit(void), option_open_file(void), game_io(void), menu_io(void), file_io(void), get_parent_dir(char *), add_slash(char *), toggle_menu(void);
static inline float diff_time(struct timespec *, struct timespec *);
static inline int is_directory(const char *), create_file_list(void), file_count(DIR *), fileSorter(const void *const, const void *const);
static inline struct dirent ** read_directory(DIR *);

/*****************/
/* SDL FUNCTIONS */
/*****************/

void init_sdl(sdlSettings *settings) {
	io_func = &game_io;
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
	if(TTF_Init()==-1) {
	    printf("TTF_Init failed: %s\n", TTF_GetError());
	    exit(EXIT_FAILURE);}
	Sans = TTF_OpenFont("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", menuFontSize);
	if(!Sans){
		printf("TTF_OpenFont failed: %s\n", TTF_GetError());
		exit(EXIT_FAILURE);}
	create_menu();
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
	TTF_CloseFont(Sans);
	SDL_ClearQueuedAudio(1);
	SDL_CloseAudio();
	SDL_Quit();
}

void render_window (windowHandle * handle, uint32_t * buffer)
{
	/* TODO: rects should probably be updated in separate function when necessary only */
	SrcR.x = handle->xClip;
	SrcR.y = handle->yClip;
	SrcR.w = handle->screenWidth - (handle->xClip << 1);
	SrcR.h = handle->screenHeight - (handle->yClip << 1);
	TrgR.x = 240;
	TrgR.y = 0;
	TrgR.w = 1440;
	TrgR.h = 1080;
	if(SDL_UpdateTexture(handle->tex, NULL, buffer, handle->screenWidth * sizeof(uint32_t))){
		printf("SDL_UpdateTexture failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	if (fullscreen)
		SDL_RenderCopy(handle->rend, handle->tex, &SrcR, &TrgR);
	else{
		SDL_SetRenderTarget(handle->rend, whiteboard);
		SDL_RenderCopy(handle->rend, handle->tex, &SrcR, NULL);
		if(showMenu){
			draw_menu(currentMenu);
			if(currentMenu->parent)
				draw_menu(currentMenu->parent);
		}
	}
	SDL_SetRenderTarget(handle->rend, NULL);
	SDL_RenderCopy(handle->rend, whiteboard, NULL, NULL);
	SDL_RenderPresent(handle->rend);
}


/****************/
/* TIME KEEPING */
/****************/

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

void render_frame(){
	render_window (&currentSettings->window, screenBuffer);
	idle_time(frameTime);
	io_func();
}

/****************/
/* FILE BROWSER */
/****************/

int is_directory(const char *str)
{
    struct stat path_stat;
    if(!stat(str, &path_stat)){
        return S_ISDIR(path_stat.st_mode);
    }
    else
    	return -1;
}

void get_parent_dir(char *str){
	add_slash(str); /* set string to known state */
	str[strlen(str) - 1] = '\0';
	char *ptr = strrchr(str, '/');
	if(ptr)
		*ptr = '\0';
	add_slash(str);
	/* Retrieve position of parent dir. TODO: more sophisticated */
	fileListOffset = oldFileListOffset;
	currentMenuRow = oldMenuRow;
	oldFileListOffset = 0;
	oldMenuRow = 1;
	create_file_list();
}

void append_to_dir(char *dir, const char *str){
	if(!strcmp(str, ".."))
		get_parent_dir(dir);
	else if(strcmp(str, ".")){
		sprintf(dir,"%s%s",dir,str);
		if(is_directory(dir) == 1){
			add_slash(dir);
			oldFileListOffset = fileListOffset;
			oldMenuRow = currentMenuRow;
			fileListOffset = 0;
			currentMenuRow = 1;
			create_file_list();
		}
		else if(!is_directory(dir)){
			toggle_menu();
			SDL_RenderClear(currentSettings->window.rend);
			if(cartFile)
				free(cartFile);
			cartFile = (char *)malloc(strlen(dir) + 1);
			strcpy(cartFile,dir);
			get_parent_dir(dir);
			init_slots();
			power_reset();
			init_time(frameTime);
		}
		else
			printf("Error: no such file or directory.\n");
	}
}

int file_count(DIR *dir){
	int fileCounter = 0;
	rewinddir(dir);
	while (readdir(dir))
		fileCounter++;
	return fileCounter;
}

int fileSorter(const void *const file1, const void *const file2){
    return strcmp((*(struct dirent **) file1)->d_name, (*(struct dirent **) file2)->d_name);
}

struct dirent ** read_directory(DIR *dir){
	struct dirent *entry, **files;
	int counter = 0, length;
	length = file_count(dir);
	files = malloc(length * sizeof(*files));
	rewinddir(dir);
	while((entry = readdir(dir)) != NULL)
		files[counter++] = entry;
	qsort(files, length, sizeof(*files), fileSorter);
	return files;
}

int create_file_list(){
	struct dirent **sortedFiles;
	uint8_t counter = 0;
	int length;
	if(!(currentDir = opendir(workDir))){
		printf("Error: failed to open directory: %s\n",workDir);
		return 1;
	}
	length = file_count(currentDir);
	sortedFiles = read_directory(currentDir);
	filesLeft = 1;
	for(int i = 0; i < MAX_MENU_ITEMS; i++){
		if((i + fileListOffset) < length){
			strcpy(fileList.name[i], sortedFiles[i + fileListOffset]->d_name);
			counter++;
		}
		else
			filesLeft = 0;
	}
	closedir(currentDir);
	free(sortedFiles);
	fileList.length = counter;
	get_menu_size(&fileList, 0, 0);
	fileList.width = (currentSettings->window.winWidth >> 1);
	for(int i = 0;i < fileList.length;i++){
		fileList.xOffset[i] = ((currentSettings->window.winWidth >> 1) - (fileList.width >> 1));
		fileList.yOffset[i] = ((currentSettings->window.winHeight >> 1) - (fileList.height * (fileList.length >> 1)) + fileList.yOffset[i]);
	}
	return 0;
}

void create_menu(){
	/* define menus */
	mainMenu.length = 4;
	mainMenu.margin = 4;
	strcpy(mainMenu.name[0], "File");
	strcpy(mainMenu.name[1], "Machine");
	strcpy(mainMenu.name[2], "Graphics");
	strcpy(mainMenu.name[3], "Audio");
	mainMenu.orientation = 1;
	mainMenu.ioFunction = &menu_io;
	mainMenu.parent = NULL;
	get_menu_size(&mainMenu, 0, 0);
	fileMenu.length = 4;
	fileMenu.margin = 4;
	strcpy(fileMenu.name[0], "Load ROM...");
	strcpy(fileMenu.name[1], "Load State");
	strcpy(fileMenu.name[2], "Save State");
	strcpy(fileMenu.name[3], "Quit");
	fileMenu.orientation = 0;
	fileMenu.parent = &mainMenu;
	get_menu_size(&fileMenu, mainMenu.xOffset[0], mainMenu.height + (mainMenu.margin << 1));
	machineMenu.length = 2;
	machineMenu.margin = 4;
	strcpy(machineMenu.name[0], "Emulated Machine...");
	strcpy(machineMenu.name[1], "Throttle (F10)");
	machineMenu.orientation = 0;
	machineMenu.parent = &mainMenu;
	get_menu_size(&machineMenu, mainMenu.xOffset[1], mainMenu.height + (mainMenu.margin << 1));
	graphicsMenu.length = 4;
	graphicsMenu.margin = 4;
	strcpy(graphicsMenu.name[0], "Fullscreen (F11)");
	strcpy(graphicsMenu.name[1], "Toggle Sprites");
	strcpy(graphicsMenu.name[2], "Toggle Background");
	strcpy(graphicsMenu.name[3], "Settings...");
	graphicsMenu.orientation = 0;
	graphicsMenu.parent = &mainMenu;
	get_menu_size(&graphicsMenu, mainMenu.xOffset[2], mainMenu.height + (mainMenu.margin << 1));
	audioMenu.length = 3;
	audioMenu.margin = 4;
	strcpy(audioMenu.name[0], "Set Samplerate");
	strcpy(audioMenu.name[1], "Toggle Channels...");
	strcpy(audioMenu.name[2], "Mute");
	audioMenu.orientation = 0;
	audioMenu.parent = &mainMenu;
	get_menu_size(&audioMenu, mainMenu.xOffset[3], mainMenu.height + (mainMenu.margin << 1));
	fileList.ioFunction = &file_io;
	getcwd(workDir, sizeof(workDir));
	add_slash(workDir);
	currentMenu = &mainMenu;
}

void add_slash(char *dir){
	if(strcmp(dir + strlen(dir) - 1, "/"))
		sprintf(dir,"%s/",dir);
}

void get_menu_size(menuItem *menu, int xoff, int yoff){
	int width, height, maxWidth = 0, maxHeight = 0;
	for(int i = 0; i < menu->length; i++){
		menu->xOffset[i] = xoff + menu->margin;
		menu->yOffset[i] = yoff + menu->margin;
		TTF_SizeText(Sans, menu->name[i], &width, &height);
		if(menu->orientation)
			xoff += (width + (menu->margin << 1));
		else
			yoff += (height + (menu->margin << 1));
		if(width > maxWidth)
			maxWidth = width;
		if(height > maxHeight)
			maxHeight= height;
	}
	menu->height = maxHeight;
	menu->width = maxWidth;
}

void set_menu(){
	switch(currentMenuColumn){
	case 0:
		currentMenu = &fileMenu;
		break;
	case 1:
		currentMenu = &machineMenu;
		break;
	case 2:
		currentMenu = &graphicsMenu;
		break;
	case 3:
		currentMenu = &audioMenu;
		break;
	}
	if(currentMenuRow > currentMenu->length)
		currentMenuRow = currentMenu->length;
}

void toggle_menu(){
	showMenu ^= 1;
	if(showMenu){
		isPaused = 1;
		io_func = mainMenu.ioFunction;
	}
	else if(!showMenu){
		isPaused = 0;
		io_func = game_io;
	}
	currentMenu = &mainMenu;
	currentMenuColumn = currentMenuRow = fileListOffset = 0;
}

void draw_menu(menuItem *menu){
	SDL_Rect menuRect, srcRect, destRect;
	SDL_Texture *text;
	SDL_Surface *menuText;
	for(int i = 0; i < menu->length; i++){
	    menuText = TTF_RenderText_Blended(Sans, menu->name[i], menuTextColor);
	    text = SDL_CreateTextureFromSurface(currentSettings->window.rend, menuText);
	    SDL_FreeSurface(menuText);
	    SDL_QueryTexture(text, NULL, NULL, &menuRect.w, &menuRect.h);
	    if(menu->width < menuRect.w)
	    	srcRect.w = menu->width;
	    else
	    	srcRect.w = menuRect.w;
	    srcRect.h = menuRect.h;
		srcRect.x = 0;
		srcRect.y = 0;

		destRect = srcRect;
	    destRect.x = menu->xOffset[i];
	    destRect.y = menu->yOffset[i];

	    if(!menu->orientation)
			menuRect.w = menu->width;
	    menuRect.w += (menu->margin << 1);
	    menuRect.h += (menu->margin << 1);
	    menuRect.x = (menu->xOffset[i] - menu->margin);
		menuRect.y = (menu->yOffset[i] - menu->margin);

		/* Highlight active menu item */
	    if(menu->orientation && currentMenuColumn == i && !currentMenuRow)
	        SDL_SetRenderDrawColor(currentSettings->window.rend, menuActiveColor[0], menuActiveColor[1], menuActiveColor[2], menuActiveColor[3]);
	    else if(!menu->orientation && currentMenuRow == i + 1)
	        SDL_SetRenderDrawColor(currentSettings->window.rend, menuActiveColor[0], menuActiveColor[1], menuActiveColor[2], menuActiveColor[3]);
	    else
	    	SDL_SetRenderDrawColor(currentSettings->window.rend, menuBgColor[0], menuBgColor[1], menuBgColor[2], menuBgColor[3]);
	    SDL_RenderFillRect(currentSettings->window.rend, &menuRect);
	    SDL_RenderCopy(currentSettings->window.rend, text, &srcRect, &destRect);
	    SDL_DestroyTexture(text);
	}
}

void call_menu_option(){
	switch((currentMenuColumn << 4) | currentMenuRow){
	case 0x01:
		io_func = &file_io;
		option_open_file();
		break;
	case 0x04:
		option_quit();
		break;
	case 0x21:
		option_fullscreen();
		break;
	}
}

void output_sound(){
	if (!throttle || (SDL_GetQueuedAudioSize(1) > (audioSettings.size * audioSettings.channels)))
		SDL_ClearQueuedAudio(1);
	if (SDL_QueueAudio(1, sampleBuffer, sampleCounter * sizeof(*sampleBuffer)))
		printf("SDL_QueueAudio failed: %s\n", SDL_GetError());
	sampleCounter = 0;
}

void option_open_file(){
	create_file_list();
	currentMenu = &fileList;
}

void option_fullscreen(){
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
}

void option_quit(){
	printf("Quitting\n");
	quit = 1;
	isPaused = 0;
}

void menu_io()
{
	while (SDL_PollEvent(&event)) {
		if (event.window.windowID == currentSettings->window.windowID)
		{
		if(event.type == SDL_KEYDOWN){
			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_UP:
				if(currentMenuRow){
					currentMenuRow--;
					if(!currentMenuRow)
						currentMenu = &mainMenu;
				}
				break;
			case SDL_SCANCODE_DOWN:
				if(!currentMenuRow)
					set_menu();
				if(currentMenuRow < (currentMenu->length))
					currentMenuRow++;
				break;
			case SDL_SCANCODE_LEFT:
				if(currentMenuColumn){
					currentMenuColumn--;
					set_menu();
				}
				break;
			case SDL_SCANCODE_RIGHT:
				if((currentMenuColumn < (mainMenu.length) - 1)){
					currentMenuColumn++;
					set_menu();
				}
				break;
			case SDL_SCANCODE_RETURN:
				call_menu_option();
				break;
			case SDL_SCANCODE_Q:
				if (!(event.key.repeat))
					toggle_menu();
				break;
			default:
				break;
			}
		}
		}
	}
}

void file_io()
{
	while (SDL_PollEvent(&event)) {
		if (event.window.windowID == currentSettings->window.windowID)
		{
		if(event.type == SDL_KEYDOWN){
			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_UP:
				currentMenuRow--;
				if(currentMenuRow <= 0){
					if(fileListOffset > 0){
						fileListOffset--;
						create_file_list();
					}
					currentMenuRow = 1;
				}
				break;
			case SDL_SCANCODE_DOWN:
				if(currentMenuRow <= currentMenu->length)
					currentMenuRow++;
				if(currentMenuRow > currentMenu->length){
					currentMenuRow = currentMenu->length;
					if(filesLeft){
						fileListOffset++;
						create_file_list();
					}
				}
				break;
			case SDL_SCANCODE_RETURN:
				append_to_dir(workDir, fileList.name[currentMenuRow - 1]);
				break;
			case SDL_SCANCODE_ESCAPE:
			case SDL_SCANCODE_BACKSPACE:
				get_parent_dir(workDir);
				break;
			case SDL_SCANCODE_Q:
				if (!(event.key.repeat))
					toggle_menu();
				break;
			default:
				break;
			}
		}
		}
	}
}

void game_io()
{
	while (SDL_PollEvent(&event)) {
		if (event.window.windowID == currentSettings->window.windowID)
		{
		switch (event.type) {
		case SDL_KEYDOWN:
			switch (event.key.keysym.scancode) {
			case SDL_SCANCODE_ESCAPE:
				option_quit();
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
				option_fullscreen();
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
				if (!(event.key.repeat))
					toggle_menu();
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
		}
		}
	}
}
