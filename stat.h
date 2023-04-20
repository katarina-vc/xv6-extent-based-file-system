#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device
#define T_SYMLINK 4 // KC - Project 4 Symlink file type
#define T_EXTENT 5 // Project 4 - Part 4 

struct stat {
  short type;  // Type of file
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short nlink; // Number of links to file
  uint size;   // Size of file in bytes
  uint addrs[12]; // addresses of the inodez - NDIRECT + 1 = 12
  uint numExtents;
};
