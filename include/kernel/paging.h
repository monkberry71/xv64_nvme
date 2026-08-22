#pragma once
#include <stdint.h>
#include <kernel/mmu.h>

pte_t* walk_pml4(pte_t *pml4, const void* va, int alloc, int user);
int map_pages(pte_t *pml4, const void* va, uint64_t size, uint64_t pa, pte_t perm);