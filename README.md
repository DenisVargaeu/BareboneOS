# miniOS

Jednoduchý operačný systém založený na jadre v jazyku C. Verzia 0.2.

## Novinky v 0.2
- Vylepšená bootovacia obrazovka s animovaným centrovaným loading barom.

## Sťahovanie
Najnovšie binárne súbory nájdete v záložke **Releases** na GitHub stránke tohto projektu:
- `minios.iso`: Bootovateľný ISO obraz.
- `minios.img`: Surový obraz disku.

## Minimálne systémové požiadavky
Pre spustenie miniOS potrebujete:
1.  **Emulátor:** QEMU, VirtualBox alebo VMware.
2.  **Fyzický hardvér:** Počítač s architektúrou x86 a podporou bootovania z CD/USB (pre ISO) alebo diskety (pre IMG).

### Ako spustiť pomocou QEMU
```bash
qemu-system-i386 -cdrom minios.iso
```

## Pre vývojárov
Ak chcete projekt sami kompilovať, potrebujete GCC, Binutils (as, ld) a GRUB.
```bash
make
```
