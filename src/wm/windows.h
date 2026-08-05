#include <stdint.h>
#include <stdbool.h>

#define MAX_WINDOWS 32

typedef struct window {
    int x, y, w, h;
    int old_x, old_y;
    int z;
    bool focused;
    bool dragging;
    bool titlebar;
    uint32_t* framebuffer;
    int fb_changed;
    char title[64];
} window_t;
