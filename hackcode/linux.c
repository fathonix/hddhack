//Modified from example in http://www.simtec.co.uk/products/SWLINUX/files/booting_article.html
/*
	WD flash firmware hack, copyright Sprite_tm 2013.

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

#include "xprintf.h"
#include <stdint.h>

#define u32 uint32_t
#define u16 uint16_t
#define u8 uint8_t


/* list of possible tags */
#define ATAG_NONE       0x00000000
#define ATAG_CORE       0x54410001
#define ATAG_MEM        0x54410002
#define ATAG_VIDEOTEXT  0x54410003
#define ATAG_RAMDISK    0x54410004
#define ATAG_INITRD2    0x54420005
#define ATAG_SERIAL     0x54410006
#define ATAG_REVISION   0x54410007
#define ATAG_VIDEOLFB   0x54410008
#define ATAG_CMDLINE    0x54410009

/* structures for each atag */
struct atag_header {
        u32 size; /* length of tag in words including this header */
        u32 tag;  /* tag type */
};

struct atag_core {
        u32 flags;
        u32 pagesize;
        u32 rootdev;
};

struct atag_mem {
        u32     size;
        u32     start;
};

struct atag_videotext {
        u8              x;
        u8              y;
        u16             video_page;
        u8              video_mode;
        u8              video_cols;
        u16             video_ega_bx;
        u8              video_lines;
        u8              video_isvga;
        u16             video_points;
};

struct atag_ramdisk {
        u32 flags;
        u32 size;
        u32 start;
};

struct atag_initrd2 {
        u32 start;
        u32 size;
};

struct atag_serialnr {
        u32 low;
        u32 high;
};

struct atag_revision {
        u32 rev;
};

struct atag_videolfb {
        u16             lfb_width;
        u16             lfb_height;
        u16             lfb_depth;
        u16             lfb_linelength;
        u32             lfb_base;
        u32             lfb_size;
        u8              red_size;
        u8              red_pos;
        u8              green_size;
        u8              green_pos;
        u8              blue_size;
        u8              blue_pos;
        u8              rsvd_size;
        u8              rsvd_pos;
};

struct atag_cmdline {
        char    cmdline[1];
};

struct atag {
        struct atag_header hdr;
        union {
                struct atag_core         core;
                struct atag_mem          mem;
                struct atag_videotext    videotext;
              struct atag_ramdisk      ramdisk;
                struct atag_initrd2      initrd2;
                struct atag_serialnr     serialnr;
                struct atag_revision     revision;
                struct atag_videolfb     videolfb;
                struct atag_cmdline      cmdline;
        } u;
};


#define tag_next(t)     ((struct tag *)((u32 *)(t) + (t)->hdr.size))
#define tag_size(type)  ((sizeof(struct atag_header) + sizeof(struct type)) >> 2)
static struct atag *params; /* used to point at the current tag */

static void
setup_core_tag(void * address,long pagesize)
{
    params = (struct tag *)address;         /* Initialise parameters to start at given address */

    params->hdr.tag = ATAG_CORE;            /* start with the core tag */
    params->hdr.size = tag_size(atag_core); /* size the tag */

    params->u.core.flags = 1;               /* ensure read-only */
    params->u.core.pagesize = pagesize;     /* systems pagesize (4k) */
    params->u.core.rootdev = 0;             /* zero root device (typicaly overidden from commandline )*/

    params = tag_next(params);              /* move pointer to next tag */
}

static void
setup_ramdisk_tag(u32 size)
{
    params->hdr.tag = ATAG_RAMDISK;         /* Ramdisk tag */
    params->hdr.size = tag_size(atag_ramdisk);  /* size tag */

    params->u.ramdisk.flags = 0;            /* Load the ramdisk */
    params->u.ramdisk.size = size;          /* Decompressed ramdisk size */
    params->u.ramdisk.start = 0;            /* Unused */

    params = tag_next(params);              /* move pointer to next tag */
}

static void
setup_initrd2_tag(u32 start, u32 size)
{
    params->hdr.tag = ATAG_INITRD2;         /* Initrd2 tag */
    params->hdr.size = tag_size(atag_initrd2);  /* size tag */

    params->u.initrd2.start = start;        /* physical start */
    params->u.initrd2.size = size;          /* compressed ramdisk size */

    params = tag_next(params);              /* move pointer to next tag */
}

static void
setup_mem_tag(u32 start, u32 len)
{
    params->hdr.tag = ATAG_MEM;             /* Memory tag */
    params->hdr.size = tag_size(atag_mem);  /* size tag */

    params->u.mem.start = start;            /* Start of memory area (physical address) */
    params->u.mem.size = len;               /* Length of area */

    params = tag_next(params);              /* move pointer to next tag */
}

#if 0
static void
setup_cmdlineq_tag(const char * line)
{
    int linelen = strlen(line);

    if(!linelen)
        return;                             /* do not insert a tag for an empty commandline */

    params->hdr.tag = ATAG_CMDLINE;         /* Commandline tag */
    params->hdr.size = (sizeof(struct atag_header) + linelen + 1 + 4) >> 2;

    strcpy(params->u.cmdline.cmdline,line); /* place commandline into tag */

    params = tag_next(params);              /* move pointer to next tag */
}

#endif

static void
setup_end_tag(void)
{
    params->hdr.tag = ATAG_NONE;            /* Empty tag ends list */
    params->hdr.size = 0;                   /* zero length */
}


#define DRAM_BASE 0x29000000
#define ZIMAGE_LOAD_ADDRESS DRAM_BASE + 0x8000
#define INITRD_LOAD_ADDRESS DRAM_BASE + 0x800000

static void
setup_tags(parameters)
{
    setup_core_tag(parameters, 4096);       /* standard core tag 4k pagesize */
    setup_mem_tag(DRAM_BASE, 0x03000000);    /* 64Mb at 0x10000000 */
//    setup_mem_tag(0, 0x20000);    /* 128kb at 0x0 */
    setup_mem_tag(0, 0x1000);    /* 128kb at 0x0 */
//    setup_ramdisk_tag(4096);                /* create 4Mb ramdisk */ 
//    setup_initrd2_tag(INITRD_LOAD_ADDRESS, 0x100000); /* 1Mb of compressed data placed 8Mb into memory */
//    setup_cmdline_tag("root=/dev/ram0");    /* commandline setting root device */
    setup_end_tag();                    /* end of tags */
}


#define DATAPERSECTOR (512-(3*4))
#define MAGIC_FIRST 0x31586e4c //'LnX1'
#define MAGIC_MORE 0x21586e4c //'LnX!'


#define SECTSTART ((struct Sector*)0x28000000)
#define SECTEND ((struct Sector*)0x2C000000)
struct Sector {
	uint32_t magic;
	uint32_t nonce;
	uint32_t sectorNo;
	char data[DATAPERSECTOR];
};

#define NULL ((void*)0)

int load_image(uint32_t addr){
	char *dst=(char*)addr;
	int x;
	struct Sector *cacheSect;
	struct Sector *head=NULL;
	struct Sector *phead;
	struct Sector *oldPos=SECTSTART;
	
	//Find entry with biggest nonce.
	head->nonce=0; head->magic=0;
	for (phead=SECTSTART; phead!=SECTEND; phead++) {
		if (phead->magic==MAGIC_FIRST) {
			if (head==NULL || phead->nonce>head->nonce) {
				head=phead;
			}
		}
	}
	if (head==NULL) {
		xprintf("ldlnx: No initial magic found.\n");
		return 1;
	}
	for (x=0; x<head->sectorNo; x++) {
		//Find sector
		int found=0;
		cacheSect=oldPos;
		while(1) {
			if (cacheSect->magic==MAGIC_MORE && cacheSect->nonce==head->nonce && cacheSect->sectorNo==x) {
				found=1;
				break;
			}
			cacheSect++;
			if (cacheSect==SECTEND) cacheSect=SECTSTART;
			if (cacheSect==oldPos) break;
		}
		if (!found) {
			xprintf("ldlnx: Missing sector %d of %d.\n", x, head->sectorNo);
			return 0;
		}
		memcpy(dst, cacheSect->data, DATAPERSECTOR);
		if ((x&31)==0)xprintf("ldlnx: Loaded sector %d of %d to addr 0x%08x.\n", x, head->sectorNo, dst);
		dst+=DATAPERSECTOR;
		oldPos=cacheSect;
	}
	xprintf("ldlnx: Loaded %d sectors. Nonce = %08x\n", head->sectorNo, head->nonce);
	return 1;
}

int get_mach_type() {
	return 4173;
}

void irq_shutdown() {
	uint16_t *irqRegs=(uint16_t*)0x1ffe0000;
	irqRegs[1]=0;
	irqRegs[3]=0;
}

int
start_linux()
{
    void (*theKernel)(int zero, int arch, u32 params);
    u32 exec_at = (u32)-1;
    u32 parm_at = (u32)-1;
    u32 machine_type;
	int r;

    exec_at = ZIMAGE_LOAD_ADDRESS;
    parm_at = DRAM_BASE + 0x100;

    r=load_image(exec_at);              /* copy image into RAM */
	if (!r) return 1;
//  load_image(INITRD_LOAD_ADDRESS);/* copy initial ramdisk image into RAM */
    setup_tags(parm_at);                    /* sets up parameters */
    machine_type = get_mach_type();         /* get machine type */
    irq_shutdown();                         /* stop irq */
//    cpu_op(CPUOP_MMUCHANGE, NULL);          /* turn MMU off */
    theKernel = (void (*)(int, int, u32))exec_at; /* set the kernel address */
	xprintf("Jumping to kernel uncompressor...\n");
    theKernel(0, machine_type, parm_at);    /* jump to kernel with register set */
	xprintf("Returned from kernel :?\n");

    return 0;
}

