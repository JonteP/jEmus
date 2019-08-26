/*
 * z80.h
 *
 *  Created on: May 25, 2019
 *      Author: jonas
 */

#ifndef Z80_H_
#define Z80_H_
#include <stdio.h>
#include <stdint.h>

#define Y_FLAG 0x20
#define H_FLAG 0x10
#define X_FLAG 0x08
#define N_FLAG 0x02
#define C_FLAG 0x01
#define XY_FLAG (Y_FLAG | X_FLAG)

void run_z80(void), power_reset(void);

extern uint8_t irqPulled, nmiPulled;

#endif /* Z80_H_ */
