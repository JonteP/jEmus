#include "vdp.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> /*exit */
#include "z80.h"
#include "my_sdl.h"

uint8_t controlFlag = 0, statusFlags = 0, readBuffer = 0, bgColor = 0, bgXScroll, bgYScroll, lineCounter;
/* REGISTERS */
uint8_t mode1, mode2, screenHeight = 192, codeReg;
uint16_t controlWord, vdpdot = 0, vCounter = 0, hCounter = 0, addReg, nameAdd;
uint32_t vdp_wait = 0, vdpcc = 0, frame = 0;
/* Mapped memory */
uint8_t vRam[0x4000], cRam[0x1f], screenBuffer[SHEIGHT][SWIDTH];
static inline void render_scanline(void);

void write_vdp_control(uint8_t value){
controlWord = controlFlag ? ((controlWord & 0x00ff) | (value << 8)) : ((controlWord & 0xff00) | value);
if (controlFlag){
	codeReg = (controlWord >> 14);
	addReg = (controlWord & 0x3fff);
	switch (codeReg){
	case 0:
		read_vdp_data(); /* dummy read? */
		addReg++;
		break;
	case 1:
		break;
	case 2: /* VDP register write */
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
			bgColor = (controlWord & 0x0f);
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
	case 3: /* Writes go to CRAM */
		break;
	}
}
controlFlag ^= 1;
}

void write_vdp_data(uint8_t value){
	if(codeReg <= 2){
		vRam[addReg & 0x3fff] = value;
	}
	else if(codeReg == 3){
		cRam[addReg & 0x1f] = value;
	}
	readBuffer = value;
	addReg++;
}

uint8_t read_vdp_data(){
	uint8_t value = readBuffer;
	readBuffer = vRam[addReg & 0x3fff];
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
		render_frame();
	}
	else if (vCounter == screenHeight && !vdpdot){
		statusFlags |= 0x80;
	}
	if (vCounter < 240 && !vdpdot)
		render_scanline();
	cycles--;
	vdp_wait--;
}
}

void render_scanline(){
	uint8_t col = 64, pix, rr, cl, cc, topBorder = 24, bottomBorder = 216;
	uint16_t nameWord, pidx;
	if (vCounter >= topBorder && vCounter < bottomBorder){
		uint8_t line = vCounter - topBorder;
		uint16_t scroll = ((mode2&0x40) && line < 16) ? 0 : bgXScroll;
	for (uint8_t j = 0; j < col; j=j+2){
		cl = 64 - ((scroll & 0xf8) >> 2) + j;
		nameWord = (vRam[nameAdd + ((line & 0xf8) << 3) + (cl&0x3f)]);
		nameWord |= (vRam[nameAdd + ((line & 0xf8) << 3) + (cl&0x3f)+1] << 8);
		pidx = ((nameWord & 0x1ff) << 5);
		rr = (nameWord & 0x400) ? 7-(line % 8) : (line % 8);
		for (uint8_t c = 0; c < 8; c++){
			cc = (nameWord & 0x200) ? c : 7-c;
			pix  = (vRam[pidx + 4*rr] & (1 << cc)) ? 1:0;
			pix |= (vRam[pidx + 4*rr + 1] & (1 << cc)) ? 2:0;
			pix |= (vRam[pidx + 4*rr + 2] & (1 << cc)) ? 4:0;
			pix |= (vRam[pidx + 4*rr + 3] & (1 << cc)) ? 8:0;
			screenBuffer[vCounter][(c+(scroll&7)+(j>>1)*8) & 0xff]=cRam[pix+((nameWord & 0x800)?0x10:0)];
			screenBuffer[vCounter][(c+scroll) & 7]= cRam[bgColor + 0x10];
		}
	}
	}
	else{
		for (uint16_t p = 0; p<256; p++){
			screenBuffer[vCounter][p] = cRam[bgColor + 0x10];
		}
	}
}
