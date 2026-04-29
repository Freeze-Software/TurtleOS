#include "kernel.h"

static const char keymap[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,  '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,
};

void keyboard_init(void) {
}

int keyboard_has_char(void) {
    return inb(0x64) & 1;
}

char keyboard_get_char(void) {
    while (!keyboard_has_char()) {
    }

    uint8_t sc = inb(0x60);
    if (sc & 0x80) {
        return 0;
    }

    if (sc >= sizeof(keymap)) {
        return 0;
    }

    return keymap[sc];
}
