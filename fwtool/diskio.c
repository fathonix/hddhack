/*
	Disk IO code, somewhat copyright Sprite_tm 2013.
	
	This code borroes heavily from:
    idle3ctl - Disable, get or set the idle3 timer of Western Digital HDD
    Copyright (C) 2011  Christophe Bothamy (cbothamy at users.sf.net)

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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <linux/types.h>
#include <errno.h>

#include "sgio.h"
int prefer_ata12 = 0;
int verbose = 0;

int vscenabled = 0;
int force = 0;

char *device;
char *progname;
int fd=0;

#define VERSION "0.9.1"

#define VSC_KEY_WRITE 0x02
#define VSC_KEY_READ 0x01

#define ROMTYPE_INTERNAL 2
#define ROMTYPE_SPI 3

int check_WDC_drive()
{
  static __u8 atabuffer[4+512];
  int i;

  if (verbose) {
    printf("Checking if Drive is a Western Digital Drive\n");
  }

  memset(atabuffer, 0, sizeof(atabuffer));
  atabuffer[0] = ATA_OP_IDENTIFY;
  atabuffer[3] = 1;
  if (do_drive_cmd(fd, atabuffer)) {
    perror(" HDIO_DRIVE_CMD(identify) failed");
    return errno;
  } 

  if (!force) {
    /* Check for a Western Digital Drive  3 first characters : WDC*/
    if ( (atabuffer[4+(27*2)+1] != 'W')
      || (atabuffer[4+(27*2)] != 'D')
      || (atabuffer[4+(28*2)+1] != 'C')) {
      fprintf(stderr, "The drive %s does not seem to be a Western Digital Drive ",device);
      fprintf(stderr, "but a ");
      for (i=27; i<47; i++) {
        if(atabuffer[4+(i*2)+1]==0)break;
        putchar(atabuffer[4+(i*2)+1]);
        if(atabuffer[4+(i*2)+0]==0)break;
        putchar(atabuffer[4+(i*2)+0]);
      }
      printf("\n");

      fprintf(stderr, "Use the --force option if you know what you're doing\n");
      return 1;
    }
  }

  return 0;
}

int diskOpen(char *fileName, int force) {
	device=fileName;
	fd=open(fileName, O_RDWR);
	if (fd<0) {
		perror(fileName);
		exit(1);
	}
	// Check if HDD is a WD
	if (check_WDC_drive() != 0) {
		if (!force) exit(1);
	}
}


/* Enable Vendor Specific Commands */
int VSC_enable()
{
  if (verbose) {
    printf("Enabling Vendor Specific ATA commands\n");
  }

  int err = 0;
  struct ata_tf tf;
  tf_init(&tf, ATA_OP_VENDOR_SPECIFIC, 0, 0);
  tf.lob.feat = 0x45;
//  tf.lob.nsect = 0x0b; //sector count
  tf.lob.lbam = 0x44;
  tf.lob.lbah = 0x57;
  tf.dev = 0xa0;

  if(sg16(fd, SG_WRITE, SG_PIO, &tf, NULL, 0, 5)) {
    err = errno;
    perror("sg16(VSC_ENABLE) failed");
    return err;
  }
  vscenabled = 1;
  return 0;
}

/* Disable Vendor Specific Commands */
int VSC_disable()
{
  if (verbose) {
    printf("Disabling Vendor Specific ATA commands\n");
  }

  int err = 0;
  struct ata_tf tf;
  tf_init(&tf, ATA_OP_VENDOR_SPECIFIC, 0, 0);
  tf.lob.feat = 0x44;
  tf.lob.lbam = 0x44;
  tf.lob.lbah = 0x57;
  tf.dev = 0xa0;

  if(sg16(fd, SG_WRITE, SG_PIO, &tf, NULL, 0, 5)) {
    err = errno;
    perror("sg16(VSC_DISABLE) failed");
    return err;
  }
  vscenabled = 0;
  return 0;
}

/* Vendor Specific Commands sendkey */
int VSC_send_romaccess(char rw, char romtype)
{
  int err = 0;
  char buffer[512];
  struct ata_tf tf;

  tf_init(&tf, ATA_OP_SMART, 0, 0);
  tf.lob.feat = 0xd6; //smart write log
  tf.lob.nsect = 0x01; //sector count
  tf.lob.lbal = 0xbe; //log page
  tf.lob.lbam = 0x4f; //const
  tf.lob.lbah = 0xc2; //const
  tf.dev = 0xa0; //const

  memset(buffer,0,sizeof(buffer));
  buffer[0]=0x24; //cmd
  buffer[2]=rw;	  //par1
//Enabling this will limit the ROM to 192K... :/
//  buffer[10]=romtype; //par5

  if(sg16(fd, SG_WRITE, SG_PIO, &tf, buffer, 512, 2)) {
    err = errno;
    perror("sg16(VSC_SENDKEY) failed");
    return err;
  }
  vscenabled = 0;
  return 0;
}

/* Vendor Specific Commands get block */
int VSC_get_block(char *buffer, int len)
{
  int err = 0;
  struct ata_tf tf;

  tf_init(&tf, ATA_OP_SMART, 0, 0);
  tf.lob.feat = 0xd5; //smart read log
  tf.lob.nsect = len/512;
  tf.lob.lbal = 0xbf;
  tf.lob.lbam = 0x4f;
  tf.lob.lbah = 0xc2;
  tf.dev = 0xa0;

  memset(buffer,0,sizeof(buffer));

  if(sg16(fd, SG_READ, SG_PIO, &tf, buffer, len, 5)) {
    err = errno;
    perror("sg16(VSC_GET_BLOCK) failed");
    return err;
  }
}


int VSC_send_block(char *buffer, int len)
{
  int err = 0;
  struct ata_tf tf;

  tf_init(&tf, ATA_OP_SMART, 0, 0);
  tf.lob.feat = 0xd6; //smart write log
  tf.lob.nsect = len/512;
  tf.lob.lbal = 0xbf;
  tf.lob.lbam = 0x4f;
  tf.lob.lbah = 0xc2;
  tf.dev = 0xa0;

  if(sg16(fd, SG_WRITE, SG_PIO, &tf, buffer, len, 20)) {
    err = errno;
    perror("sg16(VSC_SEND_BLOCK) failed");
  }
}


void diskReset(char *disk) {
  int err = 0;
  struct ata_tf tf;

  diskOpen(disk, 1);
  tf_init(&tf, ATA_OP_IDENTIFY, 0, 0);
//  tf.control = 0x4; //SRST

  if(sg16(fd, SG_READ, SG_PIO, &tf, NULL, 0, 5)) {
    err = errno;
    perror("sg16(RESET) failed");
  }

  tf.dev = 0xa0;

  if(sg16(fd, SG_WRITE, SG_PIO, &tf, NULL, 0, 5)) {
    err = errno;
    perror("sg16(UNRESET) failed");
  }

}

void diskReadRom(char* disk, char* romFile, int force) {
	int out;
	int x;
	char rom[256*1024];
	out=open(romFile, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (out<0) {
		perror(romFile);
		exit(1);
	}
	diskOpen(disk, force);
	printf("Enable VSC mode...\n");
	if (VSC_enable()!=0) exit(1);
	printf("Send command to read flash\n");
	if (VSC_send_romaccess(VSC_KEY_READ, ROMTYPE_SPI)!=0) exit(1);
	printf("Get flash parts\n");
	for (x=0; x<sizeof(rom); x+=64*1024) {
		printf("%i\n",x);
		VSC_get_block(&rom[x], 64*1024);
	}
	write(out, rom, sizeof(rom));
	close(out);

	printf("Disable VSC mode...\n");
	VSC_disable();
	printf("All done.\n");
}


void diskWriteRom(char* disk, char* romFile, int force) {
	int inf;
	int x;
	char rom[256*1024];

	inf=open(romFile, O_RDONLY);
	if (inf<0) {
		perror(romFile);
		exit(1);
	}
	read(inf, rom, sizeof(rom));
	close(inf);

	diskOpen(disk, force);

	printf("Enable VSC mode...\n");
	if (VSC_enable()!=0) exit(1);
	printf("Erasing flash memory...\n");
	if (VSC_send_romaccess(3, ROMTYPE_SPI)!=0) exit(1);
	printf("Send command to write flash\n");
	if (VSC_send_romaccess(VSC_KEY_WRITE, ROMTYPE_SPI)!=0) exit(1);
	printf("Write flash parts\n");
	for (x=0; x<sizeof(rom); x+=64*1024) {
		printf("%i\n",x);
		VSC_send_block(&rom[x], 64*1024);
	}
	printf("Disable VSC mode...\n");
	VSC_disable();
	printf("All done.\n");
}
