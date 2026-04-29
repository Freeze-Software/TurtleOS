#include "kernel.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)
#define VGA_COLOR 0x0F

static size_t row = 0;
static size_t col = 0;

static void vga_scroll(void) {
    if (row < VGA_HEIGHT) {
        return;
    }

    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }

    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = ((uint16_t)VGA_COLOR << 8) | ' ';
    }

    row = VGA_HEIGHT - 1;
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = ((uint16_t)VGA_COLOR << 8) | ' ';
        }
    }
    row = 0;
    col = 0;
}

void vga_init(void) {
    vga_clear();
}

void vga_putc(char c) {
    if (c == '\n') {
        col = 0;
        row++;
        vga_scroll();
        return;
    }

    if (c == '\r') {
        col = 0;
        return;
    }

    VGA_MEMORY[row * VGA_WIDTH + col] = ((uint16_t)VGA_COLOR << 8) | (uint8_t)c;
    col++;

    if (col >= VGA_WIDTH) {
        col = 0;
        row++;
    }

    vga_scroll();
}

void vga_write(const char *s) {
    while (*s) {
        vga_putc(*s++);
    }
}
