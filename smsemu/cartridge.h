/*
 * cartridge.h
 *
 *  Created on: May 25, 2019
 *      Author: jonas
 */

#ifndef CARTRIDGE_H_
#define CARTRIDGE_H_
#include <stdint.h>

extern uint8_t *prg, fcr[3], *bank[3];

void load_rom();

#endif /* CARTRIDGE_H_ */
