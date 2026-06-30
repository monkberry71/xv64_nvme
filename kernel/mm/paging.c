#include <stdint.h>
#include <kernel/paging.h>
#include <kernel/mmu.h>
#include <kernel/mem_layout.h>
#include <kernel/debug.h>
#include <kernel/string.h>
#include <kernel/kalloc.h>
// Get the corresponding pte of an addr from the pml4
pte_t* walk_pml4(pte_t *pml4, const void* va, int alloc, int user) {
    pte_t *table = pml4;
    for(int lv=0; lv < 4; lv++) {
        int shift = 39 - lv * 9;
        pte_t *entry_p = &table[((uint64_t) va >> shift) & 0x1FF];

        if(lv == 3) {
            // the table is PT table.
            // We must return the entry or null
            return (alloc || (*entry_p & PTE_P)) ? entry_p : 0;
        }

        if(*entry_p & PTE_P) {
            if(*entry_p & PTE_PS) return entry_p;

            // All intermediate entries should have PTE_U if it is user space addr
            if(alloc && user) *entry_p |= PTE_U;
            
            // no ps bit -> next level
            table = P2V_DIR(PTE_ADDR(*entry_p));
            continue;
        }

        if(!alloc) return 0;
        
        table = kalloc();
        if(!table) return 0;
        memset(table, 0, PGSIZE_4KB);
        
        *entry_p = V2P_DIR(table) | PTE_W | PTE_P | (user ? PTE_U : 0);
    }
    return 0; // should not reach
}

// Make a mapping between va and pa on a pml4, and pa must be aligned
int map_pages(pte_t *pml4, const void* va, uint64_t size, uint64_t pa, pte_t perm) {

    if(pa % PGSIZE_4KB) return -1;
    if(size == 0) return -1;

    uint64_t first_page = ROUND_DOWN((uint64_t)va, PGSIZE_4KB);
    uint64_t last_page = ROUND_DOWN((uint64_t)va + size - 1, PGSIZE_4KB);

    uint64_t page_count = (last_page - first_page) / PGSIZE_4KB + 1;

    for(uint64_t i=0; i<page_count; i++) {
        uint64_t current_va = first_page + i * PGSIZE_4KB;
        uint64_t current_pa = pa + i * PGSIZE_4KB;
        
        int user = (perm & PTE_U) != 0;
        pte_t *current_page_pte = walk_pml4(pml4, (void*) current_va, 1, user);
        if(!current_page_pte) return -1;

        if(*current_page_pte & PTE_P) {
            panic("map_pages: remapping");
        }

        *current_page_pte = (current_pa) | perm | PTE_P;
    }
    return 0;
}

