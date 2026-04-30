#ifndef KERNEL_H
#define KERNEL_H

#include <stddef.h>
#include <stdint.h>

void kernel_main(void);

void console_init(void);
void console_write(const char *s);
void console_writeln(const char *s);
void console_clear(void);
void console_putc(char c);
char console_getc_blocking(void);

void serial_init(void);
void serial_write(const char *s);
void serial_putc(char c);
int serial_received(void);
char serial_read(void);

void vga_init(void);
void vga_putc(char c);
void vga_write(const char *s);
void vga_clear(void);

void keyboard_init(void);
int keyboard_has_char(void);
char keyboard_get_char(void);

void io_wait(void);
uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t value);

int ata_read_sector(uint32_t lba, void *buffer);
int ata_write_sector(uint32_t lba, const void *buffer);

void reboot(void);

#endif
