# TurtleOS
## Everything

- `src/boot.s`
- `src/linker.ld`
- `src/kernel.c`
- `src/console.c`
- `src/vga.c`
- `src/serial.c`
- `src/keyboard.c`
- `grub/grub.cfg`
- `Makefile`

## Emulators

```bash
sudo apt update
sudo apt install -y build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86
```

## instructions
run:
```bash
make clean
make all
make run
```
