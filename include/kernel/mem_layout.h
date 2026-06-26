#pragma once
#include <stdint.h>

#define ENTRY_BASE 0x100000
#define KERN_BASE 0xFFFFFFFF80000000ULL

#define V2P_KERN(va) ((uint64_t)(va) - KERN_BASE)
#define P2V_KERN(pa) ((void*) ((uint64_t)(pa) + KERN_BASE))