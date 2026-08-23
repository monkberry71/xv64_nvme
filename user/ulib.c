#include <stdint.h>
#include <stdarg.h>
#include <user/user.h>
#include <user/fcntl.h>

uint64_t strlen(const char *s) {
    int n;
    for(n = 0; s[n]; n++);
    return n;
}

static void putc(int fd, char c) {
    write(fd, &c, 1);
}

static char digits[] = "0123456789abcdef";
static void print_int(int fd, int xx, int base, int sign) {
    char buf[16];
    
    unsigned x;
    if(sign && (sign = (xx < 0))) {
        x = -xx;
    } else {
        x = xx;
    }
    
    
    int i = 0;
    do {
        buf[i++] = digits[x % base];
    } while((x /= base) != 0);
    
    if(sign) {
        buf[i++] = '-';
    }
    while(--i >= 0) {
        putc(fd, buf[i]);
    }
}

void printf(int fd, const char *fmt, ...) {
    va_list ap;
    if(fmt == 0) {
        return;
    }
    
    int c;
    va_start(ap, fmt);
    for(int i=0; (c = fmt[i] & 0xFF) != 0; i++) {
        if(c != '%') {
            // if %, just print bro
            // console_putc(c)
            putc(fd, c);
            continue;
        }
        
        // ok c is the one that come after %
        c = fmt[++i] & 0xFF;
        if(c==0) break;
        
        switch(c) {
            case 'd': {
                print_int(fd, va_arg(ap, int), 10, 1);
                break;
            }
            case 'x': {
                print_int(fd, va_arg(ap, int), 16, 0);
                break;
            }
            case '%': {
                putc(fd, c);
                break;
            }
            case 's': {
                char *s;
                if((s = va_arg(ap, char*)) == 0) {
                    s = "[null]";
                }
                for(; *s; s++) putc(fd, *s);
                break;
            }
            default: {
                // console_putc('%');
                // console_putc(c);
                putc(fd, '%');
                putc(fd, c);
                break;
            }
        }
    }
    va_end(ap);

}

int64_t stat(const char *n, struct stat *st) {
    int fd = open(n, O_RDONLY);
    if(fd < 0) return -1;
    int r = fstat(fd, st);
    close(fd);
    return r;
}

void* memset(void* dst, int c, uint64_t n) {
    char *p = dst;
    while(n-- > 0) *p++ = c;
    return dst;
}

void *memcpy(void *dst, const void *src, uint64_t n) {
    char *d = dst, *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

char*
strchr(const char *s, char c)
{
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}