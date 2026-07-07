#include <stdint.h>
#include <kernel/gop.h>
#include <kernel/mb2.h>
#include <kernel/kvm.h>
#include <kernel/helper.h>
#include <kernel/debug.h>

struct {
    void* fb_base;
    uint64_t w,h,pitch;
    uint8_t bpp, type;
} gop_fb;

int gop_init(void) {
    extern const struct mb2_info *reserved_mb2_info;
    struct mb2_it it;
    mb2_it_init(&it, reserved_mb2_info);

    for(; !it.end; mb2_it_next(&it)) {
        if(it.curr->type != MB2_TAG_FB) continue;

        struct mb2_tag_fb *fb_tag = (void*)it.curr;
        if(fb_tag->fb_type != 1) {
            panic("gop_init: unsupported fb type");
        }

        gop_fb.pitch = fb_tag->pitch;
        gop_fb.w = fb_tag->width;
        gop_fb.h = fb_tag->height;
        gop_fb.bpp = fb_tag->bpp;
        gop_fb.type = fb_tag->fb_type;

        // pitch is row byte size;
        uint64_t buf_size = fb_tag->pitch * fb_tag->height;
        gop_fb.fb_base = io_remap(fb_tag->addr, buf_size);

        log_inits("gop_init");
        return 0;
    }
    panic("gop_init: no gop found");
}

void gop_draw_pixel(uint64_t x, uint64_t y, uint64_t col) {
    uint8_t bytes_per_pixel = gop_fb.bpp / 8;
    uint8_t *pixel = (uint8_t*) gop_fb.fb_base + y * gop_fb.pitch + x * bytes_per_pixel;
    for(uint8_t i=0; i < bytes_per_pixel; i++) {
        pixel[i] = (col >> (i*8)) & 0xFF;
    }
}

void gop_draw_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint64_t color) {
    uint64_t x_end = MIN(x+w, gop_fb.w);
    uint64_t y_end = MIN(y+h, gop_fb.h);

    for(uint64_t row = y; row < y_end; row++) {
        for(uint64_t col = x; col < x_end; col++) {
            gop_draw_pixel(col, row, color);
        }
    }
}

