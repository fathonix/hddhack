CC=arm-linux-gnueabi-gcc
OBJCOPY=arm-linux-gnueabi-objcopy
ASFLAGS=-mthumb-interwork -march=armv5te
CFLAGS=-mthumb-interwork -march=armv5te -O3 -fdata-sections -ffunction-sections
LDFLAGS=-L/usr/lib/gcc/arm-linux-gnueabi/4.3.5/ -lgcc --gc-sections
TARGETS=hack.bin
LD=arm-linux-gnueabi-ld
OBJS= hack.o hackcode.o  util.o serial.o xprintf.o linux.o modify.o mbr.o
MBRFILE=../../grub-1.99/grub-core/boot.img

all: $(TARGETS)

flash: all
	{ cd ..; ./flash.sh; }

clean:
	rm -f *.o $(TARGETS)

firm.elf: $(OBJS)
	$(LD) -N -T link.lds $(OBJS) $(LDFLAGS) -o firm.elf

mbr.o:
	dd if=$(MBRFILE) of=mbr.bin bs=446 count=1
	$(OBJCOPY) -B arm -I binary -O elf32-littlearm mbr.bin mbr.o

hack.bin: firm.elf
	$(OBJCOPY) firm.elf --output-target binary  hack.bin
