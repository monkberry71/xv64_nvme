#pragma once
#include <stdarg.h>

typedef void (*putc_t)(char);

// static void print_int(putc_t putc, int xx, int base, int sign);
void vprintf(putc_t putc, char *fmt, va_list ap);

void* memset(void* dst, int c, uint64_t n);
void* memcpy(void* dst, void* src, uint64_t n);
int memcmp(const void* a, const void* b, uint64_t n);
int strncmp(const char *a, const char *b, uint64_t n);
char *strncpy(char *dst, const char *src, uint64_t n);
