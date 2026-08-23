#include <stdint.h>
#include <kernel/console.h>
#include <kernel/font8x16.h>
#include <kernel/gop.h>
#include <kernel/spin_lock.h>
#include <kernel/file.h>
#include <stdarg.h>
#include <kernel/helper.h>

struct console cons;

static void font_draw_char(uint64_t x, uint64_t y, char c, uint64_t fg, uint64_t bg) {
    if(c < FONT_FIRST || c > FONT_LAST) c = '?';
    const uint8_t *glyph = font8x16[c - FONT_FIRST];
    for(int row = 0; row < FONT_CHAR_H; row++) {
        uint8_t bits = glyph[row];
        for(int col = 0; col < FONT_CHAR_W; col++) {
            uint64_t color = (bits & (1 << (7 - col))) ? fg : bg;
            gop_draw_pixel(x + col, y + row, color);
        }
    }
}

static void console_redraw_all() {
    gop_draw_rect(0,0, 
        cons.gop_fb->w,
        cons.gop_fb->h,
        cons.bg
    );

    for(int y=0; y < cons.max_rows; y++) {
        for(int x=0; x < cons.max_cols; x++) {
            char c = cons.buffer[y][x];
            if (c != ' ' && c != 0) {
                font_draw_char(x*cons.font_w,y*cons.font_h, c, cons.fg, cons.bg);
            }
        }
    }
}

static void console_scroll() {
    for(int y=0; y < cons.max_rows - 1; y++) {
        for(int x=0; x < cons.max_cols; x++) {
            cons.buffer[y][x] = cons.buffer[y+1][x];
        }
    }

    for(int x=0; x< cons.max_cols; x++){
        cons.buffer[cons.max_rows- 1][x] = ' ';
    }

    console_redraw_all();
}

static void console_putc(char c) {
    if(c == '\n') {
        cons.cur_x = 0;
        cons.cur_y++;
    } else if (c == '\r') {
        cons.cur_x = 0;
    } else if (c == '\b') {
        if(cons.cur_x == 0) return;
        cons.cur_x--;
        cons.buffer[cons.cur_y][cons.cur_x] = ' ';
        font_draw_char(cons.cur_x*cons.font_w,cons.cur_y*cons.font_h, ' ', cons.fg, cons.bg);
    } else {
        cons.buffer[cons.cur_y][cons.cur_x] = c;
        font_draw_char(cons.cur_x*cons.font_w,cons.cur_y*cons.font_h, c, cons.fg, cons.bg);
        cons.cur_x++;
    }
    
    // next line
    if(cons.cur_x >= cons.max_cols) {
        cons.cur_x = 0;
        cons.cur_y++;
    }
    
    if(cons.cur_y >= cons.max_rows) {
        console_scroll();
        cons.cur_y = cons.max_rows - 1;
    }
}

void console_printf(char *fmt, ...) {
    va_list ap;
    if(cons.locking) acquire(&cons.lk);
    va_start(ap, fmt);
    vprintf(console_putc, fmt, ap);
    va_end(ap);
    if(cons.locking) release(&cons.lk);
}

int64_t console_write(struct inode* ip, uint8_t *buf, uint64_t n) {
    iunlock(ip);
    acquire(&cons.lk);
    int64_t i;
    for(i=0; i < n; i++) {
        console_putc(buf[i] & 0xFF);
    }
    release(&cons.lk);
    ilock(ip);

    return i;
}

extern struct dev_sw devs[N_DEVS];
void console_init(void) {

    init_lock(&cons.lk, "console");
    
    extern struct gop_fb gop_fb;
    cons.gop_fb = &gop_fb;

    cons.font_w = 8;
    cons.font_h = 16;

    cons.max_cols = MIN(cons.gop_fb->w / cons.font_w, MAX_COLS);
    cons.max_rows = MIN(cons.gop_fb->h / cons.font_h, MAX_ROWS);

    cons.cur_y = 0;
    cons.cur_x = 0;

    cons.fg = GOP_WHI;
    cons.bg = GOP_BLK;

    cons.pixels_per_scanline = cons.gop_fb->pitch / (cons.gop_fb->bpp / 8);
    // pitch means byte per row
    
    devs[CONSOLE_DEVNUM].write = console_write;
    // devs[CONSOLE_DEVNUM].read = console_read;
    cons.locking = 1;

}
