#pragma once
#include <stdint.h>

#define ENTRY_BASE 0x100000
#define KERN_BASE 0xFFFFFFFF80000000ULL
#define DIRECT_BASE 0xFFFF888000000000ULL
#define IO_REMAP_BASE 0xFFFFC00000000000ULL
#define PHY_STOP (1ULL << 37) // 128GB



#define V2P_KERN(va) ((uint64_t)(va) - KERN_BASE)
#define P2V_KERN(pa) ((void*) ((uint64_t)(pa) + KERN_BASE))

#define V2P_DIR(va) ((uint64_t)(va) - DIRECT_BASE)
#define P2V_DIR(pa) ((void*) ((uint64_t)(pa) + DIRECT_BASE))