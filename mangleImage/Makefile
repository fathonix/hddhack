
kernel.sect: mangleImage linux-3.9.4/ dummy
	make -C linux-3.9.4 -j 5
	./mangleImage linux-3.9.4/arch/arm/boot/zImage kernel.sect

test: kernel.sect
	sudo mount /dev/sda1 /mnt -o sync
	echo 'HD, live' > /mnt/jeroen/bla
	cat kernel.sect > /mnt/jeroen/kerne.sect
	sync; sync; sync;
	echo 'HD, live' > /mnt/jeroen/bla
	echo 'HD, lnx!' > /mnt/jeroen/bla || true
	sudo umount /mnt

mangleImage: mangleImage.o
	$(CC) -o mangleImage mangleImage.o

dummy: