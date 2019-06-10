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

#define WWIDTH 320 /*1280 */
#define WHEIGHT 240 /*1024 */
#define SWIDTH 256
#define SHEIGHT 224
#define WPOSX 100
#define WPOSY 100
#define FRAMETIME 16688919 /*exact?*/

extern uint8_t controlFlag, statusFlags, screenBuffer[SHEIGHT][SWIDTH];
extern uint16_t vCounter, hCounter;
extern uint32_t vdp_wait;

void write_vdp_control(uint8_t), run_vdp(uint32_t), write_vdp_data(uint8_t);
uint8_t read_vdp_data(void);

#endif /* VDP_H_ */
