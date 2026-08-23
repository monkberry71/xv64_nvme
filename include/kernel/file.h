#pragma once
#include <stdint.h>
#include <kernel/sleep_lock.h>
#include <shared/fs.h>

#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200

struct pipe;
struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE } type;
    uint64_t ref;
    uint8_t readable;
    uint8_t writable;
    struct pipe *pipe;
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
struct file* file_dup(struct file *f);
int64_t file_read(struct file *f, uint8_t *addr, uint64_t n);
int64_t file_write(struct file *f, uint8_t *addr, uint64_t n);
void file_close(struct file *f);
struct file* file_alloc(void);
int file_stat(struct file *f, struct stat *st);