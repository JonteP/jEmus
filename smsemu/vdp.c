#include "vdp.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> /*exit */
#include "z80.h"
#include "my_sdl.h"
#include "smsemu.h"

uint8_t lineCounter, lineReload;
uint8_t controlFlag = 0, statusFlags = 0, readBuffer = 0, bgColor = 0, bgXScroll, bgYScroll, lineInt = 0;
/* REGISTERS */
uint8_t mode1, mode2, codeReg;
struct VideoMode ntsc192={256,342,240,262,192,216,219,222,235,262,224}, *currentMode;
struct VideoMode ntsc224={256,342,240,262,224,232,235,238,251,262,256};
struct VideoMode pal192={256,342,288,313,192,240,243,246,259,313,224};
struct VideoMode pal224={256,342,288,313,224,256,259,262,275,313,256};
uint16_t controlWord, vdpdot = 0, vCounter = 0, hCounter = 0, addReg, ntAddress, spritePattern, spriteAttribute;
uint32_t vdp_wait = 0, vdpcc = 0, frame = 0;
/* Mapped memory */					/* TODO: dynamically allocate screenBuffer */
uint8_t vRam[0x4000], cRam[0x20], *screenBuffer;
static inline void render_scanline(void);

void init_vdp(){
	screenBuffer = (uint8_t*)calloc(currentMode->height * currentMode->width, sizeof(uint8_t));
	ntsc192.vcount = (uint8_t*) malloc(ntsc192.fullheight * sizeof(uint8_t));
	for(int i=0;i<0xdb;i++){
		ntsc192.vcount[i] = i;
	}
	for(int i=0xd5;i<0x100;i++){
		ntsc192.vcount[i+6] = i;
	}
	ntsc224.vcount = (uint8_t*) malloc(ntsc224.fullheight * sizeof(uint8_t));
	for(int i=0;i<0xeb;i++){
		ntsc224.vcount[i] = i;
	}
	for(int i=0xe5;i<0x100;i++){
		ntsc224.vcount[i+6] = i;
	}
	pal192.vcount = (uint8_t*) malloc(pal192.fullheight * sizeof(uint8_t));
	for(int i=0;i<0xf3;i++){
		pal192.vcount[i] = i;
	}
	for(int i=0xba;i<0x100;i++){
		pal192.vcount[i+39] = i;
	}
	pal224.vcount = (uint8_t*) malloc(pal224.fullheight * sizeof(uint8_t));
	for(int i=0;i<0x103;i++){
		pal224.vcount[i] = i;
	}
	for(int i=0xca;i<0x100;i++){
		pal224.vcount[i+39] = i;
	}
}

void close_vdp(){
	free (screenBuffer);
	free (ntsc192.vcount);
	free (ntsc224.vcount);
	free (pal192.vcount);
	free (pal224.vcount);
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
			ntAddress = ((controlWord & 0x0f) << 10);
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
	else if ((!(mode2 & 0x20)) && irqPulled)/* TODO: disabling line interrupt should deassert the IRQ line */
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
	uint16_t ntData, pidx, sidx, yOffset, ntOffset;
	memset(spriteMask, 0, currentMode->width*sizeof(uint8_t));
	memset(priorityMask, 0, currentMode->width*sizeof(uint8_t));
	memset(transMask, 0, currentMode->width*sizeof(uint8_t));
	yOffset = vCounter + (currentMode->tborder - currentMode->tblank);
	if ((vCounter < currentMode->vactive) && (mode2 & 0x40)){
		uint16_t scroll = ((mode1&0x40) && vCounter < 16) ? 0 : bgXScroll;
	for (uint8_t cc = 0; cc < 32; cc++){
		ntRow = ((vCounter + (((mode1&0x80) && cc >= 24) ? 0 : bgYScroll)) % currentMode->vwrap);
		ntColumn = 32 - ((scroll & 0xf8) >> 3) + cc;
		if(currentMode->vactive == 192)
				/*   Name table base address 		 + Name table row (handle masking bug in vdp1)          + Name table column	*/ /* TODO: make vdp1/2 independent of video mode*/
			ntOffset = ((ntAddress & 0x3800) 		 + ((ntRow & ((ntAddress & 0x400) ? 0xf8 : 0x78)) << 3) + ((ntColumn & 0x1f) << 1));
		else if(currentMode->vactive >= 224)
			ntOffset = ((ntAddress & 0x3000) + 0x700 + ((ntRow & 0xf8) << 3) 							    + ((ntColumn & 0x1f) << 1));
		ntData  = (vRam[ntOffset    ]     );
		ntData |= (vRam[ntOffset + 1] << 8);
		pidx = ((ntData & 0x1ff) << 5);
		tileRow = (ntData & 0x400) ? 7-(ntRow & 7) : (ntRow & 7);
		for (uint8_t c = 0; c < 8; c++){
			tileColumn = (ntData & 0x200) ? c : 7-c;
			pixel  = (vRam[pidx + (tileRow << 2)    ] & (1 << tileColumn)) ? 1:0;
			pixel |= (vRam[pidx + (tileRow << 2) + 1] & (1 << tileColumn)) ? 2:0;
			pixel |= (vRam[pidx + (tileRow << 2) + 2] & (1 << tileColumn)) ? 4:0;
			pixel |= (vRam[pidx + (tileRow << 2) + 3] & (1 << tileColumn)) ? 8:0;
			screenBuffer[(yOffset*currentMode->width) + ((c+(scroll&7)+(cc << 3)) & 0xff)]
						 = cRam[pixel+((ntData & 0x800)?0x10:0)];
			priorityMask[(c+(scroll&7)+(cc << 3)) & 0xff]=((ntData & 0x1000) >> 8);
			transMask[(c+(scroll&7)+(cc << 3)) & 0xff] = pixel ? 1 : 0;
			if((mode1 & 0x20))
				screenBuffer[(yOffset*currentMode->width) + ((c+scroll) & 7)]
							 = cRam[bgColor + 0x10];
		}
	}

	for(uint8_t s = 0; s < 64; s++){
		spriteY = (vRam[spriteAttribute + s] + 1);
		if((spriteY == 0xd1) && (currentMode->vactive == 192))
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
