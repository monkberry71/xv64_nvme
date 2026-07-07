#include <stdint.h>
#include <kernel/gop.h>

int64_t sys_draw(void) {
    gop_draw_rect(30, 30, 30, 30, GOP_BLU);
    for(;;);
}