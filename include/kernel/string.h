#pragma once
#include <stdarg.h>

typedef void (*putc_t)(char);

// static void print_int(putc_t putc, int xx, int base, int sign);
void vprintf(putc_t putc, char *fmt, va_list ap);
