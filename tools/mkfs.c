#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shared/fs.h>

#define min(a,b) ((a) < (b) ? (a):(b))
#define BIT(x) (1ULL << x)

// Making a fs image
// Blk layout:
// zero | sb | free_bitmap | inodes[3] | data...

typedef struct {
    char bytes[BLK_SIZE];
} Block;

Block img[N_BLKS];
struct super_block *sb = (void*) &img[1];

void init_sb(
    uint64_t size, 
    uint64_t n_blks, 
    uint64_t n_ino, 
    uint64_t ino_start, 
    uint64_t bmap_start) {
    
    sb->size = size;
    sb->n_blocks = n_blks;
    sb->n_inodes = n_ino;
    sb->inode_start = ino_start;
    sb->bitmap_start = bmap_start;
}

// get data block
int blk_alloc(void) {
    static int free_bit_index = 6; // bump allocator
    Block *bitmap_blk_no = (void*) &img[BIT_BLK_INDEX(free_bit_index, *sb)];

    uint64_t byte_index_in_blk = free_bit_index / 8;
    uint64_t bit_index_in_byte = free_bit_index % 8;

    bitmap_blk_no->bytes[byte_index_in_blk] |= BIT(bit_index_in_byte);

    return free_bit_index++;
}

struct dinode* inodes = (void*) &img[3];
int dialloc(uint16_t type) {
    static int free_dinode_index = ROOT_INODE;

    struct dinode *to_alloc = &inodes[free_dinode_index];
    to_alloc->type = type;
    return free_dinode_index++;
}

// Which blk in the disk holds the inode ip's file (bn)th blk?
uint64_t bmap(struct dinode *ip, uint64_t bn) {
    if(bn < N_DIRECT) {
        if(ip->addrs[bn] == 0) 
            ip->addrs[bn] = blk_alloc();
        return ip->addrs[bn];
    }

    bn -= N_DIRECT;
    if(bn >= N_INDIRECT) {
        fprintf(stderr, "bmap: OOR");
        exit(1);
    }

    if(ip->addrs[N_DIRECT] == 0) {
        ip->addrs[N_DIRECT] = blk_alloc();
    }

    uint64_t *indirect_addrs = (void*) &img[ip->addrs[N_DIRECT]];
    if(indirect_addrs[bn] == 0) 
        indirect_addrs[bn] = blk_alloc();
    return indirect_addrs[bn];
}

int64_t readi(struct dinode *ip, uint8_t *dst, uint64_t off, uint64_t n) {
    if(off > ip->size || off > UINT64_MAX - n) return -1;
    if(n > ip->size - off) n = ip->size - off;

    uint64_t m, tot;
    for(tot = 0; tot < n; tot += m, off += m, dst += m) {
        Block *b = &img[bmap(ip, off / BLK_SIZE)];
        m = min(n - tot, BLK_SIZE - off % BLK_SIZE);
        memcpy(dst, b->bytes + off % BLK_SIZE, m);
    }

    return tot;
}

int64_t writei(struct dinode *ip, uint8_t *src, uint64_t off, uint64_t n) {
    if(off > ip->size || off > UINT64_MAX - n) return -1;
    if(n > (N_DIRECT + N_INDIRECT) * BLK_SIZE - off) return -1;

    uint64_t m, tot;
    for(tot = 0; tot < n; tot += m, off += m, src += m) {
        Block *b = &img[bmap(ip, off / BLK_SIZE)];
        m = min(n - tot, BLK_SIZE - off % BLK_SIZE);
        memcpy(b->bytes + off % BLK_SIZE, src, m);
    }

    if(n > 0 && off > ip->size) {
        ip->size = off;
    }

    return tot;
}

// Add new dir entry on direct inode
int dir_link(struct dinode *dp, char *name, uint64_t inode) {
    struct dir_ent de;
    uint64_t off;
    for(off = 0; off < dp->size; off += sizeof(de)) {
        int64_t bytes_read = readi(dp, (void*) &de, off, sizeof(de));
        if(bytes_read != sizeof(de)) {
            fprintf(stderr, "dir_link: cant read");
            exit(1);
        }
        if(de.inode == 0) break; // Found emtpy dir_ent
    }

    strncpy(de.name, name, DIR_SIZE);
    de.inode = inode;

    int64_t bytes_written = writei(dp, (void*) &de, off, sizeof(de));
    if(bytes_written != sizeof(de)) {
        fprintf(stderr, "dir_link: cant write");
        exit(1);
    }

    return 0;
}

int file2i(struct dinode *ip, char *file_name_on_host) {
    if (ip->type != T_FILE) return -1;

    FILE *f = fopen(file_name_on_host, "rb");
    if(f == 0) return -1;

    uint8_t buf[BLK_SIZE];
    uint64_t off = 0;

    for(;;) {
        uint64_t n = fread(buf, 1, sizeof(buf), f);
        if(n > 0) {
            if(writei(ip, buf, off, n) != (int64_t) n) {
                fclose(f);
                return -1;
            }
            off += n;
        }

        if(n < sizeof(buf)) {
            if(ferror(f)) {
                fclose(f);
                return -1;
            }
            break;
        }
    }
    fclose(f);
    return 0;
}

void add_file(struct dinode *rooti, char *name) {
    int ino = dialloc(T_FILE);
    struct dinode *ip = &inodes[ino];
    ip->n_link = 1;

    char path[128];
    snprintf(path, sizeof(path), "user/%s", name);
    if(file2i(ip, path) < 0) {
        fprintf(stderr, "file2i %s failed", name);
        exit(1);
    }
    dir_link(rooti, name, ino);
}

int save_img(void) {
    FILE *f = fopen("fs.img", "wb");
    if(!f) return -1;

    fwrite(img, sizeof(img), 1, f);
    fclose(f);
    return 0;
}

int main(void) {
    init_sb(N_BLKS, (N_BLKS - 5), (3*INODES_PB), INODE_ARR_BLK, BITMAP_BLK);

    // Manually allocate block 0, superblock, bitmap, and inode blocks.
    img[sb->bitmap_start].bytes[0] = 0x3F;

    // Make root ino
    struct dinode *rooti = &inodes[dialloc(T_DIR)];
    rooti->n_link = 1;
    dir_link(rooti, ".", ROOT_INODE);
    dir_link(rooti, "..", ROOT_INODE);

    int dev_dir_ino = dialloc(T_DIR);
    struct dinode *ddi = &inodes[dev_dir_ino];
    ddi->n_link = 1;
    dir_link(ddi, ".", dev_dir_ino);
    dir_link(ddi, "..", ROOT_INODE);
    dir_link(rooti, "dev", dev_dir_ino);
    rooti->n_link++;


    int console_ino = dialloc(T_DEV);
    struct dinode *console = &inodes[console_ino];
    console->n_link = 1;
    console->major = 1;
    console->minor = 1;
    dir_link(ddi, "console", console_ino);

    add_file(rooti, "test_file.txt");
    add_file(rooti, "init");
    add_file(rooti, "nsh");
    add_file(rooti, "cat");
    add_file(rooti, "echo");
    add_file(rooti, "mkdir");
    add_file(rooti, "ls");

    save_img();
}
