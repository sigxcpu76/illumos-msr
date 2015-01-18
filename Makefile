CFLAGS=-Wall
KFLAGS=-D_KERNEL -m64 -mcmodel=kernel -mno-red-zone -ffreestanding -nodefaultlibs
LDFLAGS=-lkstat
KLDFLAGS=-r 

all: msr cputemp

msr.o: msr.c
	gcc $(CFLAGS) $(KFLAGS) -c msr.c

msr: msr.o
	ld $(KLDFLAGS) -o msr msr.o

cputemp: cputemp.c
	gcc $(CFLAGS) -o cputemp $(LDFLAGS) cputemp.c

clean: 
	rm msr.o msr cputemp

install: all
	cp msr /kernel/drv/amd64/
	cp msr.conf /kernel/drv/
	rem_drv msr || true
	add_drv msr
	ln -s /devices/pseudo/msr@0:self /dev/cpu/self/msr
