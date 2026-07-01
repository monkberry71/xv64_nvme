#pragma once
#include <stdint.h>

typedef uint64_t segment_desc_t;

//https://wiki.osdev.org/Task_State_Segment
struct task_state_segment {
    uint32_t reserved_0;
    uint64_t rsp[3];
    uint32_t reserved_1;
    uint32_t reserved_2;
    uint64_t ist[7];
    uint32_t reserved_3;
    uint32_t reserved_4;
    uint16_t reserved_5;
    uint16_t iopb;
} __attribute__((packed));

struct tss_descriptor {
    uint16_t limit_0_15;
    uint16_t base_0_15;
    uint8_t base_16_23;
    uint8_t access;
    uint8_t limit_16_19_and_flags;
    uint8_t base_24_31;
    uint32_t base_32_63;
    uint32_t reserved;
} __attribute__((packed));

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