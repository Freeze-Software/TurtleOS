#include "kernel.h"
#include "display.h"
#include "keyboard/keyboard.h"
#include "memory/main.h"
#include <stdint.h>
#include <stdarg.h>
#include "libc/main.h"

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
};

typedef struct {
    char character;
    uint32_t fg;
    uint32_t bg;
} term_char_t;

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 16

int cols = 0;
int rows = 0;
int cursor_x = 0;
int cursor_y = 0;
static int start_row = 0;
uint8_t *dirty_rows = NULL;
uint32_t console_fg_color = 0xFFFFFF;
uint32_t console_bg_color = 0x000000;
term_char_t* console = NULL;

static inline int phys_row(int logical_row) {
    return (start_row + logical_row) % rows;
}

static inline int idx(int y, int x) {
    return phys_row(y) * cols + x;
}

void console_clear(void) {
    cursor_x = 0;
    cursor_y = 0;
    start_row = 0;

    for (int i = 0; i < rows * cols; i++) {
        console[i].character = ' ';
        console[i].fg = console_fg_color;
        console[i].bg = console_bg_color;
    }

    for (int i = 0; i < rows; i++) {
        dirty_rows[i] = 1;
    }

    render_console();
}

void console_scroll(void) {
    if (!console || !dirty_rows || rows <= 1 || cols <= 0) return;

    start_row = (start_row + 1) % rows;

    int new_row = phys_row(rows - 1);

    for (int x = 0; x < cols; x++) {
        int i = new_row * cols + x;
        console[i].character = ' ';
        console[i].fg = console_fg_color;
        console[i].bg = console_bg_color;
    }

    for (int i = 0; i < rows; i++) {
        dirty_rows[i] = 1;
    }

    render_console();
}

void render_console(void) {
    if (!console || !dirty_rows || rows <= 0 || cols <= 0) return;

    for (int y = 0; y < rows; y++) {

        if (dirty_rows[y] == 0) continue;

        int row = phys_row(y);

        for (int x = 0; x < cols; x++) {
            term_char_t cell = console[row * cols + x];

            int px = x * CHAR_WIDTH;
            int py = y * CHAR_HEIGHT;

            draw_rect(px, py, CHAR_WIDTH, CHAR_HEIGHT, cell.bg);

            if (cell.character != 0 && cell.character != ' ') {
                draw_char(px, py, cell.character, 1, cell.fg, cell.bg);
            }
        }

        dirty_rows[y] = 0;
    }
}

void console_backspace(void) {
    if (cursor_x == 0 && cursor_y == 0) return;

    if (cursor_x == 0) {
        if (cursor_y > 0) {
            cursor_y--;
            cursor_x = cols - 1;
        }
    } else {
        cursor_x--;
    }

    if (cursor_y < 0) cursor_y = 0;

    int index = idx(cursor_y, cursor_x);

    console[index].character = ' ';
    console[index].fg = console_fg_color;
    console[index].bg = console_bg_color;

    dirty_rows[cursor_y] = 1;

    render_console();
}

void print_term(char* text) {
    if (!console || !dirty_rows || rows == 0 || cols == 0) return;

    for (int i = 0; text[i] != '\0'; i++) {

        if (text[i] == '\n') {
            cursor_x = 0;
            cursor_y++;

            if (cursor_y >= rows) {
                console_scroll();
                cursor_y = rows - 1;
            }
            continue;
        }

        if (cursor_x >= cols) {
            cursor_x = 0;
            cursor_y++;
        }

        if (cursor_y >= rows) {
            console_scroll();
            cursor_y = rows - 1;
        }

        int index = idx(cursor_y, cursor_x);

        console[index].character = text[i];
        console[index].fg = console_fg_color;
        console[index].bg = console_bg_color;

        dirty_rows[cursor_y] = 1;

        cursor_x++;
    }

    render_console();
}

void console_init(uint32_t magic, uint32_t mb_addr) {
    if (magic != 0x36d76289) return;

    struct multiboot_tag* tag = (struct multiboot_tag*)(mb_addr + 8);

    while (tag->type != 0) {
        if (tag->type == 8) {
            struct multiboot_tag_framebuffer* fb =
                (struct multiboot_tag_framebuffer*)tag;

            init_display(
                fb->framebuffer_addr,
                fb->framebuffer_width,
                fb->framebuffer_height,
                fb->framebuffer_pitch,
                fb->framebuffer_bpp
            );
        }

        tag = (struct multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7));
    }

    int width, height;
    get_screen_res(&width, &height);

    rows = height / CHAR_HEIGHT;
    cols = width / CHAR_WIDTH;

    if (rows <= 2 || cols <= 10) {
        printf(0, 0, 1, 0xFFFFFF, 0x000000, "Screen too small!");
        return;
    }

    console = malloc(rows * cols * sizeof(term_char_t));
    dirty_rows = malloc(rows * sizeof(uint8_t));

    if (!console || !dirty_rows) {
        printf(0, 0, 1, 0xFFFFFF, 0x000000, "Memory allocation failed!");
        return;
    }

    console_clear();
    serial_init();

    console_writeln("Console is ready!");
}

void set_console_fg_color(uint32_t color) {
    console_fg_color = color;
}

void set_console_bg_color(uint32_t color) {
    console_bg_color = color;
}

void console_putc(char c) {
    char s[2] = {c, '\0'};
    print_term(s);
}

void console_write(const char *s) {
    print_term((char*)s);
    serial_write(s);
}

void console_writeln(const char *s) {
    console_write(s);
    console_putc('\n');
}

void console_writef(const char *fmt, ...) {
    char out[1024];

    va_list args;
    va_start(args, fmt);
    vsprintf(out, fmt, args);
    va_end(args);

    console_write(out);
}

void console_writefln(const char *fmt, ...) {
    char out[1024];

    va_list args;
    va_start(args, fmt);
    int len = vsprintf(out, fmt, args);
    va_end(args);

    out[len++] = '\n';
    out[len] = '\0';

    console_write(out);
}

char console_getc_blocking(void) {
    for (;;) {
        if (serial_received()) return serial_read();
        if (kb_available()) {
            char c = getchar();
            if (c) return c;
        }
    }
}
