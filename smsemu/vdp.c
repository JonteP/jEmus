#include "vdp.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> /*exit */
#include "z80.h"

uint8_t controlFlag = 0, statusFlags = 0, readBuffer = 0;
/* REGISTERS */
uint8_t mode1, mode2, screenHeight = 192, codeReg;
uint16_t controlWord, vdpdot = 0, vCounter = 0, hCounter = 0, addReg;
uint32_t vdp_wait = 0, vdpcc = 0, frame = 0;
/* Mapped memory */
uint8_t vRam[0x3fff], cRam[0x3f];

void write_vdp_control(uint8_t value){
controlWord = controlFlag ? ((controlWord & 0x00ff) | (value << 8)) : ((controlWord & 0xff00) | value);
if (controlFlag){
	/*printf("VDP control is: %04x\n",controlWord); */
	switch (controlWord & 0xc000){
	case 0x0000:
		break;
	case 0x4000:
		break;
	case 0x8000: /* VDP register write */
		switch (controlWord & 0x0f00){
		case 0x0000: /* Mode Control No. 1 */
			mode1 = (controlWord & 0xff);
			break;
		case 0x0100: /* Mode Control No. 2 */
			mode2 = (controlWord & 0xff);
			break;
		case 0x0200: /* Name Table Base Address */
			break;
		case 0x0300: /* Color Table Base Address */
			break;
		case 0x0400: /* Background Pattern Generator Base Address */
			break;
		case 0x0500: /* Sprite Attribute Table Base Address */
			break;
		case 0x0600: /* Sprite Pattern Generator Base Address */
			break;
		case 0x0700: /* Overscan/Backdrop Color */
			break;
		case 0x0800: /* Background X Scroll */
			break;
		case 0x0900: /* Background Y Scroll */
			break;
		case 0x0a00: /* Line counter */
			break;
		default:
			printf("Write to undefined VDP register\n");
			break;
		}
		break;
	case 0xc000:
		break;
	}
	codeReg = (controlWord >> 14);
	addReg = (controlWord & 0x3fff); /* 0x3f for CRAM */
	if(!codeReg)
		printf("Code 0 not implemented\n");
}
controlFlag ^= 1;
}

void write_vdp_data(uint8_t value){
	if(codeReg == 1){
		vRam[addReg] = value;
	}
	else if(codeReg == 3){
		cRam[addReg & 0x3f] = value;
	}
	else{
		printf("Unknown write register: %02x\n",codeReg);
		exit(1);
	}
	readBuffer = value;
}

uint8_t read_vdp_data(){
	uint8_t value = readBuffer;
	readBuffer = vRam[addReg];
	return value;
}

void run_vdp(uint32_t cycles){
while (cycles) {
	if ((statusFlags & 0x80) && (mode2 & 0x20) && !irqPulled)
	{
		irqPulled = 1;
	}
	else if (!(mode2 & 0x20) && irqPulled)
	{
			irqPulled = 0;
			printf("IRQ untriggered\n");
	}
	vdpcc++;
	vdpdot++;

	if(vdpdot == 684){
		vdpdot = 0;
		vCounter++;
	}
	if(vCounter == 262){
		vCounter = 0;
		statusFlags &= ~0x80;
		irqPulled = 0;
		frame++;
		printf("Frame: %i\n",frame);
	}
	else if (vCounter == screenHeight && !vdpdot){
		statusFlags |= 0x80;
	}
	cycles--;
	vdp_wait--;
}
}
