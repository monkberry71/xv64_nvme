#include <stdint.h>
#include <kernel/mmu.h>
#include <kernel/mem_layout.h>
#include <kernel/string.h>
#include <kernel/paging.h>
#include <kernel/kalloc.h>
#include <kernel/debug.h>
#include <kernel/helper.h>
#include <kernel/fs.h>
#include <kernel/proc.h>
#include <kernel/x86_64.h>

// xv6 uses setup_kvm for #1. kpgdir init #2. user process vm init
// we need to separate it to two funcs because 
// 1. We can't use bump alloc after kpml4 init 
// 2. we need to copy from kpml4, not making new direct mapping and kernel mapping.
extern pte_t *kpml4;

pte_t* setup_uvm(void) {
    pte_t *pml4 = kalloc();
    if(!pml4) return 0;
    memset(pml4, 0, PGSIZE_4KB);

    for(int i = 256; i < 512; i++) {
        pml4[i] = kpml4[i];
    }

    return pml4;
}

// Shrink user mem space
uint64_t dealloc_uvm(pte_t *pml4, uint64_t old_sz, uint64_t new_sz) {
    if(new_sz >= old_sz) return old_sz;

    for(uint64_t addr = ROUND_UP(new_sz, PGSIZE_4KB); addr < old_sz;) {
        pte_t *pte = walk_pml4(pml4, (void *) addr, 0, 0);
        if(!pte) {
            // We dont know which (pml4, pdpt, pd) is null, so just be safe
            addr = ROUND_UP(addr + 1, PGSIZE_2MB);
            continue;
        }

        if(*pte & PTE_P) {
            uint64_t pa = PTE_ADDR(*pte);
            if(!pa)
                panic("dealloc_uvm: cant free");
            kfree(P2V_DIR(pa));
            *pte = 0;
        }
        addr += PGSIZE_4KB;
    }
    return new_sz;
}

// allocate physical pages to grow a user addr space
uint64_t alloc_uvm(pte_t *pml4, uint64_t old_sz, uint64_t new_sz) {
    if(new_sz < old_sz) return old_sz;

    // one pml4 entry single-handly manages 512GB
    // Upper 256 kernel space pml4 entries manages 512 * 256 GB
    // which is 2^(9+8) GB == 2^7 TB == 128 TB
    if(new_sz >= USER_TOP) return 0; 

    for(uint64_t addr = ROUND_UP(old_sz, PGSIZE_4KB); addr < new_sz; addr += PGSIZE_4KB) {
        void* mem = kalloc();
        if(!mem) {
            // alloc_uvm OOM
            dealloc_uvm(pml4, new_sz, old_sz);
            return 0;
        }
        memset(mem, 0, PGSIZE_4KB);
        if(map_pages(pml4, (void *) addr, PGSIZE_4KB, V2P_DIR(mem), PTE_W | PTE_U) < 0) {
            // alloc uvm OOM
            dealloc_uvm(pml4, new_sz, old_sz);
            kfree(mem);
            return 0;
        }
    }
    return new_sz;
}

// Read from the file and load it on the memory
int load_uvm(pte_t *pml4, uint8_t *addr, struct inode *ip, uint64_t offset, uint64_t len) {
    if( (uint64_t) addr % PGSIZE_4KB)
        panic("load_uvm: addr must be 4KB aligned");
    
    for(uint64_t i=0; i < len; i+=PGSIZE_4KB) {
        pte_t *pte = walk_pml4(pml4, addr+i, 0, 0);
        if(!pte)
            panic("load_uvm: pte does not exist");
        
        uint64_t pa = PTE_ADDR(*pte);
        uint64_t how_many_to_read = MIN((len-i), PGSIZE_4KB);
        uint64_t bytes_r = readi(ip, P2V_DIR(pa), offset+i, how_many_to_read);
        if(how_many_to_read != bytes_r) return -1;
    }
    return 0;
}

void clear_pte_u(pte_t *pml4, char *uva) {
    pte_t *pte = walk_pml4(pml4, uva, 0, 0);

    if(!pte) {
        panic("clear_pte_u: nothing to clear");
    }

    *pte &= ~PTE_U;
}

// Return the direct-mapped address of the user page containing uva.
void* uva2dma(pte_t *pml4, void* uva) {
    pte_t *pte = walk_pml4(pml4, uva, 0, 0);
    if(!pte) return 0;
    if(!(*pte & PTE_P)) return 0;
    if(!(*pte & PTE_U)) return 0;
    return P2V_DIR(PTE_ADDR(*pte));
}

// Copy len bytes from a kernel buffer to a user virtual address in pml4.
// Use this when pml4 is not the current cr3
int64_t copy_out(pte_t *pml4, void* va, void* p, uint64_t len) {
    uint8_t *buf = p;
    while(len > 0) {
        uint64_t va0 = ROUND_DOWN((uint64_t) va, PGSIZE_4KB);
        uint8_t *dst_page = uva2dma(pml4, (void *) va0);
        if(!dst_page) return -1;

        // va0 -- va -- (va0 + 4KB)
        //         <- to_write ->
        uint64_t to_write = PGSIZE_4KB - ((uint64_t)va - va0);
        to_write = MIN(len, to_write);

        // P2V_DIR(va0) -- P2V_DIR(va) -- P2V_DIR(va0 + 4KB)
        // dst_page        memcpy start 
        memcpy(dst_page + ((uint64_t)va - va0), buf, to_write);
        len -= to_write;
        buf += to_write;
        va = (void *) (va0 + PGSIZE_4KB);
    }
    return 0;
}

void switch_uvm(struct proc *p) {
    if(!p)
        panic("switch_uvm: no proc");
    if(!p->kstack)
        panic("switch_uvm: no kstac");
    if(!p->pml4)
        panic("switch_uvm: no pml4");

    push_cli();
    mycpu()->tss.rsp[0] = (uint64_t) p->kstack + KERNEL_STACK_SIZE;
    mycpu()->kstack_temp = (uint64_t) p->kstack + KERNEL_STACK_SIZE;
    lcr3(V2P_DIR(p->pml4));
    pop_cli();
}

// free recursively, table must be va
static void free_entry(pte_t *table, int lv) {
    if(lv == 3) {
        // lv3 -> pt, all entries are already freed by dealloc_uvm
        // Thus, just free itself
        kfree(table);
        return;
    }

    // lv0 -> pml4, we need to keep the upper 256 entries, which is kernel's
    int max = (lv == 0) ? 256 : 512;
    for(int i=0; i<max; i++) {
        if(table[i] & PTE_P) {
            free_entry(P2V_DIR(PTE_ADDR(table[i])), lv+1);
        }
    }
    kfree(table);
    return;
}

// Free page tables themselves
// sz is old sz
void free_vm(pte_t *pml4, uint64_t sz) {
    if(!pml4) 
        panic("free_vm: no pml4");

    dealloc_uvm(pml4, sz, 0);

    free_entry(pml4, 0);
}

// only for initcode
void init_code_uvm(pte_t *pml4, uint8_t *init_code, uint64_t sz) {
    if(sz >= PGSIZE_4KB)
        panic("init_uvm: initcode is too big");
    
    void *mem = kalloc();
    if(!mem)
        panic("init_uvm: kalloc failed");
    memset(mem, 0, PGSIZE_4KB);
    map_pages(pml4, 0, PGSIZE_4KB, V2P_DIR(mem), PTE_W | PTE_U);
    memcpy(mem, init_code, sz);
}
