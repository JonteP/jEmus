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

void run_z80(void), power_reset(void);

// Function pointers to be defined by the emulated machine
uint8_t * (*read_z80_memory)(uint16_t);
void (*write_z80_memory)(uint16_t, uint8_t);
uint8_t (*read_z80_register)(uint8_t);
void (*write_z80_register)(uint8_t, uint8_t);
void (*addcycles)(uint8_t);
void (*synchronize)(void);

extern uint8_t irqPulled, nmiPulled;

#endif /* Z80_H_ */
