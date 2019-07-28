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

#define NT_MASK 0x3c00
#define PG_MASK 0x3800

typedef enum video {
	NTSC,
	PAL
} Video;

typedef enum statusFlags {
	INT = 0x80,
	OVR = 0x40,
	COL = 0x20
} StatusFlags;

struct DisplayMode {
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
	uint16_t vwrap;
	uint8_t *vcount;
};

extern uint8_t controlFlag, statusFlags, lineInt, smsColor[0xc0];
extern uint32_t *screenBuffer;
extern int16_t vdpdot;
extern uint16_t vCounter, hCounter;
extern int vdpCyclesToRun;
extern struct DisplayMode *currentMode, ntsc192, pal192;

void write_vdp_control(uint8_t), run_vdp(), write_vdp_data(uint8_t), init_vdp(), close_vdp(), latch_hcounter(uint8_t);
uint8_t read_vdp_data(void);

#endif /* VDP_H_ */
