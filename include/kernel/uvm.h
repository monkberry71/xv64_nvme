#pragma once
#include <stdint.h>
#include <kernel/mmu.h>
#include <kernel/proc.h>

pte_t* setup_uvm(void);
uint64_t alloc_uvm(pte_t *pml4, uint64_t old_sz, uint64_t new_sz);
uint64_t dealloc_uvm(pte_t *pml4, uint64_t old_sz, uint64_t new_sz);
int load_uvm(pte_t *pml4, uint8_t *addr, struct inode *ip, uint64_t offset, uint64_t len);
void clear_pte_u(pte_t *pml4, char *uva);
void *uva2dma(pte_t *pml4, void *uva);
int64_t copy_out(pte_t *pml4, void *va, void *p, uint64_t len);
void free_vm(pte_t *pml4, uint64_t sz);
void switch_uvm(struct proc *p);
// void init_code_uvm(pte_t *pml4, uint8_t *init_code, uint64_t sz);
pte_t* copy_uvm(pte_t *pml4, uint64_t sz);
