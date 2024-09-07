OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(main)
SECTIONS
{
	. = 0xffe3f000;
	. = ALIGN(4);
	.text     :
	{
	  hack.o(.text)
	}

	. = ALIGN(4);
	.rodata : { *(.rodata) }

	. = ALIGN(4);
	.data : { *(.data) }

	. = ALIGN(4);
	.got : { *(.got) }


	. = ALIGN(4);
	__bss_start = .;
	.bss : { *(.bss) }
	__bss_end__ = .;

	_end = .;

}
