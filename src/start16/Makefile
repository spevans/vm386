CFLAGS = -traditional

all: ../output/bootsect.bin ../output/start16.bin

../output/bootsect.bin : bootsect.o
	ld86 -0 -s bootsect.o -o bootsect.out
	../tools/bbin bootsect.out ../output/bootsect.bin
	../tools/bsc ../output/bootsect.bin ../fs/bootsect.h

bootsect.o : bootsect.S	
	gcc -E $(CFLAGS) bootsect.S -o bootsect.s
	as86 -0 -a bootsect.s -o bootsect.o

../output/start16.bin : start16.o load_files.o check.o tty.o cpuid.o
	ld86 -0 -s start16.o load_files.o check.o tty.o cpuid.o -o start16.out
	../tools/bbin16 start16.out ../output/start16.bin

start16.o : start16.S
	gcc -E $(CFLAGS) -DROOT_DEV=\"$(VMM_ROOT)\" start16.S -o start16.s 
	as86 -0 -a start16.s -o start16.o

load_files.o : load_files.S
	gcc -E $(CFLAGS) load_files.S -o load_files.s
	as86 -0 -a load_files.s -o load_files.o

check.o : check.S
	gcc -E $(CFLAGS) check.S -o check.s
	as86 -0 -a check.s -o check.o

cpuid.o : cpuid.S
	gcc -E $(CFLAGS) cpuid.S -o cpuid.s
	as86 -0 -a cpuid.s -o cpuid.o

tty.o : tty.S
	gcc -E $(CFLAGS) tty.S -o tty.s
	as86 -0 -a tty.s -o tty.o

clean:
	rm -f *.[os] *.out
