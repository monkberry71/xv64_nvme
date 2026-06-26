#pragma once

#define PGSIZE_4KB 4096


#define ROUND_UP(addr, size) (((uint64_t)(addr) + (uint64_t)(size) - 1) & ~((uint64_t)(size) - 1))
