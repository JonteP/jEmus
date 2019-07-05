/*
 * smsemu.h
 *
 *  Created on: Jun 25, 2019
 *      Author: jonas
 */

#ifndef SMSEMU_H_
#define SMSEMU_H_
#include <stdint.h>
#include "vdp.h"

typedef enum region {
	JAPAN,
	EXPORT
} Region;

struct machine {
	char *bios;
	int  masterClock;
	Video videoMode;
	uint8_t region;
};

extern uint8_t quit, ioPort1, ioPort2, ioControl, region;
extern char *cartFile, *cardFile, *expFile, *biosFile;
extern float frameTime;
extern struct machine *currentMachine;

#endif /* SMSEMU_H_ */
