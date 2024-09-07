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
.syntax unified
//.THUMB_FUNC

.global hack_init
.global hack_init_second
.global hack_start
.global irqcode
.global irqhandler
.global second_loader_hook
.global hooksatareqh
.global hooksatareqh_handler
.global hooksatapioh
.global hooksatapioh_handler
.global hooksataw
.global hooksataw_handler
.global orig_irq_vector
.global __bss_start             // -> .bss area in RAM
.global __bss_end__             // end of .bss area
//to shut up linker
.global main
main:

hack_start:
	stmfd sp!, {r0-r12, lr}
	//Clear bss
	mov   r0,#0                     // get a zero
	ldr   r1,=__bss_start           // -> bss start
	ldr   r2,=__bss_end__           // -> bss end
bsslp:
	cmp   r1,r2                     // check if data to clear
	strlo r0,[r1],#4                // clear 4 bytes
	blo   bsslp                        // loop until done

	blx hack_init
	ldmfd sp!, {r0-r12, lr}
	mov pc,#0

second_loader_hook:
	stmfd sp!, {r0-r12, lr}
	blx hack_init_second
	ldmfd sp!, {r0-r12, lr}
	//Replicate original code that was replaced by the hook and jump back.
	mov r3,#0
	mov r4,#0
	mov r5,#0
	ldr pc,=0xffe5d37C

irqcode:
	stmfd sp!, {r0-r12, lr}
	blx irqhandler
	ldmfd sp!, {r0-r12, lr}
	ldr pc,orig_irq_vector
orig_irq_vector:
	.word 0


hooksatareqh:
	stmfd sp!, {r0-r12, lr}
	blx hooksatareqh_handler
	ldmfd sp!, {r0-r12, lr}
	lsls r1,r0,#4
	ldr r0,=0x4002b1c
	sub sp,sp,#0x14
	adds r6,r1,r0
	ldrb r1,[r6,#0xd]

	ldr pc,=0x167cc+1



hooksatapioh:
	stmfd sp!, {r0-r12, lr}
	blx hooksatapioh_handler
	ldmfd sp!, {r0-r12, lr}

	movs r6,r0
	movs r0,#0
	sub sp,sp,#0xc
	str r0,[sp, #8]
	movs r0,#0xfd

	ldr pc,=0x440c+1

	
/*
	mov r6,r0
	ldr r0,=0x280ebffc
	movs r4,r1
	str r1, [r0, 0x44]
	adds r0,#0x60

	ldr pc,=0xffe64ea4+1


*/