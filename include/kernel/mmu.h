#pragma once

#define PGSIZE_4KB 4096
#define PGSIZE_2MB (2 * 1024 * 1024) 
#define PGSIZE_1GB (1ULL * 1024 * 1024 * 1024)

// these `size` need to be always power of 2
#define ROUND_UP(addr, size) (((uint64_t)(addr) + (uint64_t)(size) - 1) & ~((uint64_t)(size) - 1))
#define ROUND_DOWN(addr, size) ((uint64_t)(addr) & ~((uint64_t)(size) - 1))

typedef uint64_t pte_t;

#define PML4_IDX(va) (((uint64_t)(va) >> 39) & 0x1FF)
#define PDPT_IDX(va) (((uint64_t)(va) >> 30) & 0x1FF)
#define PD_IDX(va) (((uint64_t)(va) >> 21) & 0x1FF)
#define PT_IDX(va) (((uint64_t)(va) >> 12) & 0x1FF)
#define PAGE_OFFSET(va) ((uint64_t)(va) & (PGSIZE_4KB - 1))

#define PTE_ADDR(pte) ((pte) & ~(PGSIZE_4KB - 1))
#define PTE_FLAGS(pte) ((pte) & (PGSIZE_4KB - 1))

#define BIT(n) (1ULL << (n))

#define PTE_P BIT(0) // present
#define PTE_W BIT(1) // write enable
#define PTE_U BIT(2) // user 
#define PTE_PWT BIT(3) // write through
#define PTE_PCD BIT(4) // cache disable
#define PTE_A BIT(5) // accessed
#define PTE_D BIT(6) // dirty
#define PTE_PS BIT(7) // page size
#define PTE_PAT BIT(7) // if it is Page table (the last level), it is PAT
#define PTE_G BIT(8) // global
#define PTE_XD BIT(63) // execute disable

// direct mappin ptes should be
// 1. present
// 2. global, so it wont be flushed when context changed(cr3 changed)
// 3. writable
#define DM_ENTRY_FLAGS (PTE_P | PTE_G | PTE_W) 
#define KM_ENTRY_FLAGS (PTE_P | PTE_G | PTE_W)