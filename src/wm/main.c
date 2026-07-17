// ---- includes ----

#include "windows.h"
#include "../libc/main.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "../memory/main.h"
#include "../mouse/mouse.h"
#include "../mouse/render.h"
#include "../keyboard/keyboard.h"
#include "../multitasking/task.h"
#include "../display.h"
#include "../pit/pit.h"

window_t windows[MAX_WINDOWS];
int window_count = 0;
int highest_z = 0;
static int last_mouse_left = 0;
uint32_t wm_background_color;
uint32_t* backbuffer;
uint32_t* framebuffer;
#define TASKBAR_HEIGHT 28

// ---- backbuffer primitives ----

void bf_draw_pixel(int x, int y, uint32_t c) {
    if (x < 0 || y < 0 || x >= screen.width || y >= screen.height) return;
    backbuffer[screen.width * y + x] = c;
}

void bf_draw_rect(int x, int y, int w, int h, uint32_t c) {
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            bf_draw_pixel(x + xx, y + yy, c);
        }
    }
}

void clear(uint32_t* buf, uint32_t color) {
    int size = screen.width * screen.height;
    for (int i = 0; i < size; i++) {
        buf[i] = color;
    }
}
// Window rendering API
int window_draw_pixel(int window, int x, int y, uint32_t color) {
    if (window <= 0 || window > window_count)
        return -1;
    window_t* w = &windows[window];
    w->framebuffer[y * w->x + x] = color;
    return 0;
}

int window_draw_rect(int window, int x, int y, int w, int h, uint32_t color) {
	if (window <= 0 || window > window_count)
		return -1;

	window_t* w2 = &windows[window];

	for (int yy = y; yy < (y + h); yy++) {
		for (int xx = x; xx < (x + w); xx++) {
			w2->framebuffer[yy * w2->x + xx] = color;	
		}
	}
	return 0;
}

int window_draw_char(int window, int x, int y, char c, int size, uint32_t fg, uint32_t bg) {
    if (window <- 0 || window > window_count)
	    return -1;
    window_t* w = &windows[window];
    for (int row = 0; row < 16; row++) {
        uint8_t line = font_bin[(uint8_t)c * 16 + row];
        for (int col = 0; col < 8; col++) {
            uint32_t pixel_color = (line & (1 << (7 - col))) ? fg : bg;
            for (int dy = 0; dy < size; dy++) {
                for (int dx = 0; dx < size; dx++) {
                    int px = x + col * size + dx;
                    int py = y + row * size + dy;
                    w->framebuffer[py * w->x + px] = pixel_color;
                }
            }
        }
    }
    return 0;
}

int window_printf(int window, int x, int y, int size, uint32_t fg, uint32_t bg, const char* fmt, ...) {
    if (window <= 0 || window > window_count)
	    return -1;
    va_list args;
    va_start(args, fmt);
    char buffer[32];
    int cx = x;

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%') {
            i++;

            if (fmt[i] == 's') {
                char* s = va_arg(args, char*);
                for (int j = 0; s[j] != '\0'; j++) {
                    window_draw_char(window, cx, y, s[j], size, fg, bg);
                    cx += FONT_SIZE * size;
                }
            }
            else if (fmt[i] == 'd') {
                itoa(va_arg(args, int), buffer, 10);
                for (int j = 0; buffer[j] != '\0'; j++) {
                    window_draw_char(window, cx, y, buffer[j], size, fg, bg);
                    cx += FONT_SIZE * size;
                }
            }
            else if (fmt[i] == 'x') {
                itoa(va_arg(args, int), buffer, 16);
                for (int j = 0; buffer[j] != '\0'; j++) {
                    window_draw_char(window, cx, y, buffer[j], size, fg, bg);
                    cx += FONT_SIZE * size;
                }
            }
            else if (fmt[i] == 'c') {
                char c = (char)va_arg(args, int);
                window_draw_char(window, cx, y, c, size, fg, bg);
                cx += FONT_SIZE * size;
            }
            else if (fmt[i] == '%') {
                window_draw_char(window, cx, y, '%', size, fg, bg);
                cx += FONT_SIZE * size;
            }
        } else {
            window_draw_char(window, cx, y, fmt[i], size, fg, bg);
            cx += FONT_SIZE * size;
        }
    }

    va_end(args);
    return 0;
}

// ---- Checks ----
int window_pixel_chessboard(int window) {
    if (window < 0 || window >= window_count) return -1;

    window_t* w = &windows[window];

    for (int y = 0; y < w->h; y++) {
        for (int x = 0; x < w->w; x++) {

            uint32_t color = ((x + y) & 1) ? 0x000000 : 0xFFFFFF;

            w->framebuffer[y * w->w + x] = color;
        }
    }

    return 0;
}

// ---- text rendering ----

void bf_draw_char(int x, int y, char c, int size, uint32_t fg, uint32_t bg) {
    for (int row = 0; row < 16; row++) {
        uint8_t line = font_bin[(uint8_t)c * 16 + row];

        for (int col = 0; col < 8; col++) {
            uint32_t color = (line & (1 << (7 - col))) ? fg : bg;

            for (int dy = 0; dy < size; dy++) {
                for (int dx = 0; dx < size; dx++) {
                    bf_draw_pixel(
                        x + col * size + dx,
                        y + row * size + dy,
                        color
                    );
                }
            }
        }
    }
}

void bf_printf(int x, int y, int size, uint32_t fg, uint32_t bg, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char buffer[64];
    int cx = x;

    for (int i = 0; fmt[i] != '\0'; i++) {

        if (fmt[i] == '%') {
            i++;

            if (fmt[i] == 's') {
                char* s = va_arg(args, char*);

                if (!s) s = "(null)";

                for (int j = 0; s[j] != '\0'; j++) {
                    bf_draw_char(cx, y, s[j], size, fg, bg);
                    cx += 8 * size;
                }
            }

            else if (fmt[i] == 'd') {
                itoa(va_arg(args, int), buffer, 10);

                for (int j = 0; buffer[j] != '\0'; j++) {
                    bf_draw_char(cx, y, buffer[j], size, fg, bg);
                    cx += 8 * size;
                }
            }

            else if (fmt[i] == 'x') {
                itoa(va_arg(args, int), buffer, 16);

                for (int j = 0; buffer[j] != '\0'; j++) {
                    bf_draw_char(cx, y, buffer[j], size, fg, bg);
                    cx += 8 * size;
                }
            }

            else if (fmt[i] == 'c') {
                char c = (char)va_arg(args, int);
                bf_draw_char(cx, y, c, size, fg, bg);
                cx += 8 * size;
            }

            else if (fmt[i] == '%') {
                bf_draw_char(cx, y, '%', size, fg, bg);
                cx += 8 * size;
            }
        }

        else {
            bf_draw_char(cx, y, fmt[i], size, fg, bg);
            cx += 8 * size;
        }
    }

    va_end(args);
}


// ---- input drag ----

static int drag_window = -1;
static int last_mouse_x = 0;
static int last_mouse_y = 0;

// ---- z-order helper ----

void bring_window_to_front(int id) {
    if (windows[id].z == highest_z) return;
    if (id < 0 || id >= window_count) return;

    highest_z++;
    windows[id].z = highest_z;
    drag_window = -1;
}

// ---- window clear ----
int clear_window(int window) {
	if (window <= 0 || window > window_count)
		return -1;

	window_t* w = &windows[window];
	for (int x = 0; x < w->x; x++) {
		for (int y = 0; y < w->y; y++) {
			w->framebuffer[y * w->w + x] = 0x000000;
		}
	}
	return 0;
}


// ---- window creation ----

int create_window(char* name, int width, int height) {
    if (window_count >= MAX_WINDOWS) return -1;

    window_t* w = &windows[window_count];

    w->framebuffer = malloc(width * height * sizeof(uint32_t));
    if (!w->framebuffer) return -1;

    strncpy(w->title, name, sizeof(w->title) - 1);
    w->title[sizeof(w->title) - 1] = '\0';

    w->w = width;
    w->h = height;

    int sw, sh;
    get_screen_res(&sw, &sh);

    w->x = sw / 2 - width / 2;
    w->y = sh / 2 - height / 2;

    highest_z++;
    w->z = highest_z;

    window_count++;
    //clear_window((window_count - 1));
    return window_count - 1;
}

// ---- window rendering ----

void draw_window(int id) {
    window_t* w = &windows[id];

    int x0 = w->x;
    int y0 = w->y;

    // ---- titlebar ----
    bf_draw_rect(x0, y0 - 20, w->w, 20, 0x444444);

    // ---- title text ----
    bf_printf(x0 + 4, y0 - 16, 1, 0xFFFFFF, 0x444444, w->title);

    // ---- window content ----
    for (int y = 0; y < w->h; y++) {
        for (int x = 0; x < w->w; x++) {
            bf_draw_pixel(
                x0 + x,
                y0 + y,
                w->framebuffer[y * w->w + x]
            );
        }
    }
}

// ---- taskbar ----
void draw_taskbar() {
    int sw, sh;
    get_screen_res(&sw, &sh);

    int y = sh - TASKBAR_HEIGHT;

    // ---- background ----
    bf_draw_rect(0, y, sw, TASKBAR_HEIGHT, 0x222222);

    // ---- top border ----
    bf_draw_rect(0, y, sw, 1, 0x555555);

    int offset_x = 4;

    for (int i = 0; i < window_count; i++) {
        window_t* w = &windows[i];

        int btn_w = 90;
        int btn_h = 20;
        int btn_x = offset_x;
        int btn_y = y + 4;

        if (btn_x + btn_w > sw)
            break;

        // ---- hover ----
        int hovered =
            mouse.x >= btn_x &&
            mouse.x < btn_x + btn_w &&
            mouse.y >= btn_y &&
            mouse.y < btn_y + btn_h;

        uint32_t color = hovered ? 0x555555 : 0x444444;

        // ---- active window ----
        if (w->z == highest_z)
            color = 0x666666;

        // ---- button ----
        bf_draw_rect(
            btn_x,
            btn_y,
            btn_w,
            btn_h,
            color
        );

        // ---- title ----
        bf_printf(
            btn_x + 4,
            btn_y + 4,
            1,
            0xFFFFFF,
            color,
            w->title
        );

        // ---- click ---
	int clicked = mouse.left && !last_mouse_left;
        if (hovered && clicked) {
            bring_window_to_front(i);
        }

        offset_x += btn_w + 4;
    }
}

// ---- cursor ----

// ---- cursor shape ----

static const uint16_t cursor_shape[16] = {
    0b1000000000000000,
    0b1100000000000000,
    0b1110000000000000,
    0b1111000000000000,
    0b1111100000000000,
    0b1111110000000000,
    0b1111111000000000,
    0b1111111100000000,
    0b1111111110000000,
    0b1111111111000000,
    0b1111100000000000,
    0b1101100000000000,
    0b1000110000000000,
    0b0000110000000000,
    0b0000011000000000,
    0b0000011000000000,
};

// ---- cursor draw ----

void bf_draw_cursor(int mx, int my) {
    for (int y = 0; y < 16; y++) {
        uint16_t row = cursor_shape[y];

        for (int x = 0; x < 16; x++) {
            if (row & (1 << (15 - x))) {
                bf_draw_pixel(mx + x, my + y, 0xFFFFFF);
            }
        }
    }
}

// ---- drag update ----

void wm_update_drag(int mouse_btn) {
    if (!mouse_btn) {
        drag_window = -1;
        return;
    }

    if (drag_window != -1) {
        bring_window_to_front(drag_window);

        if (drag_window < 0 || drag_window >= window_count) {
            drag_window = -1;
            return;
        }

        window_t* w = &windows[drag_window];
        int dx = mouse.x - last_mouse_x;
        int dy = mouse.y - last_mouse_y;
        w->x += dx;
        w->y += dy;
    }

    else {
        for (int i = window_count - 1; i >= 0; i--) {

            window_t* w = &windows[i];

            if (mouse.x >= w->x &&
                mouse.x < w->x + w->w &&
                mouse.y >= w->y - 20 &&
                mouse.y < w->y) {

                drag_window = i;
                break;
            }
        }
    }

    last_mouse_x = mouse.x;
    last_mouse_y = mouse.y;
}

// ---- framebuffer flip ----

void screen_flip() {
    int size = screen.width * screen.height * sizeof(uint32_t);
    memcpy(framebuffer, backbuffer, size);
}

// ---- draw all windows sort by z ----

void draw_all_windows() {
    for (int i = 0; i < window_count; i++) {
        for (int j = i + 1; j < window_count; j++) {

            if (windows[j].z < windows[i].z) {
                window_t tmp = windows[i];
                windows[i] = windows[j];
                windows[j] = tmp;
            }
        }
    }

    for (int i = 0; i < window_count; i++) {
        draw_window(i);
    }
}

// test window
int test_window_id;

void test_window_task() {
	while (1) {
		int x = (100 - (8 / 2));
		int y = (100 - (16 / 2));
		char c = getchar();
		window_printf(test_window_id, x, y, 1, 0xFFFFFF, 0x000000, "%s", c);
	}
}

void test_window() {
	test_window_id = create_window("Key", 100, 100);
	add_task(test_window_task, "Test Window Task", 4096);
}


// ---- render loop ----

int wm_running;

void wm_render() {
    uint32_t last_tick = 0;
    const uint32_t TICKS_PER_FRAME = 2;
    disable_task(0);
    clear_screen();
    int sw, sh;
    get_screen_res(&sw, &sh);
    backbuffer = malloc(sw * sh * sizeof(uint32_t));
    framebuffer = (uint32_t*)screen.buffer;
    wm_running = 1;
    //test_window();

    while (wm_running) {
        if (pit_ticks - last_tick >= TICKS_PER_FRAME) {
            last_tick = pit_ticks;
            clear(backbuffer, wm_background_color);
            draw_all_windows();
	    draw_taskbar();
            wm_update_drag(mouse.left);
            bf_draw_cursor(mouse.x, mouse.y);
            screen_flip();
	    last_mouse_left = mouse.left;
        }

        __asm__ volatile("hlt");
    }
    free(backbuffer);
    enable_task(0);
    clear_screen();
}
