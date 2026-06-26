#pragma once
#include <stdarg.h>

typedef void (*putc_t)(char);

// static void print_int(putc_t putc, int xx, int base, int sign);
void vprintf(putc_t putc, char *fmt, va_list ap);

void* memset(void* dst, int c, uint64_t n);
void* memcpy(void* dst, void* src, uint64_t n);