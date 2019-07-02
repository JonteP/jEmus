#include "vdp.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> /*exit */
#include "z80.h"
#include "my_sdl.h"

uint8_t lineCounter;
uint8_t controlFlag = 0, statusFlags = 0, readBuffer = 0, bgColor = 0, bgXScroll, bgYScroll, lineReload, lineInt = 0;
/* REGISTERS */
uint8_t mode1, mode2, codeReg;
struct VideoMode ntsc192={256,262,192,216,219,222,235,262}, *currentMode;
uint16_t controlWord, vdpdot = 0, vCounter = 0, hCounter = 0, addReg, nameAdd, spritePattern, spriteAttribute;
uint32_t vdp_wait = 0, vdpcc = 0, frame = 0;
/* Mapped memory */
uint8_t vRam[0x4000], cRam[0x20], screenBuffer[SHEIGHT][SWIDTH];
static inline void render_scanline(void);

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
			if(mode2 & 0x10)
				printf("224 line mode\n");
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
	else if ((!(mode2 & 0x20)) && irqPulled)
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
	if(vCounter == currentMode->height){
		vCounter = 0;
		statusFlags &= ~0x80;
		irqPulled = 0;
		frame++;
		render_frame();
	}
	else if ((vCounter == currentMode->active) && !vdpdot){
		statusFlags |= 0x80;
	}
	if (!vdpdot){
		render_scanline();
	}
	if ((vCounter <= currentMode->active) && (vdpdot == 100)){
		lineCounter--;
		if (lineCounter == 0xff){
			lineCounter = lineReload;
			lineInt = 1;
		}
	}
	else if ((vCounter > currentMode->active) && !vdpdot)
		lineCounter = lineReload;
	vdp_wait--;
}
}
uint8_t blank=0x15, black=0x00;
void render_scanline(){
	uint8_t col = 64, pix, rr, cl, cc, row, spriteY, spriteX, spriteI, sCount = 0, offset, spriteMask[currentMode->width], priorityMask[currentMode->width], transMask[currentMode->width];
	uint16_t nameWord, pidx, sidx, yOffset;
	memset(spriteMask, 0, currentMode->width*sizeof(uint8_t));
	memset(priorityMask, 0, currentMode->width*sizeof(uint8_t));
	memset(transMask, 0, currentMode->width*sizeof(uint8_t));
	yOffset = vCounter + (currentMode->tborder - currentMode->vblank);
	if ((vCounter < currentMode->active) && (mode2 & 0x40)){
		uint16_t scroll = ((mode1&0x40) && vCounter < 16) ? 0 : bgXScroll;
	for (uint8_t j = 0; j < col; j=j+2){
		row = ((vCounter + (((mode1&0x80) && j >= 48) ? 0 : bgYScroll)) % 224);
		cl = 64 - ((scroll & 0xf8) >> 2) + j;
		nameWord = (vRam[(nameAdd & 0x3800) + ((row & ((nameAdd&0x400) ? 0xf8 : 0x78)) << 3) + (cl&0x3f)]);
		nameWord |= (vRam[(nameAdd & 0x3800) + ((row & ((nameAdd&0x400) ? 0xf8 : 0x78)) << 3) + (cl&0x3f)+1] << 8);
		pidx = ((nameWord & 0x1ff) << 5);
		rr = (nameWord & 0x400) ? 7-(row & 7) : (row & 7);
		for (uint8_t c = 0; c < 8; c++){
			cc = (nameWord & 0x200) ? c : 7-c;
			pix  = (vRam[pidx + (rr << 2)] & (1 << cc)) ? 1:0;
			pix |= (vRam[pidx + (rr << 2) + 1] & (1 << cc)) ? 2:0;
			pix |= (vRam[pidx + (rr << 2) + 2] & (1 << cc)) ? 4:0;
			pix |= (vRam[pidx + (rr << 2) + 3] & (1 << cc)) ? 8:0;
			screenBuffer[yOffset][(c+(scroll&7)+(j>>1)*8) & 0xff]=cRam[pix+((nameWord & 0x800)?0x10:0)];
			priorityMask[(c+(scroll&7)+(j>>1)*8) & 0xff]=((nameWord & 0x1000) >> 8);
			transMask[(c+(scroll&7)+(j>>1)*8) & 0xff] = pix ? 1 : 0;
			screenBuffer[yOffset][(c+scroll) & 7]= cRam[bgColor + 0x10];
		}
	}

	for(uint8_t s = 0; s < 64; s++){
		spriteY = (vRam[spriteAttribute + s] + 1);
		if(spriteY == 0xd1)
			break;
		else if ((vCounter >= spriteY) && (vCounter < (spriteY + ((mode2 & 0x02) ? 16 : 8)))){
			sCount++;
			if (sCount > 8)
				statusFlags |= 0x40;
			else{
			spriteX = vRam[spriteAttribute + (s << 1) + 128];
			spriteI = vRam[spriteAttribute + (s << 1) + 129];
			sidx = spritePattern + (((mode2 & 0x02) ? (spriteI & 0xfe) : spriteI) << 5);
			for (uint8_t c = 0; c < 8; c++){
				pix  = (vRam[sidx + ((vCounter - spriteY) << 2)] & (1 << (7-c))) ? 1:0;
				pix |= (vRam[sidx + ((vCounter - spriteY) << 2) + 1] & (1 << (7-c))) ? 2:0;
				pix |= (vRam[sidx + ((vCounter - spriteY) << 2) + 2] & (1 << (7-c))) ? 4:0;
				pix |= (vRam[sidx + ((vCounter - spriteY) << 2) + 3] & (1 << (7-c))) ? 8:0;
				offset = (c + spriteX - ((mode1 & 0x08) ? 8 : 0));
				if(pix && offset < currentMode->width && offset > 7){
					if (spriteMask[offset])
						statusFlags |= 0x20; /* set sprite collision flag */
					else{/* TODO: proper check for transparent BG */
						if((!priorityMask[offset]) || (!transMask[offset]))
							screenBuffer[yOffset][offset]=cRam[pix+0x10];
						spriteMask[offset]= pix ? 1 : 0;
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
		else if(vCounter < (currentMode->bblank))
			fillValue = blank;
		else if(vCounter < (currentMode->vblank))
			fillValue = black;
		else if(vCounter < (currentMode->tblank))
			fillValue = blank;
		else if(vCounter < (currentMode->tborder))
			fillValue = cRam[bgColor + 0x10];
		for (uint16_t p = 0; p<256; p++){
			screenBuffer[(yOffset) % currentMode->height][p] = fillValue;
		}
	}
}
