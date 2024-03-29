struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, SYMLINK } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;
};

struct extent {
  uint length; 
  uint startingAddress; 
};

// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+2];  //Changed to allow for a double indirect block
  struct extent extentz[NDIRECT];
  uint numExtents;
  int sisterblocks; // for continuous allocation and frag. reduction
  int eOffset; 
  int lOffset;
};



// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE 1


