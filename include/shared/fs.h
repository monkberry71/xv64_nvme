#pragma once
#include <stdint.h>

#define BLK_SIZE 4096
#define DISK_SIZE (16 * 1024 * 1024)
#define N_BLKS (DISK_SIZE / BLK_SIZE)

#define ROOT_INODE 1

// struct dinode's size should be a power of two
// 2 + 2 + 2 + 2 + 8 + (8 * X) 
// = 8 * (X+2) = 8X + 16
// X should be a power of two - 2
// N_DIRECT + 1 = X = (a power of two) - 2
// N_DIRECT = (a power of two) - 3
// N_DIRECT = 13 seems to be good here
#define N_DIRECT 13
#define N_INDIRECT (BLK_SIZE / sizeof(uint64_t))
#define MAX_FILE_BLK (N_DIRECT + N_INDIRECT)

#define INODES_PB (BLK_SIZE / sizeof(struct dinode))
#define INODE_BLK_INDEX(i, sb) ((i) / INODES_PB + (sb).inode_start)

#define BITS_PB (BLK_SIZE * 8)
#define BIT_BLK_INDEX(b, sb) ((b) / BITS_PB + (sb).bitmap_start)

struct super_block {
    uint64_t size; // size of fs image in blocks
    uint64_t n_blocks; // data blocks count
    uint64_t n_inodes; // inodes count
    uint64_t inode_start; // block_no of the first inode block
    uint64_t bitmap_start; // block_no of the first free bitmap block
};

enum inode_type {
    T_NONE = 0,
    T_DIR = 1,
    T_FILE = 2,
    T_DEV = 3,
};

struct dinode {
    uint16_t type;
    uint16_t major;
    uint16_t minor;
    uint16_t n_link;
    uint64_t size;
    uint64_t addrs[N_DIRECT + 1];
};

// inode number 8 + char str 24
#define DIR_SIZE 24
struct dir_ent {
    uint64_t inode;
    char name[DIR_SIZE];
};

