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
#include "hack.h"
#include "modify.h"
#include "serial.h"
#include "xprintf.h"
#include <stdint.h>

extern uint32_t orig_irq_vector;
int secondLoaderCalled=0;

void install_irqhandler() {
	uint32_t *irqpos=(uint32_t*)0x78; //irq
//	uint32_t *irqpos=(uint32_t*)0x74; //fiq
	if (*irqpos!=(uint32_t)irqcode) {
		orig_irq_vector=*irqpos;
		*irqpos=(uint32_t)irqcode;
	}
}

/*
The stuff we have to do to keep executed....

We hook the IRQ vector here (A) , plus a location a few words after some code which 
overwrites the irq-vector (B).

We can't overwrite (B) at the start, because that code gets loaded from disk. We
can't overwrite just (A) and be done, because (b) rewrites that. So we hook
both and have them re-install eachother if they;re overwritten. It's brute-force-y
but it works deliciously :)
*/



static uint8_t getChsum(uint8_t *loc, int len) {
	uint8_t chsum;
	int x;
	for (x=0; x<len; x++) {
		chsum+=*loc;
		loc++;
	}
	return chsum;
}

static void install_irqhandler_fixer() {
	uint32_t *patchLoc=(uint32_t*)0xffe5d370;
	uint8_t orgChsum;
	int x;
	if (*patchLoc!=0xe3a03000) {
		//Original code not found; either it isn't loaded yet or we already patched it.
		return;
	}

	orgChsum=getChsum((uint8_t*)patchLoc, 4*3);
	//Patch in jump to second_loader_hook
	patchLoc[0]=0xe51ff004; //ldr pc,[pc-#4] aka jump to the address in the next word
	patchLoc[1]=(uint32_t)second_loader_hook;
	//Calculate new checksum over range. Load-from-disk routines of the drive seem
	//to crap out if the 8-bit checksum over the region doesn't work out, so we use the
	//3rd word to compensate for changes in the checksum.
	uint8_t newChsum=getChsum((uint8_t*)patchLoc, 4*2);
	patchLoc[2]=(orgChsum-newChsum)&0xff;
}


static void install_satareqh_hook() {
	uint16_t *patchLoc=(uint16_t *)0x167c2;
	uint8_t orgChsum, newChsum;
	if (*patchLoc!=0x0101) return;
	orgChsum=getChsum((uint8_t*)patchLoc, 5*2);
	patchLoc[0]=0x4e01; //ldr r6,[pc-#6]
	patchLoc[1]=0x4730; //bx r6;
	patchLoc[2]=0; //unused, to align
	*((uint32_t *)&patchLoc[3])=(uint32_t)hooksatareqh;
	newChsum=getChsum((uint8_t*)patchLoc, 5*2);
	patchLoc[2]=(orgChsum-newChsum)&0xff;
}


static void install_satapioh_hook() {
	uint16_t *patchLoc=(uint16_t *)0x4402;
	uint8_t orgChsum, newChsum;
	if (*patchLoc!=0x0006) return;
	orgChsum=getChsum((uint8_t*)patchLoc, 5*2);
	patchLoc[0]=0x4e01; //ldr r6,[pc-#6]
	patchLoc[1]=0x4730; //bx r6;
	patchLoc[2]=0; //unused, to align
	*((uint32_t *)&patchLoc[3])=(uint32_t)hooksatapioh;
	newChsum=getChsum((uint8_t*)patchLoc, 5*2);
	patchLoc[2]=(orgChsum-newChsum)&0xff;

/*
	uint16_t *patchLoc=(uint16_t *)0xffe64e9a;
	uint8_t orgChsum, newChsum;
	if (*patchLoc!=0x0006) return;
	orgChsum=getChsum((uint8_t*)patchLoc, 5*2);
	patchLoc[0]=0x4e01; //ldr r6,[pc-#6]
	patchLoc[1]=0x4730; //bx r6;
	patchLoc[2]=0; //unused, to align
	*((uint32_t *)&patchLoc[3])=(uint32_t)hooksatapioh;
	newChsum=getChsum((uint8_t*)patchLoc, 5*2);
	patchLoc[2]=(orgChsum-newChsum)&0xff;
*/
}


/*
Program flow:
- ROM loader loads our code into memory
- ROM loader calls hack_init, other code loaded hasn't been touched yet
- hack_init installs irqhandler and irqhandler_fixer
- hack_init jumps to main WD code
- Main WD code overwrites irqhandler -> hack_init_second is hooked so it'll fix it again
- Main WD code overwrites hack_init_second hook -> irqhandler will fix this
*/

void hack_init() {
	//We're called after the boot-ROM has loaded the EEPROM.
	//Patch the HD loader so we get called at the end of it.
	secondLoaderCalled=0;
	install_irqhandler_fixer();
	install_irqhandler();

//	volatile uint16_t * volatile const uartRegs=(uint16_t *)0x1c00a620;
//	uartRegs[2]='I';
//	uart_putchar(' ');
//	uart_putchar('I');
}

void hack_init_second() {
	//Called at the end of the 2nd loader. Hook the irq routine here.
	//Don't xprintf inhere (or do whatever takes time); the HD will reset if you do.
	install_irqhandler();
//	uart_putchar('S');
	secondLoaderCalled=1;
}

void irqhandler(void) {
//	xdev_out(uart_putchar);
//	xdev_in(uart_getchar);
	if (secondLoaderCalled) {
		if (secondLoaderCalled==1) {
			uart_putchar('X');
			uart_putchar('X');
			uart_putchar('X');
			uart_putchar('X');
			xprintf("K.\n");
			secondLoaderCalled=2;
		}
		install_satareqh_hook();
//		install_satapioh_hook();
		fsck_cache();
	} else {
		install_irqhandler_fixer();
	}
}


