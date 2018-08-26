#include <stdint.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>	/* malloc; exit */
#include <string.h>	/* memcpy */
#include <time.h>
#include <unistd.h>
#include "globals.h"
#include "ppu.h"
#include "apu.h"
#include "6502.h"
#include "sha.h"
#include "parser.h"
#include "tree.h"
#include "my_sdl.h"

/* TODO:
 *
 * -battery backed saves
 * -cycle correct cpu emulation (prefetches, read-modify-writes and everything)
 * -dot rendering with correct timing
 * -FIR low pass filtering of sound
 * -fix sound buffering (if broken)
 *
 * Features:
 * -file load routines
 * -oam viewer
 * -save states
 */

static inline void extract_xml_data(xmlNode * s_node), xml_hash_compare(xmlNode * c_node), parse_xml_file(xmlNode * a_node);

/* constants */
const uint8_t id[4] = { 0x4e, 0x45, 0x53, 0x1a };
gameInfos gameInfo;
gameFeatures cart;
int psize, csize;
xmlDoc *nesXml;
xmlNode *root;
xmlChar *sphash, *schash;
unsigned char phash[SHA_DIGEST_LENGTH], chash[SHA_DIGEST_LENGTH];

uint8_t quit = 0, ctrb = 0, ctrb2 = 0, ctr1 = 0, ctr2 = 0;
uint8_t header[0x10], wramEnable = 0, openBus;
uint8_t *prg, *chr, *cram, *bwram, *wram, *wramSource;
uint16_t ppu_wait = 0, apu_wait = 0;
uint8_t mirroring[4][4] = { { 0, 0, 1, 1 },
							{ 0, 1, 0, 1 },
							{ 0, 0, 0, 0 },
							{ 1, 1, 1, 1 } };

FILE *romFile, *logfile, *bwramFile;
char *bwramName, *romName;

int main() {
	romName = "/home/jonas/eclipse-workspace/mmc1/zeldau.nes";
	romFile = fopen(romName, "r");
	if (romFile == NULL) {
		printf("Error: No such file\n");
		exit(EXIT_FAILURE);
	}
	fread(header, sizeof(header), 1, romFile);
	for (int i = 0; i < sizeof(id); i++) {
		if (header[i] != id[i]) {
			printf("Error: Invalid iNES header!\n");
			exit(EXIT_FAILURE);
		}
	}

	/* read data from ROM */
	psize = header[4] * PRG_BANK<<2;
	csize = header[5] * CHR_BANK<<3;

	prg = malloc(psize);
	fread(prg, psize, 1, romFile);
	SHA1(prg,psize,phash);

	sphash = malloc(SHA_DIGEST_LENGTH*2+1);
	for (int i = 0; i<sizeof(phash); i++) {
		sprintf((char *)sphash+i*2, "%02x", phash[i]);
	}
	nesXml = xmlReadFile("/home/jonas/eclipse-workspace/nes.xml", NULL, 0);
	root = xmlDocGetRootElement(nesXml);
	parse_xml_file(root);
	xmlFreeDoc(nesXml);
	xmlCleanupParser();

	if (csize) {
		chr = malloc(csize);
		fread(chr, csize, 1, romFile);
	} else {
		csize = cart.cramSize;
		chr = malloc(csize);
	}
	fclose(romFile);

	if (cart.bwramSize) {
		bwramName = strdup(romName);
		sprintf(bwramName+strlen(bwramName)-3,"sav");
		bwramFile = fopen(bwramName, "r");
		bwram = malloc(cart.bwramSize);
		if (bwramFile) {
			fread(bwram, cart.bwramSize, 1, bwramFile);
			fclose(bwramFile);
		}
		wramSource = bwram;
	} else if (cart.wramSize) {
		wram = malloc(cart.wramSize);
		wramSource = wram;
	}

	printf("PCB: %s\n",cart.pcb);
	printf("PRG size: %li bytes\n",cart.prgSize);
	printf("CHR size: %li bytes\n",cart.chrSize);
	printf("WRAM size: %li bytes\n",cart.wramSize);
	printf("BWRAM size: %li bytes\n",cart.bwramSize);
	printf("CHRRAM size: %li bytes\n",cart.cramSize);
/*	mirrmode = (header[6] & 1); */
	/* 0 = horizontal mirroring
	 * 1 = vertical mirroring
	 * 2 = one screen, low page
	 * 3 = one screen, high page
	 * 4 = 4 screen
	 */

	logfile = fopen("/home/jonas/eclipse-workspace/logfile.txt","w");
	if (logfile==NULL)
		printf("Error: Could not create logfile\n");

	rstFlag = HARD_RESET;
	init_sdl();
	init_time();

	while (quit == 0) {
		if (cpuStall)
			cpuStall = 0;
		else
			opdecode();
		synchronize(0);
	}
	fclose(logfile);
	free(prg);
	free(chr);
	if (cart.bwramSize) {
		bwramFile = fopen(bwramName, "w");
		fwrite(bwram,cart.bwramSize,1,bwramFile);
		free(bwram);
		fclose(bwramFile);
	} else if (cart.wramSize) {
		free(wram);
	}
	close_sdl();
}

void extract_xml_data(xmlNode * s_node) {
	xmlNode *cur_node = s_node->children;
	xmlChar *nam, *val;
	while (cur_node) {
		if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (xmlChar *)"feature")) {
			nam = xmlGetProp(cur_node, (xmlChar *)"name");
			val = xmlGetProp(cur_node, (xmlChar *)"value");
			if (!xmlStrcmp(nam,(xmlChar *)"slot"))
				strcpy(cart.slot,(char *)val);
			else if (!xmlStrcmp(nam,(xmlChar *)"pcb"))
				strcpy(cart.pcb,(char *)val);
			else if (!xmlStrcmp(nam,(xmlChar *)"vrc2-pin3"))
				cart.vrcPrg1 = strtol((char *)val+5,NULL,10);
			else if (!xmlStrcmp(nam,(xmlChar *)"vrc2-pin4"))
				cart.vrcPrg0 = strtol((char *)val+5,NULL,10);
			else if (!xmlStrcmp(nam,(xmlChar *)"vrc4-pin3"))
				cart.vrcPrg1 = strtol((char *)val+5,NULL,10);
			else if (!xmlStrcmp(nam,(xmlChar *)"vrc4-pin4"))
				cart.vrcPrg0 = strtol((char *)val+5,NULL,10);
			else if (!xmlStrcmp(nam,(xmlChar *)"vrc2-pin21")) {
				if (!xmlStrcmp(val,(xmlChar *)"NC"))
					cart.vrcChr = 0;
				else
					cart.vrcChr = 1;
			}
			else if (!xmlStrcmp(nam,(xmlChar *)"mmc1_type"))
				strcpy(cart.mmc1_type,(char *)val);
			else if (!xmlStrcmp(nam,(xmlChar *)"mirroring")) {
				if (!xmlStrcmp(val,(xmlChar *)"horizontal"))
					cart.mirroring = 0;
				else if (!xmlStrcmp(val,(xmlChar *)"vertical"))
					cart.mirroring = 1;
				else if (!xmlStrcmp(val,(xmlChar *)"high"))
					cart.mirroring = 3;
			}
			xmlFree(nam);
			xmlFree(val);

		} else if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (xmlChar *)"dataarea")) {
				nam = xmlGetProp(cur_node, (xmlChar *)"name");
				if (cur_node->children->next)
					val = xmlGetProp(cur_node->children->next, (xmlChar *)"size");
				else
					val = xmlGetProp(cur_node, (xmlChar *)"size");
				if (!xmlStrcmp(nam,(xmlChar *)"prg"))
					cart.prgSize = strtol((char *)val,NULL,10);
				else if (!xmlStrcmp(nam,(xmlChar *)"chr"))
					cart.chrSize = strtol((char *)val,NULL,10);
				else if (!xmlStrcmp(nam,(xmlChar *)"wram"))
					cart.wramSize = strtol((char *)val,NULL,10);
				else if (!xmlStrcmp(nam,(xmlChar *)"bwram"))
					cart.bwramSize = strtol((char *)val,NULL,10);
				else if (!xmlStrcmp(nam,(xmlChar *)"vram"))
					cart.cramSize = strtol((char *)val,NULL,10);
				xmlFree(nam);
				xmlFree(val);
		}
		cur_node = cur_node->next;
	}
}

void xml_hash_compare(xmlNode * c_node) {
	xmlNode *cur_node = NULL;
	xmlChar *hashkey;
	for (cur_node = c_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (const xmlChar *) "rom")) {
		    hashkey = xmlGetProp(cur_node, (xmlChar *)"sha1");
		    if (!xmlStrcmp(hashkey,sphash)) {
		    	extract_xml_data(cur_node->parent->parent);
		    }
		    xmlFree(hashkey);
		}
	}
}
void parse_xml_file(xmlNode * a_node) {
	xmlNode *cur_node = NULL;
	xmlChar *key;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    	if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (const xmlChar *) "dataarea")) {
    		key = xmlGetProp(cur_node, (xmlChar *)"name");
    		if (!xmlStrcmp(key, (xmlChar *)"prg")) {
    			xml_hash_compare(cur_node->children);
    		}
    		xmlFree(key);
        }
        parse_xml_file(cur_node->children);
    }
}
