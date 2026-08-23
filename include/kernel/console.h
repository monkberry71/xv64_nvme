#pragma once
#include <stdint.h>
#include <kernel/gop.h>
#include <kernel/spin_lock.h>

#define MAX_COLS 256
#define MAX_ROWS 128

struct console {
    char buffer[MAX_ROWS][MAX_COLS];
    int cur_y, cur_x;

    int max_cols, max_rows;

    uint64_t fg, bg;
    
    struct gop_fb *gop_fb;
    uint64_t pixels_per_scanline;

    int font_w, font_h;

    struct spin_lock lk;
    int locking;
};

void console_init(void);
void console_printf(char *fmt, ...);
void console_intr(void);
