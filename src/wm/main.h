#include <stdint.h>

extern uint32_t wm_background_color;
int create_window(char* name, int width, int height, int titlebar);
int window_draw_rect(int window, int x, int y, int w, int h, uint32_t color);
int window_draw_pixel(int window, int x, int y, uint32_t color);
int window_draw_char(int window, int x, int y, char c, int size, uint32_t fg, uint32_t bg);
int window_printf(int window, int x, int y, int size, uint32_t fg, uint32_t bg, const char* fmt, ...);
int window_pixel_chessboard(int window);
void test_window();
void wm_render();
