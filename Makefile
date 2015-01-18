CFLAGS=-Wall
KFLAGS=-D_KERNEL -m64 -mcmodel=kernel -mno-red-zone -ffreestanding -nodefaultlibs
LDFLAGS=-lkstat
KLDFLAGS=-r 

all: msr coretemp

msr.o: msr.c
	gcc $(CFLAGS) $(KFLAGS) -c msr.c

msr: msr.o
	ld $(KLDFLAGS) -o msr msr.o

coretemp: coretemp.c
	gcc $(CFLAGS) -o coretemp $(LDFLAGS) coretemp.c

clean: 
	rm msr.o msr coretemp

install: all
	cp msr /kernel/drv/amd64/
	cp msr.conf /kernel/drv/
	rem_drv msr || true
	add_drv msr
	ln -s /devices/pseudo/msr@0:self /dev/cpu/self/msr
