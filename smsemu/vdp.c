/* TMS9918a: the original used by SG-1000, SC-3000 and MSX
 *
 * 315-5124: TMS9918a with Mode4 added
 * 315-5246: Revision of 315-5124
 * 315-5378: 315-5246 with an added graphics mode (Game Gear)
 * 315-5313: The version used in Mega Drive (reduced functionality)
 *
 *TODO: emulate version differences:
 * -screen height
 * -masking of nametable address
 * -zooming: bug in vdp1, works in vdp2, not at all in md http://benryves.com/journal/2889425
 */

#include "vdp.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> /*exit */
#include "z80.h"
#include "my_sdl.h"
#include "smsemu.h"

uint16_t lineCounter, lineReload;
uint8_t controlFlag = 0, statusFlags = 0, readBuffer = 0, bgColor = 0, bgXScroll, bgYScroll, lineInt = 0;
uint8_t vScrollLock, hScrollLock, columnMask, lineInterrupt, spriteShift, syncEnable, displayEnable, frameInterrupt, spriteSize, spriteZoom, videoMode;

/* REGISTERS */
uint8_t modeControl1, modeControl2, codeReg;
struct DisplayMode ntsc192={256,342,240,262,192,216,219,222,235,262,224}, *currentMode;
struct DisplayMode ntsc224={256,342,240,262,224,232,235,238,251,262,256};
struct DisplayMode pal192={256,342,288,313,192,240,243,246,259,313,224};
struct DisplayMode pal224={256,342,288,313,224,256,259,262,275,313,256};
int16_t vdpdot;
uint16_t controlWord, vCounter = 0, hCounter = 0, addReg, ntAddress, spritePattern, spriteAttribute, ctAddress, ptAddress;
int vdpCyclesToRun = 0, frame = 0;
/* Mapped memory */					/* TODO: dynamically allocate screenBuffer */
uint8_t vram[0x4000], cram[0x20], *screenBuffer;
static inline void render_scanline(void), set_video_mode(void);

void init_vdp(){
	ptAddress = 0;
	set_video_mode();
	screenBuffer = (uint8_t*)calloc(currentMode->height * currentMode->width, sizeof(uint8_t));
	vdpdot = -94;
	/* TODO: can this be simplfied by thinking of v counter as signed 8 bit? */
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

void set_video_mode(){
	/* Mode 5 is probably (mode2 & 0x04) */
	videoMode = (((modeControl1 & 0x04) << 1) | ((modeControl2 & 0x08) >> 1) | (modeControl1 & 0x02) | ((modeControl2 & 0x10) >> 4));
	if(videoMode == 0x9 || videoMode == 0xd) /* text mode TODO: valid for sms2 only */
		videoMode = 1;
	else if(videoMode == 0xb){
		if(currentMachine->videoSystem == NTSC)
			currentMode = &ntsc224;
		else if(currentMachine->videoSystem == PAL)
			currentMode = &pal224;
	}
	else if(videoMode == 0xe){
		printf("Sets 240 line mode\n");
	}
	else{
		if(currentMachine->videoSystem == NTSC)
			currentMode = &ntsc192;
		else if(currentMachine->videoSystem == PAL)
			currentMode = &pal192;
	}
	printf("Video mode: %02x\n",videoMode);
}

void write_vdp_control(uint8_t value){
controlWord = controlFlag ? ((controlWord & 0x00ff) | (value << 8)) : ((controlWord & 0xff00) | value);
controlFlag ^= 1;
addReg = (controlWord & 0x3fff);
codeReg = (controlWord >> 14);
if (!controlFlag){
	switch (codeReg){
	case 0:
		read_vdp_data();
		break;
	case 2: /* VDP register write */
		switch (controlWord & 0x0f00){
		case 0x0000: /* Mode Control No. 1 */
			/* bit 7 sets external vdp */
			modeControl1 = (controlWord & 0xff);
			if((modeControl1 & 0x01))
				printf("Sync disabled\n");
			vScrollLock = (modeControl1 & 0x80);
			hScrollLock = (modeControl1 & 0x40);
			columnMask = ((modeControl1 & 0x20) >> 2);
			lineInterrupt = (modeControl1 & 0x10);
			spriteShift = (modeControl1 & 0x08);
			syncEnable = (modeControl1 & 0x01);
			set_video_mode();
			break;
		case 0x0100: /* Mode Control No. 2 */
			modeControl2 = (controlWord & 0xff);
			displayEnable = (controlWord & 0x40);
			frameInterrupt = (controlWord & 0x20);
			spriteSize = (controlWord & 0x02);
			spriteZoom = (controlWord & 0x01);
			set_video_mode();
			break;
		case 0x0200: /* Name Table Base Address */
			ntAddress = ((controlWord & 0x0f) << 10);
			break;
		case 0x0300: /* Color Table Base Address */
			if(videoMode == 2)
				ctAddress = ((controlWord & 0x80) << 6);
			else
				ctAddress = ((controlWord & 0xff) << 6);
			break;
		case 0x0400: /* Background Pattern Generator Base Address */
			if(videoMode & 0x08)
				ptAddress = 0;
			else if(videoMode & 0x02)
				ptAddress = ((controlWord & 0x04) << 11);
			else
				ptAddress = ((controlWord & 0x07) << 11);
			break;
		case 0x0500: /* Sprite Attribute Table Base Address */
			spriteAttribute = ((controlWord & 0x7e) << 7); /* TODO: mask should depend on mode */
			break;
		case 0x0600: /* Sprite Pattern Generator Base Address */
			spritePattern = ((controlWord & 0x04) << 11); /* TODO: mask should depend on mode */
			break;
		case 0x0700: /* Overscan/Backdrop Color */
			bgColor = (controlWord & 0x0f); /* TODO: upper 4 bits are used in text mode */
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
	}
}
}

void write_vdp_data(uint8_t value){
	if(codeReg < 3){
		vram[addReg++ & 0x3fff] = value;
	}
	else if(codeReg == 3){
		cram[addReg++ & 0x1f] = value;
	}
	readBuffer = value;
	controlFlag = 0;
}

uint8_t read_vdp_data(){
	uint8_t value = readBuffer;
	readBuffer = vram[addReg++ & 0x3fff];
	controlFlag = 0;
	return value;
}

void run_vdp(){
while (vdpCyclesToRun) {

	if(vdpdot == 590)
		vdpdot = -94;
	if(vdpdot == -48)
		vCounter++;
	if(vCounter == currentMode->fullheight){
		vCounter = 0;
		irqPulled = 0;
		frame++;
		render_frame();
	}
	else if ((vCounter == currentMode->vactive) && (vdpdot == -52)){
		statusFlags |= 0x80;
	}
	if (vCounter < currentMode->height && !vdpdot){
		render_scanline();
	}
	if ((vCounter <= currentMode->vactive) && (vdpdot == -50)){
		lineCounter--;
		if ((lineCounter & 0xff) == 0xff){
			lineCounter = lineReload;
			lineInt = 1;
		}
	}
	else if ((vCounter > currentMode->vactive) && !vdpdot)
		lineCounter = lineReload;
	vdpCyclesToRun--;
	if (( ((statusFlags & 0x80) && frameInterrupt) || ((lineInt) && lineInterrupt) ) && !irqPulled)
	{
		irqPulled = 1;
	}
	else if ((!frameInterrupt /*|| (!(mode1 & 0x10))*/) && irqPulled)/* TODO: disabling line interrupt should deassert the IRQ line */
	{
			irqPulled = 0;
	}
	vdpdot++;
}
}
uint8_t blank=0x15, black=0x00;
void render_scanline(){
	uint8_t pixel, tileRow, ntColumn, tileColumn, ntRow, spriteX, spriteI, spriteBuffer = 0, spriteMask[currentMode->width], priorityMask[currentMode->width], transMask[currentMode->width], color;
	int16_t spriteY;
	uint16_t ntData, ptOffset, ctOffset, sidx, ntOffset;
	memset(spriteMask, 0, currentMode->width*sizeof(uint8_t));
	memset(priorityMask, 0, currentMode->width*sizeof(uint8_t));
	memset(transMask, 0, currentMode->width*sizeof(uint8_t));
	uint16_t yOffset = vCounter + (currentMode->tborder - currentMode->tblank);

	if ((vCounter < currentMode->vactive) && displayEnable){ /* during active display */
		uint16_t scroll = (hScrollLock && vCounter < 16) ? 0 : bgXScroll;
		for (uint8_t screenColumn = 0; screenColumn < 32; screenColumn++){
			ntRow = ((vCounter + ((vScrollLock && screenColumn >= 24) ? 0 : bgYScroll)) % currentMode->vwrap);
			ntColumn = 32 - ((scroll & 0xf8) >> 3) + screenColumn;

			/* Find the address of the current tile name */
			if(videoMode & 0x08){
				if(currentMode->vactive == 192) /* TODO: this is currently old behavior only */
						/*   Name table base address 		 + Name table row (handle masking bug in vdp1)          + Name table column	*/ /* TODO: make vdp1/2 independent of video mode*/
					ntOffset = ((ntAddress & 0x3800) 		 + ((ntRow & ((ntAddress & 0x400) ? 0xf8 : 0x78)) << 3) + ((ntColumn & 0x1f) << 1));
				else if(currentMode->vactive >= 224) /* only on later vdp revisions */
					ntOffset = ((ntAddress & 0x3000) + 0x700 + ((ntRow & 0xf8) << 3) 							    + ((ntColumn & 0x1f) << 1));
			}
			else if(videoMode & 0x02) /* Graphics II mode */
				ntOffset = (ntAddress + ((ntRow & 0xf8) << 2) + (ntColumn & 0x1f));

			/* Fetch current tile name */
			ntData = vram[ntOffset];
			if(videoMode & 0x08)
				ntData |= (vram[ntOffset + 1] << 8);

			/* Find the address of the corresponding pattern generator */
			if(videoMode == 2)
				ptOffset = (ptAddress + ((vCounter & 0xc0) << 5) + ((ntData & 0xff) << 3));
			else /* TODO: this is only for mode 4 */
				ptOffset = (ptAddress + ((ntData & 0x1ff) << 5));

			/* Find the address of the corresponding color (not mode 4) */
			if(videoMode == 2)
				ctOffset = (ctAddress + ((vCounter & 0xc0) << 5) + ((ntData & 0xff) << 3));

			/* Generate the current row of the pattern */
			tileRow = ((ntData & 0x400) && (videoMode & 0x08)) ? 7-(ntRow & 7) : (ntRow & 7);
			if(videoMode == 2)
				color = vram[ctOffset + tileRow];
			for (uint8_t pixelIndex = 0; pixelIndex < 8; pixelIndex++){
				tileColumn = ((ntData & 0x200) && (videoMode & 0x08)) ? pixelIndex : 7-pixelIndex;
				pixel  = (vram[ptOffset + (tileRow << ((videoMode & 0x08) ? 2 : 0))] & (1 << tileColumn)) ? 1:0;
				if(videoMode & 0x08){ /* 4bpp */
					pixel |= (vram[ptOffset + (tileRow << 2) + 1] & (1 << tileColumn)) ? 2:0;
					pixel |= (vram[ptOffset + (tileRow << 2) + 2] & (1 << tileColumn)) ? 4:0;
					pixel |= (vram[ptOffset + (tileRow << 2) + 3] & (1 << tileColumn)) ? 8:0;
				}

				/* Output the pattern to the screen buffer */
				int targetPixel = ((pixelIndex+(scroll&7)+(screenColumn << 3)) & 0xff);
				if(videoMode == 2)
					screenBuffer[(yOffset * currentMode->width) + targetPixel] = cram[pixel ? (color >> 4) : (color & 0xf)];
				else
					screenBuffer[(yOffset * currentMode->width) + targetPixel] = cram[pixel + ((ntData & 0x800) ? 0x10 : 0)];
				priorityMask[targetPixel] = ((ntData & 0x1000) >> 8);
				transMask[targetPixel] = pixel ? 1 : 0;
				if(columnMask)
					screenBuffer[(yOffset*currentMode->width) + ((pixelIndex+scroll) & 7)]
							 = cram[bgColor + 0x10];
			}
		}
/* Sprite rendering */
	for(uint8_t s = 0; s < 64; s++){
		uint8_t spriteHeight = ((spriteSize ? 16 : 8) << spriteZoom);
		spriteY = (vram[spriteAttribute + s] + 1);
		if((spriteY == (0xd0 + 1)) && (currentMode->vactive == 192))
			break;
		if(spriteY >= (256 - spriteHeight + 1))
			spriteY = (0 - (256 - spriteY)); /* negative Y offset (sprites go offscreen from top) */
		if ((vCounter >= spriteY) && (vCounter < (spriteY + spriteHeight))){
			spriteBuffer++;
			if (spriteBuffer > 8)
				statusFlags |= 0x40;
			else{
				spriteX = vram[spriteAttribute + (s << 1) + 128];
				spriteI = vram[spriteAttribute + (s << 1) + 129];
				sidx = spritePattern + ((spriteSize ? (spriteI & 0xfe) : spriteI) << 5);
				for (uint8_t c = 0; c < (8 << spriteZoom); c++){
					pixel  = (vram[sidx + (((vCounter - spriteY) >> spriteZoom) << 2)    ] & (1 << (7 - (c >> spriteZoom)))) ? 1:0;
					pixel |= (vram[sidx + (((vCounter - spriteY) >> spriteZoom) << 2) + 1] & (1 << (7 - (c >> spriteZoom)))) ? 2:0;
					pixel |= (vram[sidx + (((vCounter - spriteY) >> spriteZoom) << 2) + 2] & (1 << (7 - (c >> spriteZoom)))) ? 4:0;
					pixel |= (vram[sidx + (((vCounter - spriteY) >> spriteZoom) << 2) + 3] & (1 << (7 - (c >> spriteZoom)))) ? 8:0;
					int pixelOffset = (c + spriteX - spriteShift); /* must be signed int */
					if(pixel && pixelOffset < currentMode->width && pixelOffset >= columnMask){
						if (spriteMask[pixelOffset])
							statusFlags |= 0x20; /* set sprite collision flag */
						else{
							if((!priorityMask[pixelOffset]) || (!transMask[pixelOffset]))
								screenBuffer[(yOffset*currentMode->width) + pixelOffset] = cram[pixel+0x10];
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
			fillValue = cram[bgColor + 0x10];
		/*else if(vCounter < (currentMode->bblank))
			fillValue = blank;
		else if(vCounter < (currentMode->vblank))
			fillValue = black;
		else if(vCounter < (currentMode->tblank))
			fillValue = blank;*/
		else if(vCounter < (currentMode->tborder))
			fillValue = cram[bgColor + 0x10];
		for (uint16_t p = 0; p<256; p++){
			screenBuffer[(((yOffset) % currentMode->height)*currentMode->width) + p]
						 = fillValue;
		}
	}
}

void latch_hcounter(uint8_t value){
	if(value & (IO2_PORTA_TH | IO2_PORTB_TH)){
		hCounter = (vdpdot >> 2);
	}
}
