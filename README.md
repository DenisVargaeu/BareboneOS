# miniOS Simulation

Jednoduchá simulácia operačného systému v termináli napísaná v jazyku C.

## Funkcie
- Kernel loop (nekonečná slučka shellu)
- ASCII boot screen s logmi
- Command parser
- Príkazy: `help`, `clear`, `echo`, `time`, `ls`, `exit`
- Farebný "hacker" prompt: `root@miniOS:#`

## Ako to spustiť

### Požiadavky
- GCC compiler
- Make (voliteľné)

### Kompilácia a spustenie pomocou Make
```bash
make
./minios
```

### Kompilácia bez Make
```bash
gcc src/main.c -o minios
./minios
```

## Príkazy v simulácii
- `help`: Zobrazí zoznam príkazov.
- `clear`: Vyčistí obrazovku.
- `echo <text>`: Vypíše zadaný text.
- `time`: Zobrazí aktuálny čas systému.
- `ls`: Simuluje výpis súborov v koreňovom adresári.
- `exit`: Ukončí simuláciu a "vypne" systém.
