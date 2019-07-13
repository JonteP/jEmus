#include "cartridge.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>	/* malloc; exit */
#include <string.h>
#include "parser.h"
#include "tree.h"
#include "sha.h"
#include "smsemu.h"

static inline struct RomFile load_rom(char *);
static inline xmlChar * calculate_checksum(uint8_t *, int);
static inline uint8_t * read0(uint16_t), * read1(uint16_t), * read2(uint16_t), * read3(uint16_t), * empty(uint16_t);
static inline void generic_mapper(), sega_mapper(), codemasters_mapper(), setup_banks();
static inline void extract_xml_data(xmlNode *,struct RomFile *), xml_hash_compare(xmlNode *,struct RomFile *), parse_xml_file(xmlNode *,struct RomFile *);
uint8_t fcr[3], *bank[3], bRam[0x8000], memControl, bramReg = 0, returnValue[1]={0}, cpuRam[0x2000];
struct RomFile cartRom, cardRom, biosRom, expRom, *currentRom;
char *xmlFile = "sms.xml", *bName;
FILE *bFile;
xmlDoc *smsXml;

void init_slots()
{
	if((smsXml = xmlReadFile(xmlFile, NULL, 0)) == 0){
		fprintf(stderr,"Error: %s could not be opened.\n",xmlFile);
		exit(1);}
	biosRom = load_rom(biosFile);
	cartRom = load_rom(cartFile);
	cardRom = load_rom(cardFile);
	expRom = load_rom(expFile);
	xmlFree(smsXml);
	memory_control(0xe7);
}

void memory_control(uint8_t value){
	/* TODO: wram + ioport enable/disable */
	memControl = value;
	if(!(memControl & 0x80)){
		currentRom = &expRom;
		banking = &generic_mapper;
	}
	else if(!(memControl & 0x40)){
		currentRom = &cartRom;
	}
	else if(!(memControl & 0x20)){
		currentRom = &cardRom;
		banking = &generic_mapper;
	}
	else if(!(memControl & 0x08)){
		currentRom = &biosRom;
		if(currentRom->mapper == CODEMASTERS){
			banking = &codemasters_mapper;
			fcr[0] = 0;
			fcr[1] = 1;
			fcr[2] = 0;
		}
		else if(currentRom->mapper == SEGA){
			banking = &sega_mapper;
			fcr[0] = 0;
			fcr[1] = 1;
			fcr[2] = 2;
		}
		else
			banking = &generic_mapper;
	}
	banking();
	setup_banks();
}

void generic_mapper(){
	bank[0] = currentRom->rom;
	bank[1] = currentRom->rom + 0x4000;
	bank[2] = currentRom->rom + 0x8000;
}

void sega_mapper(){
	/* TODO:
	 * bank shifting
	 * mapping of slot 3
	 *  */
	bank[0] = currentRom->rom + ((fcr[0] & currentRom->mask) << 14);
	bank[1] = currentRom->rom + ((fcr[1] & currentRom->mask) << 14);
	bank[2] = (bramReg & 0x8) ? (bRam + ((bramReg & 0x4) << 12)) : currentRom->rom + ((fcr[2] & currentRom->mask) << 14);
}

void codemasters_mapper(){
	/* TODO: RAM mapping */
	bank[0] = currentRom->rom;
	bank[1] = currentRom->rom + 0x4000;
	bank[2] = currentRom->rom + ((fcr[2] & currentRom->mask) << 14);
}

void setup_banks(){
	/* TODO: implement RAM disable */
	read_bank3 = &read3;
	if(currentRom->rom == NULL){
		read_bank0 = &empty;
		read_bank1 = &empty;
		read_bank2 = &empty;
	}
	else{
		read_bank0 = &read0;
		read_bank1 = &read1;
		read_bank2 = &read2;
	}
}

uint8_t * empty(uint16_t address){
	/* TODO: different read back value on sms 1 */
	returnValue[0]=0xff;
	return returnValue;
}
uint8_t * read0(uint16_t address){
	uint8_t *value;
	if (address < 0x400)
		value = (currentRom->rom + (address & 0x3ff));
	else
		value = &bank[0][address & 0x3fff];
	return value;
}
uint8_t * read1(uint16_t address){
	uint8_t *value = &bank[1][address & 0x3fff];
	return value;
}
uint8_t * read2(uint16_t address){
	uint8_t *value = &bank[2][address & 0x3fff];
	return value;
}
uint8_t * read3(uint16_t address){
	/* TODO: bankable */
	uint8_t *value = &cpuRam[address & 0x1fff];
	return value;
}

struct RomFile load_rom(char *r){/* TODO: add check for cart in card slot etc. */
	FILE *rfile = fopen(r, "r");
	if(rfile == NULL){
		struct RomFile output = { NULL };
		output.sha1 = NULL;
		return output;
	}
	fseek(rfile, 0L, SEEK_END);
	int rsize = ftell(rfile);
	rewind(rfile);
	uint8_t *tmpRom = malloc(rsize);
	fread(tmpRom, rsize, 1, rfile);
	uint8_t mask = ((rsize >> 14) - 1);
	struct RomFile output = { tmpRom, mask };
	if(rsize > 0x8000)
		output.mapper = SEGA;
	else
		output.mapper = GENERIC;
	output.battery=0;
	output.sha1=calculate_checksum(tmpRom,rsize);
	parse_xml_file(xmlDocGetRootElement(smsXml),&output);
	if(output.battery){
		bName = strdup(r);
		sprintf(bName+strlen(bName)-3, "sav");
		if((bFile = fopen(bName,"r")) != NULL){
			fread(bRam, 0x8000, 1, bFile);
			fclose(bFile);
		}
	}
	return output;
}
void close_rom()
{
	if(cartRom.rom != NULL){
		free(cartRom.rom);
		free(cartRom.sha1);
	}
	if(biosRom.rom != NULL){
		free(biosRom.rom);
		free(biosRom.sha1);
	}
	if(cardRom.rom != NULL){
		free(cardRom.rom);
		free(cardRom.sha1);
	}
	if(expRom.rom != NULL){
		free(expRom.rom);
		free(expRom.sha1);
	}
	if(currentRom->battery){
		bFile = fopen(bName,"w");
		fwrite(bRam,0x8000,1,bFile);
		fclose(bFile);
	}
}

xmlChar * calculate_checksum(uint8_t *data, int size){
	unsigned char hash[SHA_DIGEST_LENGTH];
	SHA1(data,size,hash);
	xmlChar *shash = malloc(SHA_DIGEST_LENGTH*2+1);
	for (int i = 0; i<sizeof(hash); i++)
	{
		sprintf((char *)shash+i*2, "%02x", hash[i]);
	}
	return shash;
}

void extract_xml_data(xmlNode * node, struct RomFile *rom) {
	xmlNode *cur_node = node->children;
	xmlChar *nam, *val;
	while (cur_node) {
		if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (xmlChar *)"feature")) {
			nam = xmlGetProp(cur_node, (xmlChar *)"name");
			val = xmlGetProp(cur_node, (xmlChar *)"value");
			if (!xmlStrcmp(nam,(xmlChar *)"slot")){
				if (!xmlStrcmp(val,(xmlChar *)"codemasters"))
					rom->mapper = CODEMASTERS;
			}
			else if (!xmlStrcmp(nam,(xmlChar *)"battery")){
				if (!xmlStrcmp(val,(xmlChar *)"yes"))
					rom->battery = 1;
			}

			xmlFree(nam);
			xmlFree(val);
		}
		cur_node = cur_node->next;
	}
}

void xml_hash_compare(xmlNode * node, struct RomFile *rom)
{
	xmlNode *cur_node = NULL;
	xmlChar *hashkey;
	for (cur_node = node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (const xmlChar *) "rom")) {
		    hashkey = xmlGetProp(cur_node, (xmlChar *)"sha1");
		    if (!xmlStrcmp(hashkey,rom->sha1)) {
		    	extract_xml_data(cur_node->parent->parent, rom);
		    }
		    xmlFree(hashkey);
		}
	}
}
void parse_xml_file(xmlNode * node, struct RomFile *rom)
{
	xmlNode *cur_node = NULL;
	xmlChar *key;

    for (cur_node = node; cur_node; cur_node = cur_node->next) {
    	if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (const xmlChar *) "dataarea")) {
    		key = xmlGetProp(cur_node, (xmlChar *)"name");
    		if (!xmlStrcmp(key, (xmlChar *)"rom")) {
    			xml_hash_compare(cur_node->children, rom);
    		}
    		xmlFree(key);
        }
        parse_xml_file(cur_node->children, rom);
    }
}
