#pragma once
#include <stdint.h>

void kfree(void* dm_va);
void kalloc_init(void);
void* kalloc(void);