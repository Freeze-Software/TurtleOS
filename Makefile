CC := gcc
AS := nasm
LD := ld

CFLAGS := -m32 -ffreestanding -O2 -Wall -Wextra -fno-pic -fno-stack-protector -fno-builtin -mno-sse -mno-sse2 -mno-mmx -mno-avx
ASFLAGS := -f elf32
LDFLAGS := -m elf_i386 -T src/linker.ld -nostdlib

SRC_C := $(shell find src -name "*.c")
SRC_S := $(shell find src -name "*.asm")
OBJ := $(SRC_C:.c=.o) $(SRC_S:.asm=.o)

KERNEL := build/kernel.elf
ISO_DIR := build/isofiles
ISO := build/kernel.iso
DATA_DISK := build/turtleos-data.img

.PHONY: all clean run run-nographic iso disk

all: $(KERNEL)

$(KERNEL): $(OBJ)
	@mkdir -p build
	$(LD) $(LDFLAGS) -o $@ src/boot.o $(filter-out src/boot.o,$(OBJ))

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.asm
	$(AS) $(ASFLAGS) $< -o $@

iso: $(KERNEL)
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR) >/dev/null 2>&1

disk:
	@mkdir -p build
	@if [ ! -f $(DATA_DISK) ]; then \
		dd if=/dev/zero of=$(DATA_DISK) bs=1M count=4 >/dev/null 2>&1; \
	fi

run: iso disk
	@if [ -n "$$DISPLAY" ] || [ -n "$$WAYLAND_DISPLAY" ]; then \
		qemu-system-i386 -cdrom $(ISO) -drive file=$(DATA_DISK),format=raw,if=ide,index=0,media=disk -m 512M; \
	else \
		echo "No GUI display detected, falling back to nographic mode."; \
		qemu-system-i386 -cdrom $(ISO) -drive file=$(DATA_DISK),format=raw,if=ide,index=0,media=disk -nographic -serial mon:stdio -m 512M; \
	fi

run-nographic: iso disk
	qemu-system-i386 -cdrom $(ISO) -drive file=$(DATA_DISK),format=raw,if=ide,index=0,media=disk -nographic -serial mon:stdio -m 512M

clean:
	rm -rf build
	find src -name "*.o" -delete 
