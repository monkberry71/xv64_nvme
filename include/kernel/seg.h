#pragma once
#include <stdint.h>

typedef uint64_t segment_desc_t;

// struct segment_desc {
//     uint64_t limit_low : 16; 0
//     uint64_t base_low : 16; 16
//     uint64_t base_mid : 8; 24
//     uint64_t access : 8; 40
//     uint64_t limit_high : 4; 48
//     uint64_t flags : 4; 52
//     uint64_t base_high : 8; 56
// }

#define SEG_KCODE 1
#define SEG_KDATA 2 
#define SEG_UDATA32 3 // place holder for syscall structure
#define SEG_UDATA 4 // 
#define SEG_UCODE 5
#define SEG_TSS 6   

void bsp_seg_init(void);