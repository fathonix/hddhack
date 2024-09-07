/*
	WD flash modding tool, copyright Sprite_tm 2013.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <regex.h>

//
typedef struct __attribute__((packed)) {
	uint8_t blockCode;
	uint8_t flag; //1=compressed, 2=not compressed, 3=compressed differently.
	uint8_t unk2;
	uint8_t unk3;
	uint32_t blockLenPlusCs;
	uint32_t blockLen;
	uint32_t blockStartAddr;
	uint32_t blockLoadAddr;
	uint32_t blockExecAddr;
	uint32_t unk4;
	uint32_t fstwPlusCs;
} FlashHdrEntry;


//Ini compiled regexps. Global because we only want to compile these once.
int regexesCompiled=0;
regex_t sectionRegex;
regex_t valueRegex;

//Little-endian to native endian
uint32_t le32ToN(uint32_t n) {
	unsigned char *p=(unsigned char*)&n;
	return p[0]+(p[1]<<8)+(p[2]<<16)+(p[3]<<24);
}

//Native endian to little-endian
uint32_t n32ToLe(uint32_t n) {
	uint32_t ret;
	unsigned char *p=(unsigned char *)&ret;
	p[0]=(n)&0xff;
	p[1]=(n>>8)&0xff;
	p[2]=(n>>16)&0xff;
	p[3]=(n>>24)&0xff;
	return ret;
}

//Calc checksum over a header line
unsigned int calcHdrLineCs(FlashHdrEntry *h) {
	int x;
	uint8_t cs=0;
	unsigned char *p=(char*)h;
	for (x=0; x<31; x++) {
		cs+=p[x];
	}
	return cs;
}


int checkHdrLineCs(FlashHdrEntry *h) {
	int x;
	uint8_t cs=calcHdrLineCs(h);
	unsigned char *p=(char*)h;
	if (cs!=p[31]) {
		printf("Header csum err. calc: %hhx read: %hhx\n", cs, p[31]);
	} else {
		printf("Header csum OK: %hhx\n", cs);
	}
	return cs==p[31];
}


void showHdrLine(FlashHdrEntry *h) {
	printf("\nBlock code:              %hhx\n", h->blockCode);
	printf("Unk1/2/3:                %hhx %hhx %hhx\n", h->flag, h->unk2, h->unk3);
	printf("Block len + cs:          %x\n", le32ToN(h->blockLenPlusCs));
	printf("Block len:               %x\n", le32ToN(h->blockLen));
	printf("Block start paddr:       %x\n", le32ToN(h->blockStartAddr));
	printf("Block load vaddress:     %x\n", le32ToN(h->blockLoadAddr));
	printf("Block execute vaddress:  %x\n", le32ToN(h->blockExecAddr));
	printf("unk4:                    %x\n", le32ToN(h->unk4));
	printf("'1st word' plus chsum:   %x\n", le32ToN(h->fstwPlusCs));
}

//Calculate 8-bit chsum
unsigned int calcDataCs8(unsigned char *p, int len) {
	unsigned char cs=0;
	int x;
	for (x=0; x<len; x++) cs+=p[x];
	return cs;
}

//Calculate 16-bit chsum
unsigned int calcDataCs16(unsigned char *p, int len) {
	unsigned int cs=0;
	int x;
	for (x=0; x<len; x+=2) {
		cs+=p[x+1]<<8;
		cs+=p[x];
	}
	return cs&0xffff;
}

int showFwInfo(unsigned char *flash) {
	int x;
	FlashHdrEntry *fl;
	for (x=0; x<32; x++) {
		fl=(FlashHdrEntry*)(flash+(x*sizeof(FlashHdrEntry)));
		if (fl->blockCode<=0xa || fl->blockCode==0x5a) {
			showHdrLine(fl);
			if (checkHdrLineCs(fl)) {
				//Check chsum of data
				int ccs=0, rcs=0;
				if (fl->blockLenPlusCs-fl->blockLen==1) {
					ccs=calcDataCs8(&flash[fl->blockStartAddr], fl->blockLen);
					rcs=flash[fl->blockStartAddr+fl->blockLen];
				} else if (fl->blockLenPlusCs-fl->blockLen==2) {
					ccs=calcDataCs16(&flash[fl->blockStartAddr], fl->blockLen);
					rcs=flash[fl->blockStartAddr+fl->blockLen];
					rcs+=flash[fl->blockStartAddr+fl->blockLen+1]<<8;
				} else {
					printf("Huh? Checksum is %i bytes!\n", (fl->blockLen-fl->blockLenPlusCs));
				}
				if (ccs!=rcs) {
					printf("Data chsum error! Calc %x read %x\n", ccs, rcs);
				} else {
					printf("Data checksum OK: %x\n", ccs);
				}
			}
		} else {
			break;
		}
	}
	printf("Header end at 0x%x.\n", (x*sizeof(FlashHdrEntry)));
}

void dumpFlashBlocks(unsigned char *flash, char *origFile) {
	int x;
	FlashHdrEntry *fl;
	FILE *ini;
	ini=fopen("blocks.ini", "w");
	fprintf(ini, "#fwtool flash description ini file thingy\n\n");
	//The origFile is needed because the end of the flash contains some configuration sectors
	//that aren't described by the header. By including the original filename in the description,
	//we can copy them verbosely.
	fprintf(ini, "origfile=%s\n\n", origFile);
	for (x=0; x<32; x++) {
		fl=(FlashHdrEntry*)(flash+(x*sizeof(FlashHdrEntry)));
		if (fl->blockCode<=0xa || fl->blockCode==0x5a) {
			if (checkHdrLineCs(fl)) {
				char fn[256];
				int of;
				sprintf(fn, "block%hhx.bin", fl->blockCode);
				printf("Dumping %s...\n", fn);
				of=open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0666);
				if (of<0) {
					perror(fn);
					exit(1);
				}
				write(of, &flash[fl->blockStartAddr], fl->blockLen);
				close(of);

				//Dump info in ini
				fprintf(ini, "[%s]\n", fn);
				fprintf(ini, "blockcode=0x%hhx\n", fl->blockCode);
				fprintf(ini, "flag=0x%hhx\n", fl->flag);
				fprintf(ini, "unk2=0x%hhx\n", fl->unk2);
				fprintf(ini, "unk3=0x%hhx\n", fl->unk3);
				fprintf(ini, "blocklencs=0x%x\n", le32ToN(fl->blockLenPlusCs));
				fprintf(ini, "blocklen=0x%x\n", le32ToN(fl->blockLen));
				fprintf(ini, "blockstartaddr=0x%x\n", le32ToN(fl->blockStartAddr));
				fprintf(ini, "blockloadaddr=0x%x\n", le32ToN(fl->blockLoadAddr));
				fprintf(ini, "blockexecaddr=0x%x\n", le32ToN(fl->blockExecAddr));
				fprintf(ini, "unk4=0x%x\n", le32ToN(fl->unk4));
				fprintf(ini, "fstwPlusCs=0x%x\n", le32ToN(fl->fstwPlusCs));
				fprintf(ini, "\n");
			}
		}
	}
	fclose(ini);
}

unsigned char* flashFileLoad(char *file) {
	unsigned char *flash;
	FlashHdrEntry *fl;
	int f;
	f=open(file, O_RDONLY);
	if (f<1) {
		perror("opening");
		exit(1);
	}
	
	flash=mmap(NULL, lseek(f, 0, SEEK_END), PROT_READ, MAP_SHARED, f, 0);
	if (flash==NULL) {
		perror("mmap");
		exit(1);
	}
	return flash;
}


#define INILINE_NONE 0
#define INILINE_SECTION 1
#define INILINE_VALUE 2

//Parse a line from an ini file.
//For a section, this returns INILINE_SECTION and sets name to the section name
//For a name=val pair, this returns INILINE_VALUE and sets name and value accordingly.
int parseIniLine(char *line, char *name, char *value) {
	regmatch_t matches[3];
	int r;
	if (!regexesCompiled) {
		r=regcomp(&sectionRegex, "\\[ *([a-zA-Z0-9.-/]*) *\\].*", REG_EXTENDED|REG_ICASE);
		if (r!=0) printf("Regex compile error section\n");
		r=regcomp(&valueRegex, " *([a-zA-Z0-9]+) *= *(.*) *\n", REG_EXTENDED|REG_ICASE);
		if (r!=0) printf("Regex compile error value\n");
		regexesCompiled=1;
	}
	if (regexec(&sectionRegex, line, 3, matches, 0)==0) {
		strncpy(name, line+matches[1].rm_so, matches[1].rm_eo-matches[1].rm_so);
		name[matches[1].rm_eo-matches[1].rm_so]=0;
		return INILINE_SECTION;
	}
	if (regexec(&valueRegex, line, 3, matches, 0)==0) {
		strncpy(name, line+matches[1].rm_so, matches[1].rm_eo-matches[1].rm_so);
		name[matches[1].rm_eo-matches[1].rm_so]=0;
		strncpy(value, line+matches[2].rm_so, matches[2].rm_eo-matches[2].rm_so);
		value[matches[2].rm_eo-matches[2].rm_so]=0;
		return INILINE_VALUE;
	}
	return INILINE_NONE;
}

//Assemble a flash binary from an ini-file describing it, the blocks and the original file.
void assembleFlashFile(char* iniFile, char* outFile) {
	FILE *ini;
	int out, block;
	int r, x;
	char line[1024], name[1024], value[1024];
	char fileName[32][1024];
	FlashHdrEntry header[32];
	int curHeader=-1;
	char flashmem[256*1024];
	int flashpos;

	for (x=0; x<sizeof(flashmem); x++) flashmem[x]=0xff;

	//Open files.
	ini=fopen(iniFile, "r");
	if (ini==NULL) {
		perror(iniFile);
		exit(1);
	}
	out=open(outFile, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (out<0) {
		perror(outFile);
		exit(1);
	}

	//Parse ini file sections
	while (!feof(ini)) {
		fgets(line, 1024, ini);
		r=parseIniLine(line, name, value);
		if (r==INILINE_SECTION) {
			curHeader++;
			strcpy(fileName[curHeader], name);
		} else if (r==INILINE_VALUE) {
			if (curHeader>=0) {
				if (strcmp(name, "blockcode")==0) {
					header[curHeader].blockCode=strtoll(value, NULL, 0);
				} else if (strcmp(name, "flag")==0) {
					header[curHeader].flag=strtoll(value, NULL, 0);
				} else if (strcmp(name, "unk2")==0) {
					header[curHeader].unk2=strtoll(value, NULL, 0);
				} else if (strcmp(name, "unk3")==0) {
					header[curHeader].unk3=strtoll(value, NULL, 0);
				} else if (strcmp(name, "blocklencs")==0) {
					header[curHeader].blockLenPlusCs=n32ToLe(strtoll(value, NULL, 0));
				} else if (strcmp(name, "blocklen")==0) {
					header[curHeader].blockLen=n32ToLe(strtoll(value, NULL, 0));
				} else if (strcmp(name, "blockstartaddr")==0) {
					header[curHeader].blockStartAddr=n32ToLe(strtoll(value, NULL, 0));
				} else if (strcmp(name, "blockloadaddr")==0) {
					header[curHeader].blockLoadAddr=n32ToLe(strtoll(value, NULL, 0));
				} else if (strcmp(name, "blockexecaddr")==0) {
					header[curHeader].blockExecAddr=n32ToLe(strtoll(value, NULL, 0));
				} else if (strcmp(name, "unk4")==0) {
					header[curHeader].unk4=n32ToLe(strtoll(value, NULL, 0));
				} else if (strcmp(name, "fstwPlusCs")==0) {
					header[curHeader].fstwPlusCs=n32ToLe(strtoll(value, NULL, 0));
				} else {
					printf("Unknown field %s in ini file!\n", name);
					exit(1);
				}
			} else {
				//global section
				if (strcmp(name, "origfile")==0) {
					block=open(value, O_RDONLY);
					if (block<0) {
						perror(value);
						exit(1);
					}
					read(block, flashmem, sizeof(flashmem));
				} else {
					printf("Unknown field %s in ini file!\n", name);
					exit(1);
				}
			}
		}
	}

	flashpos=sizeof(FlashHdrEntry)*(curHeader+1);

	//Load blocks, correct headers
	for (x=0; x<=curHeader; x++) {
		int len;
		int cslen;
		unsigned int cs;
		unsigned char *p;
		printf("Adding block %hhx; file is %s\n", header[x].blockCode, fileName[x]);
		block=open(fileName[x], O_RDONLY);
		if (block<0) {
			perror(fileName[x]);
			exit(1);
		}
		//Get size of block
		len=lseek(block, 0, SEEK_END);
		lseek(block, 0, SEEK_SET);
		//Read into flash
		read(block, &flashmem[flashpos], len);
		
		//Correct size of block to size of file
		cslen=le32ToN(header[x].blockLenPlusCs)-le32ToN(header[x].blockLen);
		if (le32ToN(header[x].blockLen)!=len) {
			printf("Adjusting size of block %s from %x to 0x%x...\n", fileName[x], le32ToN(header[x].blockLen), len);
			header[x].blockLen=n32ToLe(len);
			header[x].blockLenPlusCs=n32ToLe(len+cslen);
		}
		//Correct offset in flash
		if (le32ToN(header[x].blockStartAddr)!=flashpos) {
			printf("Adjusting phys addr from %x to %x\n", le32ToN(header[x].blockStartAddr), flashpos);
			header[x].blockStartAddr=n32ToLe(flashpos);
		}
		//Correct header checksum
		cs=calcHdrLineCs(&header[x]);
		p=(unsigned char *)&header[x];
		if (p[31]!=cs) {
			printf("Adjusting header cs of block %s to 0x%x...\n", fileName[x], cs);
			p[31]=cs;
		}
		//Correct data checksum
		if (cslen==1) {
			cs=calcDataCs8(&flashmem[flashpos], len);
			flashmem[flashpos+len]=cs;
			len++;
		} else if (cslen==2) {
			cs=calcDataCs16(&flashmem[flashpos], len);
			flashmem[flashpos+len]=cs&0xff;
			flashmem[flashpos+len+1]=cs>>8;
			len+=2;
		} else {
			printf("%s: unknown cs len of %i! Can't calculate!\n", fileName[x], cslen);
			exit(1);
		}
		flashpos+=len;
		close(block);
	}
	//Copy headers to flash mem array
	for (x=0; x<=curHeader; x++) {
		memcpy(&flashmem[x*sizeof(FlashHdrEntry)], &header[x], sizeof(FlashHdrEntry));
	}
	//Write out data
	write(out, flashmem, sizeof(flashmem));
	close(out);
}
