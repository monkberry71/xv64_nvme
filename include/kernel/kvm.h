#pragma once
#include <stdint.h>
#include <kernel/mmu.h>

pte_t* setup_kvm(void);
void kvm_alloc(void);
void switch_kvm(void);
void io_init(void);
void* io_remap(uint64_t pa, uint64_t size);