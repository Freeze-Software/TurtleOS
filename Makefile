CC := gcc
AS := gcc
LD := ld

CFLAGS := -m32 -ffreestanding -O2 -Wall -Wextra -fno-pic -fno-stack-protector -fno-builtin
ASFLAGS := -m32 -c
LDFLAGS := -m elf_i386 -T src/linker.ld -nostdlib

SRC_C := $(wildcard src/*.c)
SRC_S := $(wildcard src/*.s)
OBJ := $(SRC_C:.c=.o) $(SRC_S:.s=.o)

KERNEL := build/kernel.elf
ISO_DIR := build/isofiles
ISO := build/kernel.iso

.PHONY: all clean run run-nographic iso

all: $(KERNEL)

$(KERNEL): $(OBJ)
	@mkdir -p build
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.s
	$(AS) $(ASFLAGS) $< -o $@

iso: $(KERNEL)
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR) >/dev/null 2>&1

run: iso
	@if [ -n "$$DISPLAY" ] || [ -n "$$WAYLAND_DISPLAY" ]; then \
		qemu-system-i386 -cdrom $(ISO); \
	else \
		echo "No GUI display detected, falling back to nographic mode."; \
		qemu-system-i386 -cdrom $(ISO) -nographic -serial mon:stdio; \
	fi

run-nographic: iso
	qemu-system-i386 -cdrom $(ISO) -nographic -serial mon:stdio

clean:
	rm -rf build src/*.o
