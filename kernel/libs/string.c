#include <stdint.h>
#include <stdarg.h>
#include <kernel/string.h>
#include <kernel/debug.h>

void* memset(void* dst, int c, uint64_t n) {
    char *p = dst;
    for(int i=0; i<n; i++) {
        p[i] = c;
    }
    return dst;
}

void* memcpy(void* dst, void* src, uint64_t n) {
    char *d = dst;
    char *s = src;
    for(int i=0; i<n; i++) {
        d[i] = s[i];
    }
    return dst;
}

static char digits[] = "0123456789ABCDEF";
static void print_int(putc_t putc, int64_t xx, int base, int sign) {
    char buf[16];

    uint64_t x;
    if(sign && (sign = (xx < 0))) {
        x = -xx;
    } else {
        x = xx;
    }

    if(sign) {
        putc('-');
    }

    int i = 0;
    do {
        buf[i++] = digits[x % base];
    } while((x /= base) != 0);

    while(--i >= 0) {
        putc(buf[i]);
    }
}

void vprintf(putc_t putc, char *fmt, va_list ap) {
    // va_list ap;
    if (fmt == 0) panic("null fmt");

    // va_start(ap, fmt);
    
    int c;
    for(int i=0; (c = fmt[i] & 0xFF) != 0; i++) {
        if(c != '%') {
            // if not %, just print
            putc(c);
            continue;
        }

        c = fmt[++i] & 0xFF;
        if(c == 0) break;

        switch(c) {
            case 'd': {
                print_int(putc, va_arg(ap, int), 10, 1);
                break;
            }
            case 'x': {
                print_int(putc, va_arg(ap, uint64_t), 16, 0);
                break;
            }
            case 'p': {
                print_int(putc, (uint64_t)va_arg(ap, void*), 16, 0);
                break;
            }
            case 's': {
                char *s;
                if((s = va_arg(ap, char*)) == 0) {
                    s = "[null]";
                }
                for(; *s; s++) putc(*s);
                break;
            }
            default: {
                putc('%');
                putc(c);
                break;
            }
        }
    }
    // va_end(ap);
}
