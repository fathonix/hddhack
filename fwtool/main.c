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
#include "wdromfmt.h"
#include "diskio.h"

void showUsage(char *name) {
	printf("Usage: \n");
	printf("%s -i flash.bin - Dump info about flash file\n", name);
	printf("%s -d flash.bin - Disassemble flash file into blocks\n", name);
	printf("%s -a flash.ini out.bin - Assemble flash from ini file\n", name);
	printf("%s -R /dev/sdx flash.bin - Dump flash from drive\n", name);
	printf("%s -W /dev/sdx flash.bin - Flash drive from file (DANGEROUS)\n", name);
	printf("%s -X /dev/sdx - Reset drive (unfortunately doesn't reload firmware)\n", name);
	exit(0);
}

int main(int argc, char **argv) {
	int f,x;
	char *flash;
	if (argc<=1) {
		showUsage(argv[0]);
		exit(1);
	}
	if (strcmp(argv[1],"-i")==0 && argc==3) {
		flash=flashFileLoad(argv[2]);
		showFwInfo(flash);
	} else if (strcmp(argv[1],"-d")==0 && argc==3) {
		flash=flashFileLoad(argv[2]);
		dumpFlashBlocks(flash, argv[2]);
	} else if (strcmp(argv[1],"-a")==0 && argc==4) {
		assembleFlashFile(argv[2], argv[3]);
	} else if (strcmp(argv[1],"-R")==0 && argc>=4) {
		diskReadRom(argv[2], argv[3], argc==5 && strcmp(argv[4],"--force")==0);
	} else if (strcmp(argv[1],"-W")==0 && argc>=4) {
		diskWriteRom(argv[2], argv[3], argc==5 && strcmp(argv[4],"--force")==0);
	} else if (strcmp(argv[1],"-X")==0 && argc==3) {
		diskReset(argv[2]);
	} else {
		showUsage(argv[0]);
		exit(1);
	}
	return 0;
}
