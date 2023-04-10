//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "stddef.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

char*
strcpy2(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}


// Create the path new as a link to the same inode as old.
/*
KC Project 4 - Part 2 - Just adding some notes
Other system calls (e.g., link and unlink) must not follow symbolic links; 
these system calls operate on the symbolic link itself.
*/
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
/*
KC Project 4 - Part 2 - Just adding some notes
Other system calls (e.g., link and unlink) must not follow symbolic links; 
these system calls operate on the symbolic link itself.
*/
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor, char *symLinkTargetPath)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  // DELETEME: nameiparent() looks up a path in the fiel system and returns a pointer to the inode structure for the directory containing the path
  // this function is often used to find the parent directory of a file/directory being created or renamed
  // the path arg is the path to the file or directory being looked up, the "name" arg is the buffer that will contain the file or directories name
  // so here we get the name of the parent folder from our path
  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp); // acqiure the lock on the parent directory inode so we can modify the inode's data

 // This whole block checks to see if the inode we are creating lowkey already exists
 // deleteme: looks up a file name in a directory and returns a pointer to the inode structure for the file if it exists, if the file doesnt exist then null is returned
 // dp is the directory to search, name is the file to lookup, 0 (poff) will be set to the files directory entry within the directory 
 // look up the file name in the directory
  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp); // releases lock on the dp inode and also free it if its no longer in use
    ilock(ip); // lock the ip inode
    if(type == T_FILE && ip->type == T_FILE)
      return ip; // return the inode for a file if it already exists
    iunlockput(ip); // free and release the inode 
    return 0; // leave
  }

  // if the file we are trying to create doesnt already exists, then lets allocate that bih
  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");


  ilock(ip);
  // check if type if a symlink
  if(type == T_SYMLINK) {
    cprintf("\nwe have a symlink!\n");
  }
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip); // update writes an inode structure and its data blocks to disk

 // creates a directory specific inode
  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  // creates a new directory entry for a file or new directory within a directory
  // dp is the inode for the parent directory that this new thingy thing will exist within
  // name is the name of the new file or directory
  // inum is the inode number of the new file or directory
  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp); // free and release the inode

  return ip;
}

/*
KC - Project 4 - Part 2 - Just adding notes 
Modify the open system call to handle the case where the path refers to a symbolic link. 
If the file does not exist, open must fail. When a process specifies O_NOFOLLOW in the flags 
to open, open should open the symlink (and not follow the symbolic link).

If the linked file is also a symbolic link, you must recursively follow it until a non-link file 
is reached. If the links form a cycle, you must return an error code. You may approximate this 
by returning an error code if the depth of links reaches some threshold (e.g., 10).
*/
int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;
  char targetBuf[DIRSIZ];
  
  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();
  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0, NULL);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }

    // deleteme modifyme strt
    if (ip->type == T_SYMLINK) {
      // lock the inode
      cprintf("\npath things: %s\n", path);

      // get the target path from the inode
      // readi() reads data from an inode structure
      // the ip is a pointer to the inode structure being read
      // dst is the a pointer to the buffer where data will be stored
      // off is the offset within the file to start reading at
      // and n is the number of bytes to read
      readi(ip, targetBuf, 0, 7);
      cprintf("\ntargetBuf things: %s\n", targetBuf);
      path = "/0";
      strcpy2(path, targetBuf);
      cprintf("\npath thing 2s: %s\n", path);
      iunlockput(ip); // releases and free the lock on ip
  }
// deleteme modifyme end
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();
  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0, NULL)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor, NULL)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

// KC - Project 4 SymLink System Call implementation
/*
Implement the symlink(target, path) system call to create a new symbolic link at path that refers to target. 
Note that target does not need to exist for the system call to succeed. You will need to choose somewhere to 
store the target path of a symbolic link, for example, in the inode's data blocks. symlink should return an 
integer representing success (0) or failure (-1) similar to link and unlink.
*/
int
sys_symlink(void)
{
  // The symlink system call takes in two args, char* target, and char* path. 
  // target: the name/location of the file we are trying to link (we are not handling symlink for directories) (this should be
  // the name of the file we are trying to link or path to that file we are trying to link)
  // path: the symlinks nickname (aka pathname) (aka new pathname)
  // essentially when given the path (nickname), we go look up the actual file at the target
  char *path;
  char *target;

  // get the argz
  if(argstr(0, &target) < 0 || argstr(1, &path) < 0) {
    return -1; // symlink returns -1 for failure
  }

  cprintf("\ntarget: %s\n", target);
  cprintf("path: %s\n", path);

  // Add transaction statements
  begin_op();

  struct inode *ip = create(path, T_SYMLINK, 0, 0, NULL);
  // writei() writes data to an inode structure
  // the ip arg is the inode structure being written to
  // buf is the pointer to the buffer containing the data to write
  // uint off is the offset within the file to start writing
  // and n is the number of bytes to write
  // writei(ip,&len, 0, sizeof(int));
  writei(ip, target, 0, strlen(target));
  iunlockput(ip);
  end_op();
  // symlink returns 0 for success
  return 0;
}