struct symLinkFile {
  int ref; // reference count
  struct inode *ip;
  char readable;
  char writable;
  uint off;
};
