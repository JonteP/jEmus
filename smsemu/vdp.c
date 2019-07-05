#include "vdp.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> /*exit */
#include "z80.h"
#include "my_sdl.h"
#include "smsemu.h"

uint8_t lineCounter;
uint8_t controlFlag = 0, statusFlags = 0, readBuffer = 0, bgColor = 0, bgXScroll, bgYScroll, lineReload, lineInt = 0;
/* REGISTERS */
uint8_t mode1, mode2, codeReg;
struct VideoMode ntsc192={256,342,240,262,192,216,219,222,235,262}, *currentMode;
struct VideoMode ntsc224={256,342,240,262,224,232,235,238,251,262};
struct VideoMode pal192={256,342,288,313,192,240,243,246,259,313};
struct VideoMode pal224={256,342,288,313,224,256,259,262,275,313};
uint16_t controlWord, vdpdot = 0, vCounter = 0, hCounter = 0, addReg, nameAdd, spritePattern, spriteAttribute;
uint32_t vdp_wait = 0, vdpcc = 0, frame = 0;
/* Mapped memory */					/* TODO: dynamically allocate screenBuffer */
uint8_t vRam[0x4000], cRam[0x20], *screenBuffer;
static inline void render_scanline(void);

void init_vdp(){
	screenBuffer = (uint8_t*)calloc(currentMode->height * currentMode->width, sizeof(uint8_t));
}

void close_vdp(){
	free (screenBuffer);
}

void write_vdp_control(uint8_t value){
controlWord = controlFlag ? ((controlWord & 0x00ff) | (value << 8)) : ((controlWord & 0xff00) | value);
addReg = (controlWord & 0x3fff);
codeReg = (controlWord >> 14);
if (controlFlag){
	switch (codeReg){
	case 0:
		read_vdp_data(); /* dummy read? */
		break;
	case 1:
		break;
	case 2: /* VDP register write */
		switch (controlWord & 0x0f00){
		case 0x0000: /* Mode Control No. 1 */
			mode1 = (controlWord & 0xff);
			if(!(mode1 & 0x04))
				printf("Uses legacy mode\n");
			if((mode1 & 0x01))
				printf("Is monochrome\n");
			break;
		case 0x0100: /* Mode Control No. 2 */
			mode2 = (controlWord & 0xff);
			if(mode2 & 0x01)
				printf("Uses zoomed sprites\n");
			if(mode2 & 0x10){
				if(currentMachine->videoMode == NTSC)
					currentMode = &ntsc224;
				else if(currentMachine->videoMode == PAL)
					currentMode = &pal224;
			}
			else if(!(mode2 & 0x10)){
				if(currentMachine->videoMode == NTSC)
					currentMode = &ntsc192;
				else if(currentMachine->videoMode == PAL)
					currentMode = &pal192;
			}
			break;
		case 0x0200: /* Name Table Base Address */
			nameAdd = ((controlWord & 0x0f) << 10);
			break;
		case 0x0300: /* Color Table Base Address */
			break;
		case 0x0400: /* Background Pattern Generator Base Address */
			break;
		case 0x0500: /* Sprite Attribute Table Base Address */
			spriteAttribute = ((controlWord & 0x7e) << 7);
			break;
		case 0x0600: /* Sprite Pattern Generator Base Address */
			spritePattern = ((controlWord & 0x04) << 11);
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
			lineReload = (controlWord & 0xff);
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
	if(codeReg < 3){
		vRam[addReg++ & 0x3fff] = value;
	}
	else if(codeReg == 3){
		cRam[addReg++ & 0x1f] = value;
	}
	readBuffer = value;
	controlFlag = 0;
}

uint8_t read_vdp_data(){
	uint8_t value = readBuffer;
	readBuffer = vRam[addReg++ & 0x3fff];
	controlFlag = 0;
	return value;
}

void run_vdp(){
while (vdp_wait) {
	if ((((statusFlags & 0x80) && (mode2 & 0x20)) || ((lineInt) && (mode1 & 0x10))) && !irqPulled)
	{
		irqPulled = 1;
	}
	else if ((!(mode2 & 0x20)) && irqPulled)/* disabling line interrupt should deassert the IRQ line */
	{
			irqPulled = 0;
	}
	vdpcc++;
	vdpdot++;
	hCounter = (vdpdot & 0xfe);

	if(vdpdot == 684){
		vdpdot = 0;
		vCounter++;
	}
	if(vCounter == currentMode->fullheight){
		vCounter = 0;
		statusFlags &= ~0x80;
		irqPulled = 0;
		frame++;
		render_frame();
	}
	else if ((vCounter == currentMode->vactive) && !vdpdot){
		statusFlags |= 0x80;
	}
	if (vCounter < currentMode->height && !vdpdot){
		render_scanline();
	}
	if ((vCounter <= currentMode->vactive) && (vdpdot == 100)){
		lineCounter--;
		if (lineCounter == 0xff){
			lineCounter = lineReload;
			lineInt = 1;
		}
	}
	else if ((vCounter > currentMode->vactive) && !vdpdot)
		lineCounter = lineReload;
	vdp_wait--;
}
}
uint8_t blank=0x15, black=0x00;
void render_scanline(){
	uint8_t pixel, tileRow, ntColumn, tileColumn, ntRow, spriteY, spriteX, spriteI, spriteBuffer = 0, pixelOffset, spriteMask[currentMode->width], priorityMask[currentMode->width], transMask[currentMode->width];
	uint16_t nameWord, pidx, sidx, yOffset;
	memset(spriteMask, 0, currentMode->width*sizeof(uint8_t));
	memset(priorityMask, 0, currentMode->width*sizeof(uint8_t));
	memset(transMask, 0, currentMode->width*sizeof(uint8_t));
	yOffset = vCounter + (currentMode->tborder - currentMode->tblank);
	if ((vCounter < currentMode->vactive) && (mode2 & 0x40)){
		uint16_t scroll = ((mode1&0x40) && vCounter < 16) ? 0 : bgXScroll;
	for (uint8_t j = 0; j < 32; j++){
		ntRow = ((vCounter + (((mode1&0x80) && j >= 24) ? 0 : bgYScroll)) % 224);
		ntColumn = 32 - ((scroll & 0xf8) >> 3) + j;
		nameWord  = (vRam[(nameAdd & 0x3800) + ((ntRow & ((nameAdd & 0x400) ? 0xf8 : 0x78)) << 3) + ((ntColumn & 0x1f) << 1)    ]     );
		nameWord |= (vRam[(nameAdd & 0x3800) + ((ntRow & ((nameAdd & 0x400) ? 0xf8 : 0x78)) << 3) + ((ntColumn & 0x1f) << 1) + 1] << 8);
		pidx = ((nameWord & 0x1ff) << 5);
		tileRow = (nameWord & 0x400) ? 7-(ntRow & 7) : (ntRow & 7);
		for (uint8_t c = 0; c < 8; c++){
			tileColumn = (nameWord & 0x200) ? c : 7-c;
			pixel  = (vRam[pidx + (tileRow << 2)    ] & (1 << tileColumn)) ? 1:0;
			pixel |= (vRam[pidx + (tileRow << 2) + 1] & (1 << tileColumn)) ? 2:0;
			pixel |= (vRam[pidx + (tileRow << 2) + 2] & (1 << tileColumn)) ? 4:0;
			pixel |= (vRam[pidx + (tileRow << 2) + 3] & (1 << tileColumn)) ? 8:0;
			screenBuffer[(yOffset*currentMode->width) + ((c+(scroll&7)+(j)*8) & 0xff)]
						 = cRam[pixel+((nameWord & 0x800)?0x10:0)];
			priorityMask[(c+(scroll&7)+(j)*8) & 0xff]=((nameWord & 0x1000) >> 8);
			transMask[(c+(scroll&7)+(j)*8) & 0xff] = pixel ? 1 : 0;
			if((mode1 & 0x20))
				screenBuffer[(yOffset*currentMode->width) + ((c+scroll) & 7)]
							 = cRam[0x10];
		}
	}

	for(uint8_t s = 0; s < 64; s++){
		spriteY = (vRam[spriteAttribute + s] + 1);
		if(spriteY == 0xd1)
			break;
		else if ((vCounter >= spriteY) && (vCounter < (spriteY + ((mode2 & 0x02) ? 16 : 8)))){
			spriteBuffer++;
			if (spriteBuffer > 8)
				statusFlags |= 0x40;
			else{
			spriteX = vRam[spriteAttribute + (s << 1) + 128];
			spriteI = vRam[spriteAttribute + (s << 1) + 129];
			sidx = spritePattern + (((mode2 & 0x02) ? (spriteI & 0xfe) : spriteI) << 5);
			for (uint8_t c = 0; c < 8; c++){
				pixel  = (vRam[sidx + ((vCounter - spriteY) << 2)    ] & (1 << (7-c))) ? 1:0;
				pixel |= (vRam[sidx + ((vCounter - spriteY) << 2) + 1] & (1 << (7-c))) ? 2:0;
				pixel |= (vRam[sidx + ((vCounter - spriteY) << 2) + 2] & (1 << (7-c))) ? 4:0;
				pixel |= (vRam[sidx + ((vCounter - spriteY) << 2) + 3] & (1 << (7-c))) ? 8:0;
				pixelOffset = (c + spriteX - ((mode1 & 0x08) ? 8 : 0));
				if(pixel && pixelOffset < currentMode->width && pixelOffset > 7){
					if (spriteMask[pixelOffset])
						statusFlags |= 0x20; /* set sprite collision flag */
					else{
						if((!priorityMask[pixelOffset]) || (!transMask[pixelOffset]))
							screenBuffer[(yOffset*currentMode->width) + pixelOffset]
										 = cRam[pixel+0x10];
						spriteMask[pixelOffset]= pixel ? 1 : 0;
					}
				}
			}
		}
		}
}


	}
	else{
		uint8_t fillValue;
		if(vCounter < (currentMode->bborder))
			fillValue = cRam[bgColor + 0x10];
		/*else if(vCounter < (currentMode->bblank))
			fillValue = blank;
		else if(vCounter < (currentMode->vblank))
			fillValue = black;
		else if(vCounter < (currentMode->tblank))
			fillValue = blank;*/
		else if(vCounter < (currentMode->tborder))
			fillValue = cRam[bgColor + 0x10];
		for (uint16_t p = 0; p<256; p++){
			screenBuffer[(((yOffset) % currentMode->height)*currentMode->width) + p]
						 = fillValue;
		}
	}
}
