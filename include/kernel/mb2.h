#pragma once

#include <stdint.h>

// https://www.gnu.org/software/grub/manual/multiboot2/html_node/Boot-information-format.html

// Boot information consists of 1. fixed part and 2. a series of tags. Its start is 8-bytes aligned. 

// It is a basic tag structure
struct mb2_tag {
    uint32_t type;
    uint32_t size;
};

struct mb2_info {
    uint32_t total_size; // The fixed part
    uint32_t reserved; // The fixed part
    struct mb2_tag tags[]; // 2. a series of tags
};


// Framebuffer tag
#define MB2_TAG_FB 8
struct mb2_tag_fb {
    struct mb2_tag tag;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t fb_type;
    uint16_t reserved;
};


// Memory map tag

//‘entry_size’ contains the size of one entry so that in future new fields may be added to it. It’s guaranteed to be a multiple of 8. 
// Each entry has the following structure:

#define MB2_MMAP_RAM 1
#define MB2_MMAP_ACPI 3
struct mb2_mmap_entry {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
};

#define MB2_TAG_MMAP 6
struct mb2_tag_mm {
    struct mb2_tag tag;
    uint32_t entry_size;
    uint32_t entry_version;
    struct mb2_mmap_entry entries[];
};

// ==== mb2 iterator ====
struct mb2_it {
    struct mb2_tag *curr;
    int end;
};

void preserve_mb2(void* mb2_info_phys);
void mb2_it_init(struct mb2_it *it, struct mb2_info *base);
void mb2_it_next(struct mb2_it *it);
