#pragma once
#include <stdint.h>
#include <kernel/file.h>
#include <shared/fs.h>

void read_sb(struct super_block *sb);
void init_sb(uint64_t size, uint64_t n_blks, uint64_t n_ino, uint64_t ino_start, uint64_t bmap_start);
void inode_init(void);

struct inode *ialloc(uint16_t type);
void iupdate(struct inode *ip);
struct inode *idup(struct inode *ip);
void ilock(struct inode *ip);
void iunlock(struct inode *ip);
void iput(struct inode *ip);

uint64_t bmap(struct inode *ip, uint64_t bn);
void stati(struct inode *ip, struct stat *st);
int64_t readi(struct inode *ip, uint8_t *dst, uint64_t off, uint64_t n);
int64_t writei(struct inode *ip, uint8_t *src, uint64_t off, uint64_t n);

struct inode *dir_lookup(struct inode *dp, char *name, uint64_t *out_poff);
int dir_link(struct inode *dp, char *name, uint64_t inum);

struct inode *namei(char *path);
struct inode *namei_parent(char *path, char *name);
