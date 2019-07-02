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

struct VideoMode {
	uint16_t width;
	uint16_t height;
	uint16_t active;
	uint16_t bborder;
	uint16_t bblank;
	uint16_t vblank;
	uint16_t tblank;
	uint16_t tborder;

};

extern struct VideoMode *currentMode, ntsc192;
#define WWIDTH 320 /*1280 */
#define WHEIGHT 262 /*1024 */
#define SWIDTH 256
#define SHEIGHT 262
#define WPOSX 100
#define WPOSY 100
#define FRAMETIME 16688919 /*exact?*/

extern uint8_t controlFlag, statusFlags, screenBuffer[SHEIGHT][SWIDTH], lineInt;
extern uint16_t vCounter, hCounter;
extern uint32_t vdp_wait;

void write_vdp_control(uint8_t), run_vdp(), write_vdp_data(uint8_t);
uint8_t read_vdp_data(void);

#endif /* VDP_H_ */
