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

#define IO1_PORTB_DOWN 	0x80;
#define IO1_PORTB_UP 	0x40;
#define IO1_PORTA_TR	0x20;
#define IO1_PORTA_TL	0x10;
#define IO1_PORTA_RIGHT	0x08;
#define IO1_PORTA_LEFT	0x04;
#define IO1_PORTA_DOWN	0x02;
#define IO1_PORTA_UP	0x01;
#define IO2_PORTB_TH	0x80;
#define IO2_PORTA_TH	0x40;
#define IO2_RESET		0x10;
#define IO2_PORTB_TR	0x08;
#define IO2_PORTB_TL	0x04;
#define IO2_PORTB_RIGHT	0x02;
#define IO2_PORTB_LEFT	0x01;

typedef enum _region {
	JAPAN,
	EXPORT
} Region;

struct machine {
	char *bios;
	int  masterClock;
	Video videoMode;
	Region region;
	/* has card slot
	 * has expansion slot
	 * has bios
	 * has FM
	 * has reset
	 * has pause
	 */
};

extern uint8_t quit, ioPort1, ioPort2, ioControl, region;
extern char *cartFile, *cardFile, *expFile, *biosFile;
extern float frameTime;
extern struct machine *currentMachine;

#endif /* SMSEMU_H_ */
