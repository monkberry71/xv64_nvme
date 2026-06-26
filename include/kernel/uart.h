#pragma once
#include <stdint.h>

int serial_init(void);
void serial_putc(const char c);
void serial_printf(char *fmt, ...);