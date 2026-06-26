#pragma once
#include <stdint.h>

struct bump_mem {
    uint64_t bump_line; // Phys
    int enabled; // We must not use bump allocator after we are able to use kalloc, it might mess up the kalloc
};

int bump_init(void);
void* bump_alloc(void);
void bump_disable(void);
void* get_bump_line(void);