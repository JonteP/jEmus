/*
 * vdp.h
 *
 *  Created on: Jun 3, 2019
 *      Author: jonas
 */

#ifndef VDP_H_
#define VDP_H_
#include <stdio.h>
#include <stdint.h>

typedef enum video {
	NTSC,
	PAL
} Video;

struct VideoMode {
	uint16_t width;
	uint16_t fullwidth;
	uint16_t height;
	uint16_t fullheight;
	uint16_t vactive;
	uint16_t bborder;
	uint16_t bblank;
	uint16_t vblank;
	uint16_t tblank;
	uint16_t tborder;
};

/* CONSTANTS */
#define WWIDTH 320 /*1280 */
#define WHEIGHT 262 /*1024 */
#define SWIDTH 256
#define H_DOTS 342
#define WPOSX 100
#define WPOSY 100

extern uint8_t controlFlag, statusFlags, *screenBuffer, lineInt;
extern uint16_t vCounter, hCounter;
extern uint32_t vdp_wait;
extern struct VideoMode *currentMode, ntsc192, pal192;

void write_vdp_control(uint8_t), run_vdp(), write_vdp_data(uint8_t), init_vdp(), close_vdp();
uint8_t read_vdp_data(void);

#endif /* VDP_H_ */
