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

#define N_FLAG 0x02

void opdecode(void), power_reset(void);

extern FILE *logfile;
uint_fast8_t irqPulled, nmiPulled, reset;

#endif /* Z80_H_ */
