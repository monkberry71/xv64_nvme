#include <stdint.h>
#include <kernel/fs.h>
#include <shared/fs.h>
#include <kernel/bio.h>
#include <kernel/string.h>
#include <kernel/defs.h>
#include <kernel/debug.h>
#include <kernel/uart.h>
#include <kernel/proc.h>
#include <kernel/helper.h>

struct super_block sb;

static struct inode *iget(uint64_t inum);

// Cache super block
void read_sb(struct super_block *sb) {
    struct buf *bp = bread(1);
    memcpy(sb, bp->dma_buf, sizeof(*sb));
    brelse(bp);
}

// read_sb needs to read from the disk with sleeplock
// which needs a scheduler
// so for first test, just use hard-coded sb
void init_sb(
    uint64_t size, 
    uint64_t n_blks, 
    uint64_t n_ino, 
    uint64_t ino_start, 
    uint64_t bmap_start) {
    
    sb.size = size;
    sb.n_blocks = n_blks;
    sb.n_inodes = n_ino;
    sb.inode_start = ino_start;
    sb.bitmap_start = bmap_start;
}

static void bzero(uint64_t bno) {
    struct buf *bp = bread(bno);
    memset(bp->dma_buf, 0, BLK_SIZE);
    bwrite(bp);
    brelse(bp);
}

// blk allocator
static uint64_t blk_alloc(void) {
    for(uint64_t b = 0; b < sb.size; b += BITS_PB) {
        struct buf *bp = bread(BIT_BLK_INDEX(b, sb));
        uint8_t *view = bp->dma_buf;

        // (b+bi)th block 
        for(uint64_t bi = 0; bi < BITS_PB && b + bi < sb.size; bi++) {
            uint8_t mask = BIT(bi % 8);
            if((view[bi/8] & mask) != 0) continue;

            view[bi/8] |= mask;
            bwrite(bp);
            brelse(bp);
            bzero(b + bi);
            return b + bi;
        }
        brelse(bp);
    }
    panic("blk_alloc: oob");
}

static void blk_free(uint64_t blk) {
    struct buf *bp = bread(BIT_BLK_INDEX(blk, sb));

    // There is only one bitmap block actually, so this is pointless
    uint64_t bi = blk % BITS_PB;
    uint8_t *view = bp->dma_buf;
    uint8_t mask = BIT(bi % 8);
    if((view[bi/8] & mask) == 0) 
        panic("blk_free: double free");

    view[bi/8] &= ~mask;
    bwrite(bp);
    brelse(bp);
}

// inodes
// It is similar to bcache, but it is more about syncing access to inode
// In bcache, only one thread could get the pointer and other threads sleep for it
// However in icache, threads or struct file could get pointers limitlessly.
// inode->ref_count tracks the num of in-memory pointers to the icache entry
// So thread's stack or struct file (or etc) could have a pointer,
// but they need to acquire the sleeplock to edit inode
// Same as bcache, when selecting icache entry
struct {
    struct spin_lock lk;
    struct inode inodes[N_INODE];
} icache;

void inode_init(void) {
    init_lock(&icache.lk, "icache");

    for(int i = 0; i < N_INODE; i++) {
        struct inode *entry = &icache.inodes[i];
        init_sleep_lock(&entry->lk, "inode");
    }


    // Use read_sb or init_sb
    // read_sb(&sb);
    init_sb(N_BLKS, (N_BLKS - 5), (3*INODES_PB), INODE_ARR_BLK, BITMAP_BLK);

    log_inits("inode_init");
}

// Allocate an inode on the storage
// The storage's inode block manages all inode allocation
struct inode* ialloc(uint16_t type) {
    for(int i = 1; i < sb.n_inodes; i++) {
        struct buf *bp = bread(INODE_BLK_INDEX(i, sb));
        struct dinode *dip = (struct dinode *) bp->dma_buf + i % INODES_PB;

        if(dip->type != T_NONE) {
            brelse(bp);
            continue;
        }

        memset(dip, 0, sizeof(*dip));
        dip->type = type;
        bwrite(bp);
        brelse(bp);
        return iget(i);
    }
    panic("ialloc: no inodes");
}

// write-through inode to disk
// must be called after every change to dinode fields
// A caller must hold ip->lock for callee, which is me
void iupdate(struct inode *ip) {
    struct buf *bp = bread(INODE_BLK_INDEX(ip->inum, sb));
    struct dinode *dip = (struct dinode *) bp->dma_buf + ip->inum % INODES_PB;

    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->n_link = ip->n_link;
    dip->size = ip->size;

    memcpy(dip->addrs, ip->addrs, sizeof(ip->addrs));
    bwrite(bp);
    brelse(bp);
}

// Find the inode with inum on cache, alloc on cache if there is not
// unlike bcache, in-memory ref can be multiple
// However, when someone gonna use it, they need to acquire the lock
static struct inode* iget(uint64_t inum) {
    acquire(&icache.lk);
    struct inode *empty = 0;

    struct inode *ip;
    for(int i=0; i < N_INODE; i++) {
        ip = &icache.inodes[i];
        if(ip->ref_count > 0 && ip->inum == inum) {
            ip->ref_count++;
            release(&icache.lk);
            return ip;
        }

        // if ref == 0, just evict
        if(!empty && ip->ref_count == 0) {
            empty = ip;
        }
    }

    if(!empty) 
        panic("iget: cannot evict");
    
    ip = empty;
    ip->inum = inum;
    ip->ref_count = 1;

    // valid field must be protected with sleeplock
    // However, in here, we are sure that ip->ref_cnt was 0
    // So there is no one read or writing valid
    ip->valid = 0;
    release(&icache.lk);

    return ip;    
}

struct inode* idup(struct inode *ip) {
    acquire(&icache.lk);
    ip->ref_count++;
    release(&icache.lk);
    return ip;
}

void ilock(struct inode *ip) {
    if(!ip || ip->ref_count < 1) 
        panic("ilock: nothing to lock");

    acquire_sleep(&ip->lk);

    // icache always has the freshest, latest value
    // so if it was valid, we can just use it
    if(ip->valid) return;

    // So if it was not valid, we need to load from the disk

    struct buf *bp = bread(INODE_BLK_INDEX(ip->inum, sb));
    struct dinode *dip = (struct dinode *) bp->dma_buf + ip->inum % INODES_PB;

    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->n_link = dip->n_link;
    ip->size = dip->size;
    memcpy(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if(ip->type == T_NONE) {
        panic("ilock: just read non-valid inode from disk");
    }
}

void iunlock(struct inode *ip) {
    if(!ip)
        panic("iunlock: null");
    if(!holding_sleep(&ip->lk)) 
        panic("iunlock: double unlock");
    if(ip->ref_count < 1)
        panic("iunlock: ref_count is not valid");

    release_sleep(&ip->lk);
}

// discard inode file's content
// A caller must hold ip->lock for callee, which is me
static void itrunc(struct inode *ip) {
    for(int i=0; i<N_DIRECT; i++) {
        if(!ip->addrs[i]) continue;
        blk_free(ip->addrs[i]);
        ip->addrs[i] = 0;
    }

    // It has an indirect block
    if(ip->addrs[N_DIRECT]) {
        struct buf *bp = bread(ip->addrs[N_DIRECT]);
        uint64_t *b_addr =(uint64_t *) bp->dma_buf;
        
        for(int i=0; i<N_INDIRECT; i++) {
            if(b_addr[i])
                blk_free(b_addr[i]);
        }
        brelse(bp);
        blk_free(ip->addrs[N_DIRECT]);
        ip->addrs[N_DIRECT] = 0;
    }
    ip->size = 0;
    iupdate(ip);
}

void iput(struct inode *ip) {
    acquire_sleep(&ip->lk);
    if(ip->valid && ip->n_link == 0) {

        // ip->n_link == 0,
        // iget cannot increase ref_count
        // since there is no way to earn the inode number
        // Thus, we can just release the lock fast
        acquire(&icache.lk);
        uint64_t r = ip->ref_count;
        release(&icache.lk);

        if(r == 1) {
            itrunc(ip);
            ip->type = T_NONE;
            iupdate(ip);
            ip->valid = 0;
        }
    }
    release_sleep(&ip->lk);

    acquire(&icache.lk);
    ip->ref_count--;
    release(&icache.lk);
}

// Which blk in the disk holds the inode ip's file (bn)th blk?
uint64_t bmap(struct inode *ip, uint64_t bn) {
    if(bn < N_DIRECT) {
        if(ip->addrs[bn] == 0) 
            ip->addrs[bn] = blk_alloc();
        return ip->addrs[bn];
    }

    bn -= N_DIRECT;
    if(bn >= N_INDIRECT) {
        panic("bmap: out of range");
    }

    if(ip->addrs[N_DIRECT] == 0) {
        ip->addrs[N_DIRECT] = blk_alloc();
    }

    struct buf *bp = bread(ip->addrs[N_DIRECT]);
    uint64_t *indirect_addrs = (uint64_t *) bp->dma_buf;
    if(indirect_addrs[bn] == 0) {
        indirect_addrs[bn] = blk_alloc();
        bwrite(bp);
    }
    uint64_t block_no_ret = indirect_addrs[bn];
    brelse(bp);
    return block_no_ret;
}

// caller must hold sleeplock for callee
void stati(struct inode *ip, struct stat *st) {
    st->dev = ip->dev;
    st->ino = ip->inum;
    st->type = ip->type;
    st->n_link = ip->n_link;
    st->size = ip->size;
}

// caller must hold sleeplock for callee
int64_t readi(struct inode *ip, uint8_t *dst, uint64_t off, uint64_t n) {
    if(off > ip->size || off > UINT64_MAX - n) return -1;
    if(n > ip->size - off) n = ip->size - off;

    uint64_t m, tot;
    for(tot = 0; tot < n; tot += m, off += m, dst += m) {
        // Block *b = &img[bmap(ip, off / BLK_SIZE)];
        struct buf *bp = bread(bmap(ip, off / BLK_SIZE));
        m = MIN(n - tot, BLK_SIZE - off % BLK_SIZE);
        memcpy(dst, (uint8_t *) bp->dma_buf + off % BLK_SIZE, m);
        brelse(bp);
    }
    return tot;
}

// caller must hold sleeplock for callee
int64_t writei(struct inode *ip, uint8_t *src, uint64_t off, uint64_t n) {
    if(off > ip->size || off > UINT64_MAX - n) return -1;
    if(n > (N_DIRECT + N_INDIRECT) * BLK_SIZE - off) return -1;

    uint64_t m, tot;
    for(tot = 0; tot < n; tot += m, off += m, src += m) {
        // Block *b = &img[bmap(ip, off / BLK_SIZE)];
        struct buf *bp = bread(bmap(ip, off / BLK_SIZE));
        m = MIN(n - tot, BLK_SIZE - off % BLK_SIZE);
        memcpy((uint8_t *) bp->dma_buf + off % BLK_SIZE, src, m);
        bwrite(bp);
        brelse(bp);
    }

    if(n > 0 && off > ip->size) {
        ip->size = off;
        iupdate(ip);
    }

    return tot;
}

// Look for a certain named dir entry in a dir inode
// If found, set *poff to byte offset of the entry
struct inode* dir_lookup(struct inode *dp, char *name, uint64_t *out_poff) {
    if(dp->type != T_DIR)
        panic("dir_lookup: not dir");
    
    struct dir_ent de;
    for(uint64_t off = 0; off < dp->size; off += sizeof(de)) {
        int64_t bytes_read = readi(dp, (uint8_t *) &de, off, sizeof(de));
        if(bytes_read != sizeof(de))
            panic("dir_lookup: cant read");
        
        // empty?
        if(de.inode == 0) continue;

        if(strncmp(name, de.name, DIR_SIZE) != 0) continue;
        
        if(out_poff) *out_poff = off;
        uint64_t inum = de.inode;
        return iget(inum);
        // if we lookup ".", and if iget returns inode locked,
        // iget will never return because the inode lock is already hold within this call rn.
        // This is why iget returns the inode without sleeplocked
    }
    return 0;
}

// add a new dir entry to dir inode
int dir_link(struct inode *dp, char *name, uint64_t inum) {
    // check name collision
    struct inode *ip = dir_lookup(dp, name, 0);
    if(ip) {
        iput(ip);
        return -1;
    }

    // finding empty slot for new de
    struct dir_ent de;
    uint64_t off;
    for(off = 0; off < dp->size; off += sizeof(de)) {
        int64_t bytes_read = readi(dp, (uint8_t *) &de, off, sizeof(de));
        if(bytes_read != sizeof(de))
            panic("dir_link: cant read");
        if(de.inode == 0) break;
    }

    strncpy(de.name, name, DIR_SIZE);
    de.inode = inum;
    int64_t bytes_written = writei(dp, (uint8_t *) &de, off, sizeof(de));
    if(bytes_written != sizeof(de))
        panic("dir_link: cant write");
    
    return 0;
}

// Skip one element in the path
// name is out param
// path="/a//bb/ccc" -> name="a", return="bb/ccc"
// path="bb/ccc" -> name="bb", return="ccc"
static char* skip_elem(char *path, char *name) {
    while(*path == '/') path++; // skip all '/'
    if(*path == '\0') return 0;

    char *start = path;
    while(*path != '/' && *path != 0) path++; // go till the name end
    int len = path - start;
    if(len >= DIR_SIZE) {
        memcpy(name, start, DIR_SIZE);
    } else {
        memcpy(name, start, len);
        name[len] = 0;
    }

    while(*path == '/') path++; // skip all '/' to return clean path that doesn't have '/' in the head
    return path;
}

static struct inode* namex(char *path, int namei_parent, char *name) {
    struct inode *ip = (*path == '/') ? iget(ROOT_INODE) : idup(myproc()->cwd);

    while((path = skip_elem(path, name)) != 0) {
        ilock(ip);
        if(ip->type != T_DIR) {
            // not dir
            iunlock(ip);
            iput(ip);
            return 0;
        }

        if(namei_parent && *path == '\0') {
            // namei_parent means, we need to return the parent inode
            iunlock(ip);
            return ip;
        }
        
        struct inode *next;
        // set next to name's ip
        if((next = dir_lookup(ip, name, 0)) == 0) {
            // cannot find it
            iunlock(ip);
            iput(ip);
            return 0;
        }
        iunlock(ip);
        iput(ip);
        ip = next;
    }
    if(namei_parent) {
        iput(ip);
        return 0;
    }
    return ip;
}

// Change path to inode
struct inode* namei(char *path) {
    char name[DIR_SIZE];
    return namex(path, 0, name);
}

struct inode* namei_parent(char *path, char *name) {
    return namex(path, 1, name);
}

void test_fs_read(uint64_t arg) {
    struct inode *ip = namei("/test_file.txt");
    if(!ip)
        panic("test_fs_read: namei failed");
    
    ilock(ip);
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    int64_t n = readi(ip, buf, 0, sizeof(buf) - 1);
    if(n < 0)
        panic("test_fs_read: readi cannot read");
    
    serial_printf("test_fs_read: %s\n", buf);

    iunlock(ip);
    iput(ip);

    for(;;);
}

void test_fs_read2(uint64_t arg) {
    struct inode *ip = namei("/dev/console");
    if(ip == 0) panic("console lookup failed");
    ilock(ip);
    serial_printf("console type=%d major=%d minor=%d\n", ip->type, ip->major, ip->minor);
    iunlock(ip);
    iput(ip);
    for(;;);
}
