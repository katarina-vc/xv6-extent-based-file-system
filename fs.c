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
 // checks if n is a negative number
 // if either are true. then return an error. 
 // If the requested range is out of bounds, it returns -1 (error). 
  if(off > ip->size || off + n < off) {
        return -1;
  }
   // If the requested range exceeds the file's size, it adjusts the number of bytes to read accordingly.
  if(off + n > ip->size) {
      n = ip->size - off;
  }

  if(ip->type == T_EXTENT) {
        int extentNumToReadFrom = -1; // the extent in our file's extent array we start reading based on the file offset
        int extentLengthNumToReadFrom = -1; // the length number we start reading based on the file's offset and extent number
        int extentAddressToReadFrom = -1; // the actual address we want to start reading from based on the extent, extent's length and file offset

        // get the address for our offset we want to start reading from
        int offsetAddress = bmap(ip, off/BSIZE);

        // loop through each extent in our file to find where the offset address matches a position in one our the file's extents
        for(int i = 0; i < ip->numExtents; i++) {
          int foundFlag = 0; // indicates we found the extent number and length position number where the offset lives
          int extentAddressRange = ip->extentz[i].startingAddress; // start searching at the beginning of each extent

            // loop through each address in the extent based off of the starting address and the length of that extent
            for(int j = 0; j < ip->extentz[i].length; j++) {
              // check if our file's offset address matches an addres in this extent
              if(extentAddressRange == offsetAddress) {
                // we found our matching address!
                extentNumToReadFrom = i;
                extentLengthNumToReadFrom = j;
                extentAddressToReadFrom = extentAddressRange;
                foundFlag = 1;
                break;
              } 

              extentAddressRange++;
            }

          if(foundFlag == 1){
            break;
          }
        }

        // now actually loop through the extent to read it out
        uint tot = 0;
        uint min1;
        uint m;
        for(int i = extentNumToReadFrom; i < ip->numExtents; i++) {
            // check that the total number of bytes read is not greater than or equal to the bytes requested. if so return n;
            if(tot > n) {
              // cprintf("\n READI() OUTER LOOP EXITING READI(): FINISHED READING FROM EXTENT NO: %d, TOT: %d, N: %d\n", i, tot, n);
              return n;
            }

            // Check if we are still reading from the original extent we started with
            // if not, then we need to grab the next extents starting address and set that
            // to continue copying bytes from
            if(i != extentNumToReadFrom) {
               extentAddressToReadFrom = ip->extentz[i].startingAddress;
               extentLengthNumToReadFrom = 0;
            }

            // cprintf("\nOUTER READ - READING FROM EXTENT NO: %d, STARTING ADDY NO: %d, LEGNTH: %d\n", i, extentAddressToReadFrom, extentLengthNumToReadFrom);

            // loop through each address in the extent and map the data to the destination buffer until we hit the total number of bytes read
            for(int j = extentLengthNumToReadFrom; j < ip->extentz[i].length; j++) {
                // check that the total number of bytes read is not greater than or equal to the bytes requested. if so return n;
                 if(tot > n) {
                  // cprintf("\n READI() INNER LOOP EXITING READI(): DIDN'T READ EXTENT NO: %d, BLOCK NO: %d, TOT: %d, N: %d\n", i, j, tot, n);
                  return n;
                }

                // cprintf("\nINNER READ - READING FROM EXTENT NO: %d, BLOCK NO: %d, ADDRESS WE R READING: %d\n", i, j, extentAddressToReadFrom);
                
                // read the extent's address
                bp = bread(ip->dev, extentAddressToReadFrom);
                // calculate the number of m bytes to copy
                min1 = min(strlen((char *)bp->data), BSIZE - off%BSIZE);
                m = min(n - tot, min1);

                // cprintf("\n INNER READ M CALC: n-tot: %d, bsize - off/bsize: %d, m = %d sizeof data: %d, strlen data: %d\n", n-tot, BSIZE - off%BSIZE, m, sizeof(bp->data), strlen((char *)bp->data));
                // copy the bytes to the destination buffer
                memmove(dst, bp->data, m);
                // cprintf("\ndst: %s\n", dst);
               //  cprintf("\n data we r copying: %s\n", bp->data + off%BSIZE);
                             //    cprintf("\n data we r copying 2: %s\n", bp->data);
                brelse(bp);

                // increment the extent address
                extentAddressToReadFrom++;
                tot+=m;
                off+=m;
                dst+=m;
            }
        } // end for loop
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
      //It calculates the number of bytes to read from the current block (m) using the min() function, 
      // which returns the smaller of two values.
        m = min(n - tot, BSIZE - off%BSIZE);
        // It copies the data from the buffer to the destination using the memmove() function.
        memmove(dst, bp->data + off%BSIZE, m);

        // After copying the data, the function releases the buffer used to store the
        // block data, allowing it to be reused by other operations.
        brelse(bp);
      }
  }

// Finally, the function returns the number of bytes read (n).

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
      cprintf("You have too many extents. Your extent file is too big and we cannot continue to write. Exiting...\n");
      exit();
    } else {
      ip->numExtents++;
    }

    struct extent iextent;
    int lengthCounter = 0;

    for(tot=0; tot<n; tot+=m, off+=m, src+=m){
      // get the starting address for the extent
      // cprintf("\n\n\nwritei tot: %d for extent no: %d\n", tot, ip->numExtents);
      if(tot == 0) {
        // check if this index node ip already has at least one extent
          iextent.startingAddress = bmap(ip, ip->sisterblocks);
                // cprintf("\nwritei tot == 0 start addy: %d for extent no: %d\n", iextent.startingAddress, ip->numExtents);
          bp = bread(ip->dev, iextent.startingAddress);
      } else {
        bp = bread(ip->dev, bmap(ip, ip->sisterblocks));
      }

      ip->sisterblocks++;
      lengthCounter++;

      // Copy data from the source buffer to the block buffer:
      m = min(n - tot, BSIZE - off%BSIZE);     // calculates the number of bytes to copy to the current block (m) and 
      memmove(bp->data + off%BSIZE, src, m);   // then copies the data from the source buffer (src) to the block buffer (bp->data).
      // cprintf("\n writei: data we are writing and m: %d  |||| %s\n",m, src);
      // cprintf("\n");
      log_write(bp); // Write the modified block to the disk:
           //  cprintf("\ndata that was writtennn..: %s\n", bp->data);

      brelse(bp); // release the block buffer
    } // end forloop

    iextent.length = lengthCounter;

      // cprintf("WRITEI EXTENT STRUCT: n: %d off: %d\n", n, off);
    if(n > 0){
      // cprintf("WRITEI EXTENT STRUCT: iextent size: %d iextent sA: %d\n", iextent.length, iextent.startingAddress);
      ip->extentz[ip->numExtents - 1] = iextent;
      // cprintf("1 WRITEI EXTENT STRUCT: ip extent size: %d ip extent sA: %d\n", ip->extentz[ip->numExtents - 1].length, ip->extentz[ip->numExtents - 1].startingAddress);
      ip->size += n;
      iupdate(ip);
      // cprintf("2 WRITEI EXTENT STRUCT: ip extent size: %d ip extent sA: %d\n", ip->extentz[ip->numExtents - 1].length, ip->extentz[ip->numExtents - 1].startingAddress);
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