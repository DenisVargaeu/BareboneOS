CC = gcc
AS = as
LDFLAGS = -ffreestanding -O2 -nostdlib -m32 -no-pie
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -m32 -fno-pie -fno-stack-protector

all: minios.bin

minios.bin: src/boot.s src/kernel.c linker.ld
	$(AS) --32 src/boot.s -o boot.o
	$(CC) -c src/kernel.c -o kernel.o $(CFLAGS)
	$(CC) -T linker.ld -o minios.bin $(LDFLAGS) boot.o kernel.o

minios.iso: minios.bin grub.cfg
	mkdir -p isodir/boot/grub
	cp minios.bin isodir/boot/minios.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o minios.iso isodir

clean:
	rm -rf *.o minios.bin minios.iso isodir
