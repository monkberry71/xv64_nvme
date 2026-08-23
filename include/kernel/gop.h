#pragma once
#include <stdint.h>
#define GOP_RED 0x00FF0000
#define GOP_GRN 0x0000FF00
#define GOP_BLU 0x000000FF
#define GOP_BLK 0x00000000
#define GOP_WHI 0x00FFFFFF

struct gop_fb {
    void* fb_base;
    uint64_t w,h,pitch;
    uint8_t bpp, type;
};

int gop_init(void);
void gop_draw_pixel(uint64_t x, uint64_t y, uint64_t col);
void gop_draw_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint64_t color);