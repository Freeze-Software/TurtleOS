#include <stdint.h>
#include "mouse.h"
#include "render.h"
#include "../display.h"

extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t val);

ps2mouse_state_t mouse;

uint8_t mouse_cycle = 0;
uint8_t mouse_packet[3];

static void ps2mouse_wait_input() {
    while (inb(0x64) & 2);
}

static void ps2mouse_wait_output() {
    while (!(inb(0x64) & 1));
}


void mouse_irq_handler() {
    if (!(inb(0x64) & 0x20))
        return;

    uint8_t data = inb(0x60);

    if (mouse_cycle == 0 && !(data & 0x08))
        return;

    mouse_packet[mouse_cycle++] = data;
    if (mouse_cycle < 3)
        return;
    mouse_cycle = 0;

    int dx = (int8_t)mouse_packet[1];
    int dy = (int8_t)mouse_packet[2];
    mouse.last_x = mouse.x;
    mouse.last_y = mouse.y;
    mouse.x += dx;
    mouse.y -= dy;
    mouse.left   = mouse_packet[0] & 1;
    mouse.right  = mouse_packet[0] & 2;
    mouse.middle = mouse_packet[0] & 4;
    //update_cursor(mouse.x + dx, mouse.y - dy);
}

static void ps2mouse_write(uint8_t data) {
    ps2mouse_wait_input();
    outb(0x64, 0xD4);
    ps2mouse_wait_input();
    outb(0x60, data);
}

static uint8_t ps2mouse_read() {
    ps2mouse_wait_output();
    return inb(0x60);
}

void ps2mouse_init() {
    ps2mouse_wait_input();
    outb(0x64, 0xAD); // disable keyboard
    ps2mouse_wait_input();
    outb(0x64, 0xA7); // disable mouse
    while (inb(0x64) & 1)
        inb(0x60);

    ps2mouse_wait_input();
    outb(0x64, 0x20);
    ps2mouse_wait_output();
    uint8_t config = inb(0x60);
    config |=  (1 << 0); // enable keyboard IRQ
    config |=  (1 << 1); // enable mouse IRQ
    config &= ~(1 << 4); // enable keyboard clock
    config &= ~(1 << 5); // enable mouse clock
    ps2mouse_wait_input();
    outb(0x64, 0x60);
    ps2mouse_wait_input();
    outb(0x60, config);
    ps2mouse_wait_input();
    outb(0x64, 0xA8);
    ps2mouse_write(0xFF);
    ps2mouse_read();
    ps2mouse_read();
    ps2mouse_read();
    ps2mouse_write(0xF6);
    ps2mouse_read(); // ACK
    ps2mouse_write(0xF4);
    ps2mouse_read(); // ACK
    ps2mouse_wait_input();
    outb(0x64, 0xAE);
    mouse.x = 0;
    mouse.y = 0;
    update_cursor(mouse.x, mouse.y);
}
