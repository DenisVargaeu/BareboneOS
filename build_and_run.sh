#!/bin/bash
# Absolútne minimálny build skript

# 1. Vyčistenie
rm -f boot.o kernel.o minios.bin

# 2. Kompilácia
as --32 src/boot.s -o boot.o
gcc -m32 -c src/kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -fno-pie -fno-stack-protector

# 3. Linkovanie
ld -m elf_i386 -T linker.ld -o minios.bin boot.o kernel.o

# 4. Verifikácia Multibootu
if grub-file --is-x86-multiboot minios.bin; then
    echo "Multiboot potvrdený! Spúšťam QEMU..."
    qemu-system-i386 -kernel minios.bin
else
    echo "CHYBA: Súbor nie je Multiboot compliant!"
fi
