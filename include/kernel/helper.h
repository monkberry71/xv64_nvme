#pragma once

// Useful macros

#define MIN(x,y) ((x) > (y) ? (y) : (x))
#define MAX(x,y) ((x) < (y) ? (y) : (x))

#define BIT(n) (1ULL << (n))

// these `size` need to be always power of 2
#define ROUND_UP(addr, size) (((uint64_t)(addr) + (uint64_t)(size) - 1) & ~((uint64_t)(size) - 1))
#define ROUND_DOWN(addr, size) ((uint64_t)(addr) & ~((uint64_t)(size) - 1))

#define N_ELEMS(arr) (sizeof(arr) / sizeof((arr)[0]))
