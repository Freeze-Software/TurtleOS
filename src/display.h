#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdarg.h>
#include "font.h"

#define FONT_SIZE 8

typedef struct {
    uint32_t* buffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;
} display;

typedef struct {
    int x;
    int y;
} vec2d;

extern display screen;

int strcmp(const char* a, const char* b);

void init_display(uint64_t adress, uint32_t iwidth, uint32_t iheight,
                   uint32_t ipitch, uint8_t ibpp);

void hex_to_rgb(unsigned int color, unsigned int* r, unsigned int* g, unsigned int* b);
unsigned int rgb(unsigned int r, unsigned int g, unsigned int b);
uint32_t argb(uint32_t a, uint32_t r, uint32_t g, uint32_t b, int x, int y);

void draw_pixel(int x, int y, uint32_t color);
void clear_screen();
void draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void draw_line_thick(int x0, int y0, int x1, int y1, uint32_t color, int thickness);
void draw_triangle(vec2d A, vec2d B, vec2d C, uint32_t color);
void draw_rect(int x, int y, int w, int h, uint32_t color);
void draw_rect_outline(int x, int y, int w, int h, uint32_t color);

void draw_char(int x, int y, char c, int size, uint32_t fg, uint32_t bg);
void print(int x, int y, char* string, int size, uint32_t fg, uint32_t bg);

void printf(int x, int y, int size, uint32_t fg, uint32_t bg, const char* fmt, ...);

void get_screen_res(int* width, int* height);

#endif
