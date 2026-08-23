#pragma once
#include <stdint.h>
#include <kernel/sleep_lock.h>
#include <shared/fs.h>

struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE } type;
    uint64_t ref;
    uint8_t readable;
    uint8_t writable;
    // struct pipe *pipe;
    struct inode *ip;
    uint64_t off;
};

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

struct dev_sw {
    int64_t (*read)(struct inode*, uint8_t*, uint64_t);
    int64_t (*write)(struct inode*, uint8_t*, uint64_t);
};

#define CONSOLE_DEVNUM 1
extern struct dev_sw devs[];

void file_init(void);
