CFLAGS = -traditional

all: ../output/bootsect.bin ../output/start16.bin ../fs/bootsect.h


../output/bootsect.bin : bootsect.o
	ld86 -0 -s bootsect.o -o bootsect.out
	../tools/bbin bootsect.out ../output/bootsect.bin
	../tools/bsc ../output/bootsect.bin ../fs/bootsect.h

bootsect.o : bootsect.S	
	gcc -E $(CFLAGS) bootsect.S -o bootsect.asm
	as86 -0 -a bootsect.asm -o bootsect.o

../output/start16.bin : start16.o load_files.o check.o tty.o cpuid.o
	ld86 -0 -s start16.o load_files.o check.o tty.o cpuid.o -o start16.out
	../tools/bbin16 start16.out ../output/start16.bin

start16.o : start16.S
	gcc -E $(CFLAGS) -DROOT_DEV=\"$(VMM_ROOT)\" start16.S -o start16.asm
	as86 -0 -a start16.asm -o start16.o

load_files.o : load_files.S
	gcc -E $(CFLAGS) load_files.S -o load_files.asm
	as86 -0 -a load_files.asm -o load_files.o

check.o : check.S
	gcc -E $(CFLAGS) check.S -o check.asm
	as86 -0 -a check.asm -o check.o

cpuid.o : cpuid.S
	gcc -E $(CFLAGS) cpuid.S -o cpuid.asm
	as86 -0 -a cpuid.asm -o cpuid.o

tty.o : tty.S
	gcc -E $(CFLAGS) tty.S -o tty.asm
	as86 -0 -a tty.asm -o tty.o

clean:
	rm -f *.o *.out *asm
