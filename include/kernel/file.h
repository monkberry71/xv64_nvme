#pragma once
#include <stdint.h>
#include <kernel/sleep_lock.h>
#include <shared/fs.h>

struct inode {
    uint64_t dev;
    uint64_t inum;
    uint64_t ref_count;
    struct sleep_lock lk;
    int valid;

    uint16_t type;
    uint16_t major;
    uint16_t minor;
    uint16_t n_link;
    uint64_t size;
    uint64_t addrs[N_DIRECT + 1];
};

