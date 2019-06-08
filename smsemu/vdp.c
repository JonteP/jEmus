#include "vdp.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> /*exit */
#include "z80.h"

uint8_t controlFlag = 0, statusFlags = 0, readBuffer = 0, bgColor, bgXScroll, bgYScroll, lineCounter;
/* REGISTERS */
uint8_t mode1, mode2, screenHeight = 192, codeReg;
uint16_t controlWord, vdpdot = 0, vCounter = 0, hCounter = 0, addReg, nameAdd;
uint32_t vdp_wait = 0, vdpcc = 0, frame = 0;
/* Mapped memory */
uint8_t vRam[0x3fff], cRam[0x1f], screenBuffer[224][256];
static inline void render_frame(void);

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
			nameAdd = ((controlWord & 0x0e) << 10);
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
			bgColor = (controlWord & 0xff);
			break;
		case 0x0800: /* Background X Scroll */
			bgXScroll = (controlWord & 0xff);
			break;
		case 0x0900: /* Background Y Scroll */
			bgYScroll = (controlWord & 0xff);
			break;
		case 0x0a00: /* Line counter */
			lineCounter = (controlWord & 0xff);
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
	addReg = (controlWord & 0x3fff);
	if(!codeReg)
		printf("Code 0 not implemented\n");
}
controlFlag ^= 1;
}

void write_vdp_data(uint8_t value){
	if(codeReg == 1){
		vRam[addReg] = value;
		printf("Write to VDP address: %04x, value: %02x\n",addReg, value);
	}
	else if(codeReg == 3){
		cRam[addReg & 0x1f] = value;
	}
	else{
		printf("Unknown write register: %02x\n",codeReg);
		exit(1);
	}
	readBuffer = value;
	addReg++;
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
		if(frame==252)
			render_frame();
	}
	else if (vCounter == screenHeight && !vdpdot){
		statusFlags |= 0x80;
	}
	cycles--;
	vdp_wait--;
}
}

void render_frame(){
	uint8_t row = 32;
	uint8_t col = 64;
	for (uint8_t i = 0; i < row; i++) {
		for (uint8_t j = 0; j < col; j=j+2){
			uint16_t nameWord = (vRam[nameAdd + i*64 + j] << 8);
			nameWord |= vRam[nameAdd + i*64 + j+1];
			printf("drawing from: %04x, attribute: %04x\n",nameAdd + i*64 + j,nameWord);
			uint16_t pidx = (nameWord & 0x1ff);
			for (uint8_t r = 0; r < 8; r++){
				for (uint8_t c = 0; c < 8; c++){
					uint8_t pix  = (vRam[pidx + 4*r] & (1 << (7-c))) ? 8:0;
							pix += (vRam[pidx + 4*r + 1] & (1 << (7-c))) ? 4:0;
							pix += (vRam[pidx + 4*r + 2] & (1 << (7-c))) ? 2:0;
							pix += (vRam[pidx + 4*r + 3] & (1 << (7-c))) ? 1:0;
							screenBuffer[r*i][c*(j>>1)]=pix;
				}
			}
		}
	}
}
