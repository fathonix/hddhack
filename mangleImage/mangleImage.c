#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>


#define DATAPERSECTOR (512-(3*4))
#define MAGIC_FIRST 0x31586e4c //'LnX1'
#define MAGIC_MORE 0x21586e4c //'LnX!'

struct Sector {
	uint32_t magic;
	uint32_t nonce;
	uint32_t sectorNo;
	char data[DATAPERSECTOR];
};

int main(int argc, char **argv) {
	int in, out;
	struct Sector s;
	if (argc<2) {
		printf("Usage: %s zImage outfile\n", argv[0]);
		exit(1);
	}
	in=open(argv[1], O_RDONLY);
	if (in<0) {
		perror(argv[1]);
		exit(1);
	}
	out=open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (out<0) {
		perror(argv[2]);
		exit(1);
	}
	s.magic=MAGIC_MORE;
	s.nonce=time(NULL);
	s.sectorNo=0;

	while (read(in, s.data, DATAPERSECTOR)!=0) {
		write(out, (char*)&s, 512);
		s.sectorNo++;
	}

	//Write header (footer?)
	s.magic=MAGIC_FIRST;
	write(out, (char*)&s, 512);
	printf("Wrote %i sectors. Nonce = %x\n", s.sectorNo+1, s.nonce);
	exit(0);
}