#pragma once
#include <stdint.h>
#define ELF_MAGIC 0x464C457FU

struct elf_header {
    uint32_t magic;
    uint8_t e_ident_rest[12];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t ph_off;
    uint64_t sh_off;
    uint32_t flags;
    uint16_t eh_size;
    uint16_t ph_entry_size; 
    uint16_t ph_num;
    uint16_t sh_entry_size;
    uint16_t sh_num;
    uint16_t sh_str_ndx;
};

struct prog_header {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t file_sz;
    uint64_t mem_sz;
    uint64_t align;
};

// Values for Proghdr type
#define ELF_PROG_LOAD           1 // load this to mem

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      1 
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4