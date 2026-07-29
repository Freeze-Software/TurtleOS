#ifndef KERNEL_H
#define KERNEL_H
#include <stddef.h>
#include <stdint.h>
#include "pci/pci.h"

// ---------------- KERNEL ----------------
void kernel_main(uint32_t magic, uint32_t mb_addr);
void shell();

// ---------------- CONSOLE ----------------
void console_init(uint32_t magic, uint32_t mb_addr);
void console_write(const char *s);
void console_writeln(const char *s);
void console_writef(const char *fmt, ...);
void console_writefln(const char *fmt, ...);
void console_putc(char c);
char console_getc_blocking(void);
void console_clear(void);
void console_clear_line(int line);
void console_clear_character(int x, int y);
void console_backspace(void);
void console_scroll(void);
void render_console(void);
void print_term(char *text);
void set_console_fg_color(uint32_t color);
void set_console_bg_color(uint32_t color);

// ---------------- SERIAL ----------------
void serial_init(void);
void serial_write(const char *s);
void serial_putc(char c);
int serial_received(void);
char serial_read(void);

// ---------------- VGA ----------------
void vga_init(void);
void vga_putc(char c);
void vga_write(const char *s);
void vga_clear(void);

// ---------------- PCI ----------------
static pci_bus_t g_pci_bus;

// ---------------- KEYBOARD ----------------
void keyboard_init(void);
int keyboard_has_char(void);
char keyboard_get_char(void);

// ---------------- PORT I/O ----------------
void io_wait(void);
uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t value);

// ---------------- ATA ----------------
int ata_read_sector(uint32_t lba, void *buffer);
int ata_write_sector(uint32_t lba, const void *buffer);
int ata_zero_sector(uint32_t lba);
uint32_t ata_get_sector_count();

// ---------------- SYSTEM ----------------
void reboot(void);

// ---------------- LOGGING ----------------
void log_init();
void log(char* string, char* name);
void log_print();

#endif
