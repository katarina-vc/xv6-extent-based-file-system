// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
static void itrunc(struct inode*);
// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb; 

// Read the super block.
void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// Zero a block.
static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// Blocks.

// Allocate a zeroed disk block.
static uint
balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  // read all the blocks on the superblock (the xv6 mega block of all blocks)
  for(b = 0; b < sb.size; b += BPB){
    bp = bread(dev, BBLOCK(b, sb));
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
      m = 1 << (bi % 8);
      if((bp->data[bi/8] & m) == 0){  // Is block free? if soo....
        bp->data[bi/8] |= m;  // Mark block in use.
        log_write(bp); // write the updated block map back to the physical disk
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi; // return the block number of the newly allocated blocky block
      }
    }
    brelse(bp);
  }
  panic("balloc: out of blocks");
}

// Free a disk block.
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;

  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
  log_write(bp);
  brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at
// sb.startinode. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a cache of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The cached
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in cache: an entry in the inode cache
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a cache entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   cache entry is only correct when ip->valid is 1.
//   ilock() reads the inode from
//   the disk and sets ip->valid, while iput() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays cached and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The icache.lock spin-lock protects the allocation of icache
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold icache.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} icache;

void
iinit(int dev)
{
  int i = 0;
  
  initlock(&icache.lock, "icache");
  for(i = 0; i < NINODE; i++) {
    initsleeplock(&icache.inode[i].lock, "inode");
  }

  readsb(dev, &sb);
  cprintf("sb: size %d nblocks %d ninodes %d nlog %d logstart %d\
 inodestart %d bmap start %d\n", sb.size, sb.nblocks,
          sb.ninodes, sb.nlog, sb.logstart, sb.inodestart,
          sb.bmapstart);
}

static struct inode* iget(uint dev, uint inum);

//PAGEBREAK!
// Allocate an inode on device dev.
// Mark it as allocated by  giving it type type.
// Returns an unlocked but allocated and referenced inode.
struct inode*
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for(inum = 1; inum < sb.ninodes; inum++){
    bp = bread(dev, IBLOCK(inum, sb));
    dip = (struct dinode*)bp->data + inum%IPB;
    if(dip->type == 0){  // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp);   // mark it allocated on the disk
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  panic("ialloc: no inodes");
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk, since i-node cache is write-through.
// Caller must hold ip->lock.
void
iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode*)bp->data + ip->inum%IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode*
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  acquire(&icache.lock);

  // Is the inode already cached?
  empty = 0;
  for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode cache entry.
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&icache.lock);

  return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode*
idup(struct inode *ip)
{
  acquire(&icache.lock);
  ip->ref++;
  release(&icache.lock);
  return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
ilock(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if(ip->valid == 0){
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if(ip->type == 0)
      panic("ilock: no type");
  }
}

// Unlock the given inode.
void
iunlock(struct inode *ip)
{
  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void
iput(struct inode *ip)
{
  acquiresleep(&ip->lock);
  if(ip->valid && ip->nlink == 0){
    acquire(&icache.lock);
    int r = ip->ref;
    release(&icache.lock);
    if(r == 1){
      // inode has no links and no other references: truncate and free.
      itrunc(ip);
      ip->type = 0;
      iupdate(ip);
      ip->valid = 0;
    }
  }
  releasesleep(&ip->lock);

  acquire(&icache.lock);
  ip->ref--;
  release(&icache.lock);
}

// Common idiom: unlock, then put.
void
iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

//PAGEBREAK!
// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
// KC - bmap maps a block number within a file to a physical block number on the disk
// KC - used during file read or write operatings to determine the correct disk location for a given file offset
// param 1 = pointer to inode, param 2 bn = logical/virtual block number within the file
// if bmap is called during a write operation and the block hasn't been allocated yet, then bmap allocates it and updates the inode block pointers
static uint
bmap(struct inode *ip, uint bn)
{
   // ip == index node of file we are checking out
   // bn == block number aka the virtual block number we are searching for its best buddy physical block number

  uint addr, *a;
  // short entry, offset;
  struct buf *bp;

  // Implementation of direct system
  // if the logical/virtual block number (bn) falls within the direct block pointers, bmap will
  // return the physical block number from the inodes direct block point pointer array (the dinodez)
  if(bn < NDIRECT){
    // NDIRECT is the number of direct pointer addresses xv6 allows which is like 11
    // the if statement right below here checks if this block has even been allocated on the physical disk yet.
    if((addr = ip->addrs[bn]) == 0) {
      ip->addrs[bn] = addr = balloc(ip->dev);     // if not, then we want to allocate it on the disk.
    }

    return addr; // after allocating bc it didnt exist, or just finding it, we return it.
  }
  bn -= NDIRECT;

  // Implementation of single indirect system
  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }
  bn -= NINDIRECT;

  // Implementation of double indirect system
  if(!(bn >= D_INDIRECT))
  {
    int n_bit=1;  // do not change, please
    if(!(addr = ip->addrs[NDIRECT + n_bit]))
      ip->addrs[NDIRECT + n_bit] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if(!(addr = a[bn / D_INDIRECT_INSTANCE]))
    {
      a[bn / D_INDIRECT_INSTANCE] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if(!(addr = a[bn % D_INDIRECT_INSTANCE]))
    {
      a[bn % D_INDIRECT_INSTANCE] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
static void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}

// Copy stat information from inode.
// Caller must hold ip->lock. stathere
void
stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;

  if(ip->type == T_EXTENT) {
      st->numExtents = ip->numExtents;
    for(int i = 0; i < ip->numExtents; i++){
        st->extentz[i].length = ip->extentz[i].length;
        st->extentz[i].startingAddress = ip->extentz[i].startingAddress;
    }
  } else {
    if(ip->type == T_FILE) {
      for(int i = 0; i < NDIRECT; i++){
        st->addrs[i] = ip->addrs[i];
      }
    }
  }
}

//PAGEBREAK!
// Read data from inode.
// Caller must hold ip->lock.
int readi(struct inode *ip, char *dst, uint off, uint n) {
  uint tot, m;
  struct buf *bp;

  if(ip->type == T_DEV){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read) {
      return -1;
    }
    return devsw[ip->major].read(ip, dst, n);
  }

 // checks if the requested read operation is within the file's boundaries. 
 // If the requested range is out of bounds, it returns -1 (error). 
  if(off > ip->size || off + n < off) {
        return -1;
  }
   // If the requested range exceeds the file's size, it adjusts the number of bytes to read accordingly.
  if(off + n > ip->size) {
      n = ip->size - off;
  }

  if(ip->type == T_EXTENT) {
       //  cprintf("\nss: %d\n", ip->sisterblocks);

        begin_op();
        if(ip->brotherblocks < ip->sisterblocks) {
          // cprintf("\n bb less than bb: %d\n", ip->brotherblocks);
          bp = bread(ip->dev, bmap(ip, ip->brotherblocks));
          // cprintf("\nbp less than bpdata: %s\n", bp->data);
          memmove(dst, bp->data, sizeof(bp->data));   // then copies the data from the source buffer (src) to the block buffer (bp->data).
          // cprintf("\ndst: %s\n", dst);
          brelse(bp); // release the block buffer

          ip->brotherblocks++;
          iupdate(ip);
          end_op();
          return 512;
        } else if(ip->brotherblocks == ip->sisterblocks) {
          // cprintf("\n bb == to bb: %d\n", ip->brotherblocks);
            ip->brotherblocks = 0; // reset we are done.
            iupdate(ip);
            end_op();
            return -1;
        } else if(ip->brotherblocks > ip->sisterblocks) {
            // cprintf(" \n HEREEE bb: %d\n", ip->brotherblocks);
            ip->brotherblocks = 0; // reset we are done.
            iupdate(ip);
            end_op();
            return -1;
        }


  } else { 
    // reads the data in a loop.
    // In each iteration, it reads a portion of the requested data and updates the total bytes read, 
    // the file offset, and the destination buffer pointer.
      for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
        // The function reads a block from the file using the bread() and bmap() functions. 
        // bmap() is responsible for translating the file's logical block number to the corresponding 
        // physical block number on the disk. bread() reads the physical block into a buffer
        bp = bread(ip->dev, bmap(ip, off/BSIZE));

        // calculates the number of bytes to copy from the current block (m) and then copies 
        // the data from the block buffer (bp->data) to the destination buffer (dst).
        m = min(n - tot, BSIZE - off%BSIZE);
        memmove(dst, bp->data + off%BSIZE, m);

        // After copying the data, the function releases the buffer used to store the
        // block data, allowing it to be reused by other operations.
        brelse(bp);
      }
  }

  return n;
}

// PAGEBREAK!
// Write data to inode.
// Caller must hold ip->lock.
int
writei(struct inode *ip, char *src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(ip->type == T_DEV) {
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write) {
      cprintf("Here\n\n :(\n\n)");
      return -1;
    }
    return devsw[ip->major].write(ip, src, n);
  }

// checks if the requested write operation is within the allowed file size boundaries. 
// If the requested range is out of bounds, it returns -1 (error).
  if(off > ip->size || off + n < off) {
    cprintf("offset es 2 big ;(\n");
    return -1;
  }
  if(off + n > MAXFILE*BSIZE) {
    cprintf("offset es outta boundss\n");
    return -1;
  }

// Project 4 Part 4 things.
// writes the data in a loop. In each iteration, it writes a portion of the requested data and updates 
// the total bytes written, the file offset, and the source buffer pointer.
  if(ip->type == T_EXTENT) {
    // Check we havent made too made extents
    if(ip->numExtents > NDIRECT) {
      cprintf("You have too many extents. File is too big. Exiting...\n");
      exit();
    } else {
      ip->numExtents++;
    }

    struct extent iextent;
    int lengthCounter = 0;

    for(tot=0; tot<n; tot+=m, off+=m, src+=m){
      // get the starting address for the extent
      if(tot == 0) {
        // check if this index node ip already has at least one extent
          iextent.startingAddress = bmap(ip, ip->sisterblocks);
          bp = bread(ip->dev, iextent.startingAddress);
      } else {
        bp = bread(ip->dev, bmap(ip, ip->sisterblocks));
      }

      ip->sisterblocks++;
      lengthCounter++;
      ip->brotherblocks = 0;

      // Copy data from the source buffer to the block buffer:
      m = min(n - tot, BSIZE - off%BSIZE);     // calculates the number of bytes to copy to the current block (m) and 
      memmove(bp->data + off%BSIZE, src, m);   // then copies the data from the source buffer (src) to the block buffer (bp->data).
      log_write(bp); // Write the modified block to the disk:
      brelse(bp); // release the block buffer
    } // end forloop

    iextent.length = lengthCounter;

    if(n > 0 && off > ip->size){
      ip->extentz[ip->numExtents - 1] = iextent;
      ip->size = off;
      iupdate(ip);
    }
  } else { // ************************************************************** end T_EXTENT check
      for(tot=0; tot<n; tot+=m, off+=m, src+=m){
        // Read a file block:
        // The function reads a block from the file using the bread() and bmap() functions. 
        // bmap() is responsible for translating the file's logical block number to the corresponding 
        // physical block number on the disk. bread() reads the physical block into a buffer.
        bp = bread(ip->dev, bmap(ip, off/BSIZE));

        // Copy data from the source buffer to the block buffer:
        m = min(n - tot, BSIZE - off%BSIZE);     // calculates the number of bytes to copy to the current block (m) and 
        memmove(bp->data + off%BSIZE, src, m);   // then copies the data from the source buffer (src) to the block buffer (bp->data).

        log_write(bp); // Write the modified block to the disk:
        brelse(bp); // release the block buffer
      }

    // Update the inode size and metadata:
    /*
      If any data was written and the new file offset is greater than the inode's current size, 
      the function updates the inode's size to reflect the new file size. It then calls 
      iupdate() to write the updated inode metadata to the disk.
    */
      if(n > 0 && off > ip->size){
        ip->size = off;
        iupdate(ip);
      }
  }

  return n; // Return the number of bytes written
}

//PAGEBREAK!
// Directories

int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if(de.inum == 0)
      continue;
    if(namecmp(name, de.name) == 0){
      // entry matches path element
      if(poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int
dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  if((ip = dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");

  return 0;
}

//PAGEBREAK!
// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  if(*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd);

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(ip->type != T_DIR){
      iunlockput(ip);
      return 0;
    }
    if(nameiparent && *path == '\0'){
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    if((next = dirlookup(ip, name, 0)) == 0){
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode*
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}