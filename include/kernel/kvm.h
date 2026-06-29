#pragma once
#include <stdint.h>
#include <kernel/mmu.h>

pte_t* setup_kvm(void);
void kvm_alloc(void);
void switch_kvm(void);