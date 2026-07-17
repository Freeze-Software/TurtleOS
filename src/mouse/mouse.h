#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stdbool.h>

extern uint8_t mouse_cycle;

typedef struct {
    int x;
    int y;
    int last_x;
    int last_y;
    uint8_t left;
    uint8_t right;
    uint8_t middle;
} ps2mouse_state_t;

extern ps2mouse_state_t mouse;

void ps2mouse_init(void);
void mouse_irq_handler();

#endif
