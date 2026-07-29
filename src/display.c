#include "display.h"
#include "libc/main.h"

display screen;

int strcmp(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b)
            return *a - *b;
        a++;
        b++;
    }
    return *a - *b;
}

void init_display(uint64_t adress, uint32_t iwidth, uint32_t iheight, uint32_t ipitch, uint8_t ibpp) {
    screen.buffer = (uint32_t*)(uint32_t)adress;
    screen.width = iwidth;
    screen.height = iheight;
    screen.pitch = ipitch;
    screen.bpp = ibpp;
}

void hex_to_rgb(unsigned int color, unsigned int* r, unsigned int* g, unsigned int* b) {
    *r = (color >> 16) & 0xFF;
    *g = (color >> 8) & 0xFF;
    *b = color & 0xFF;
}

void draw_pixel(int x, int y, uint32_t color) {
    screen.buffer[y * (screen.pitch / 4) + x] = color;
}

unsigned int rgb(unsigned int r, unsigned int g, unsigned int b) {
    return (r << 16) | (g << 8) | b;
}

uint32_t argb(uint32_t a, uint32_t r, uint32_t g, uint32_t b, int x, int y) {
    uint32_t old_pixel = screen.buffer[y * (screen.pitch / 4) + x];
    unsigned int oR, oG, oB;
    hex_to_rgb(old_pixel, &oR, &oG, &oB);
    float alpha = a / 255.0f;
    float inv_alpha = 1.0f - alpha;
    int nR = (int)(oR * inv_alpha + r * alpha);
    int nG = (int)(oG * inv_alpha + g * alpha);
    int nB = (int)(oB * inv_alpha + b * alpha);
    if (nR > 255) nR = 255;
    if (nG > 255) nG = 255;
    if (nB > 255) nB = 255;

    return (nR << 16) | (nG << 8) | nB;
}

void clear_screen() {
    for (int x = 0; x < screen.width; x++) {
        for (int y = 0; y < screen.height; y++) {
            screen.buffer[y * (screen.pitch / 4) + x] = 0x000000;
        }
    }
}

void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? (y0 - y1) : (y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_line_thick(int x0, int y0, int x1, int y1, uint32_t color, int thickness) {
    for (int i = -1; i <= thickness; i++) {
        draw_line(x0, y0 + i, x1, y1 + i, color);
    }
}

void draw_triangle(vec2d A, vec2d B, vec2d C, uint32_t color) {
    draw_line(A.x, A.y, B.x, B.y, color);
    draw_line(B.x, B.y, C.x, C.y, color);
    draw_line(C.x, C.y, A.x, A.y, color);
}

void get_screen_res(int* width, int* height) {
    *width = screen.width;
    *height = screen.height;
}

void draw_char(int x, int y, char c, int size, uint32_t fg, uint32_t bg) {
    for (int row = 0; row < 16; row++) {
        uint8_t line = font_bin[(uint8_t)c * 16 + row];
        for (int col = 0; col < 8; col++) {
            uint32_t pixel_color = (line & (1 << (7 - col))) ? fg : bg;
            for (int dy = 0; dy < size; dy++) {
                for (int dx = 0; dx < size; dx++) {
                    int px = x + col * size + dx;
                    int py = y + row * size + dy;
                    screen.buffer[py * (screen.pitch / 4) + px] = pixel_color;
                }
            }
        }
    }
}

void print(int x, int y, char* string, int size, uint32_t fg, uint32_t bg) {
    for (int i = 0; string[i] != '\0'; i++) {
        draw_char(x + (i * (FONT_SIZE * size)), y, string[i], size, fg, bg);
    }
}

void draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            screen.buffer[(y + dy) * (screen.pitch / 4) + (x + dx)] = color;
        }
    }
}

void draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < w; i++) {
        draw_pixel(x + i, y, color);
        draw_pixel(x + i, y + h - 1, color);
    }

    for (int j = 0; j < h; j++) {
        draw_pixel(x, y + j, color);
        draw_pixel(x + w - 1, y + j, color);
    }
}

void printf(int x, int y, int size, uint32_t fg, uint32_t bg, const char* fmt, ...) {
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
                    draw_char(cx, y, s[j], size, fg, bg);
                    cx += FONT_SIZE * size;
                }
            }
            else if (fmt[i] == 'd') {
                itoa(va_arg(args, int), buffer, 10);
                for (int j = 0; buffer[j] != '\0'; j++) {
                    draw_char(cx, y, buffer[j], size, fg, bg);
                    cx += FONT_SIZE * size;
                }
            }
            else if (fmt[i] == 'x') {
                itoa(va_arg(args, int), buffer, 16);
                for (int j = 0; buffer[j] != '\0'; j++) {
                    draw_char(cx, y, buffer[j], size, fg, bg);
                    cx += FONT_SIZE * size;
                }
            }
            else if (fmt[i] == 'c') {
                char c = (char)va_arg(args, int);
                draw_char(cx, y, c, size, fg, bg);
                cx += FONT_SIZE * size;
            }
            else if (fmt[i] == '%') {
                draw_char(cx, y, '%', size, fg, bg);
                cx += FONT_SIZE * size;
            }
        } else {
            draw_char(cx, y, fmt[i], size, fg, bg);
            cx += FONT_SIZE * size;
        }
    }

    va_end(args);
}
