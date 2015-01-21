CFLAGS=-Wall -Wno-missing-braces 
KFLAGS=-D_KERNEL -m64 -mcmodel=kernel -mno-red-zone -ffreestanding -nodefaultlibs
KLDFLAGS=-r 

all: coretemp

coretemp.o: coretemp.c
	gcc $(CFLAGS) $(KFLAGS) -c coretemp.c

coretemp: coretemp.o
	ld $(KLDFLAGS) -o coretemp coretemp.o

clean: 
	rm -f coretemp

install: all
	cp coretemp /kernel/drv/amd64/
	cp coretemp.conf /kernel/drv/
	rem_drv coretemp || true
	add_drv -m '* 0660 root sys' coretemp
