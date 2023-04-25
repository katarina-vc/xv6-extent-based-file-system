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


// JTM - Implement system call for lseek
int
sys_lseek(void){
	// Variable initialization
	int fileDescriptor;
	int offset;
	struct file *filePointer;
	
	// Verify that the arguments are good.
	if(argint(0, &fileDescriptor) < 0 || argint(1, &offset) < 0){
		return -1;
	}

	// Verify we have a valid file descriptor for the current process
	if((filePointer = myproc()->ofile[fileDescriptor]) == 0){
		return -1;
	}

	// Add the offset to the current offset
	filePointer->off += offset;

	// Return the new offset
	return filePointer->off;
}


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

int sys_link(void) {
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
int sys_unlink(void) {
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

// Project 4 - Part 2 - Stuff n thingz.
int openSymLink(char* path, int omode, struct inode* ip, int depth) {
  struct inode *indexNodeTarget;
  struct file *targetFile;
  int fd;

  // If the O_NOFOLLOW Flag is specified, open the symlink file, not the symlink's target
  if(omode & O_NOFOLLOW) {
     omode = O_RDONLY; // change OMODE to O_RDONLY so we can read the symlink

     indexNodeTarget = ip;
        
      iunlock(ip); // we no longer need the symlink's index node.

      if((targetFile = filealloc()) == 0 || (fd = fdalloc(targetFile)) < 0) {
          // Something went wrong.
          cprintf("Symlink O_NOFOLLOW file open error: Something went wrong opening the target file.");

          if(targetFile) {
            fileclose(targetFile);
          }

          iunlockput(indexNodeTarget);
          end_op();
          return -1;
      }

      end_op();

      targetFile->type = SYMLINK;
      targetFile->ip = indexNodeTarget;
      targetFile->off = 0;
      targetFile->readable = !(omode & O_WRONLY);
      targetFile->writable = (omode & O_WRONLY) || (omode & O_RDWR);

      return fd;
  } else {
      if((indexNodeTarget = namei((char*)ip->addrs)) == 0) {
          // Something went wrong. Project 4 Requirements: If the symlink's target file does not exist, then open fails. 
          cprintf("Symlink open error: Something went wrong finding the symlink target. Target file may not exist.");
	  iunlock(ip); // we no longer need the symlink's index node.
          end_op();
          return -1;
      }

       if(depth == 10) {
          cprintf("Symlink Error: Symlink cycle depth of 10 reached. Ending operation.\n");
          iunlock(ip); // we no longer need the symlink's index node.
	  end_op();
          return -1;
        }

      // Check if the file we opened is just another symlink, if so then recursively follow
      // until we get to an actual target file (and not a symlink file) and then open that (or fail if it doesnt exist)
      if (indexNodeTarget->type == T_SYMLINK) {
          depth++;
          iunlock(ip); // we no longer need the symlink's index node.
          ilock(indexNodeTarget);
          fd = openSymLink(path, omode, indexNodeTarget, depth);
          return fd;
      } else {
        depth = 0;
      }

      iunlock(ip); // we no longer need the symlink's index node.

      if((targetFile = filealloc()) == 0 || (fd = fdalloc(targetFile)) < 0) {
          // Something went wrong.
          cprintf("Symlink file open error: Something went wrong opening the target file.");

          if(targetFile) {
            fileclose(targetFile);
          }

          iunlockput(indexNodeTarget);
          end_op();
          return -1;
      }

      end_op();

      targetFile->type = FD_INODE;
      targetFile->ip = indexNodeTarget;
      targetFile->off = 0;
      targetFile->readable = !(omode & O_WRONLY);
      targetFile->writable = (omode & O_WRONLY) || (omode & O_RDWR);

      return fd;
  } // end if-else O_NOFOLLOW
} // end openSymLink()

// Project 4 - Part 2 - Create the symlink.
void createSymLink(char *path, char *target, struct inode *symLinkIndexNode) {
    struct file *symLinkFile;

    symLinkFile = filealloc();
    memmove((char*)symLinkIndexNode->addrs, target, DIRSIZ);
    iunlock(symLinkIndexNode);

    symLinkFile->ip = symLinkIndexNode;
    symLinkFile->type = SYMLINK;
    symLinkFile->off = 0;
} // end createSymLink()

static struct inode* create(char *path, short type, short major, short minor, char *target) {
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0) { 
    return 0;
  }
   
  ilock(dp); 

  if((ip = dirlookup(dp, name, 0)) != 0) {

    iunlockput(dp); // releases lock on the dp inode and also free it if its no longer in use
    ilock(ip); // lock the ip inode

    if(type == T_FILE && ip->type == T_FILE) {
      return ip; // return the inode for a file if it already exists
    }
    iunlockput(ip); // free and release the inode 
    return 0; // leave
  }

  if((ip = ialloc(dp->dev, type)) == 0) {
      panic("create: ialloc");
  }


  ilock(ip);

  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;

  iupdate(ip); 

 // creates a directory specific inode
  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots"); 
  }

  if(dirlink(dp, name, ip->inum) < 0) {
        panic("create: dirlink");
  }

  iunlockput(dp); // free and release the inode

  // Project 4 - Part 2: Check if type if a symlink, if so, create a symlink
  if(type == T_SYMLINK) {
     createSymLink(path, target, ip);
  }

  return ip;
} // end create()

int sys_open(void) {
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;
  
  if(argstr(0, &path) < 0 || argint(1, &omode) < 0) {
    return -1;
  }

  begin_op();

  if(omode & O_CREATE) {
        ip = create(path, T_FILE, 0, 0, NULL);
        if(ip == 0){
          end_op();
          return -1;
        }
  } else if (omode & O_EXTENT) { // Project 4 Part 4 - The O_EXTENT flag indicates we want to create an extent file
    ip = create(path, T_EXTENT, 0, 0, NULL);
    
    if(ip == 0){
      end_op();
      return -1;
    }
    
    ip->numExtents = 0;
    ip->sisterblocks = 0;
    ip->eOffset = 0;
    ip->lOffset = 0;

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
  } // end if-else omode create

  // Check for SymLinks, if so then we open the symLinkFile, and then find it's target file, and then actually open that target file.
  if (ip->type == T_SYMLINK) {
      fd = openSymLink(path, omode, ip, 0);
      return fd;
  }

  if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
    if(f) {
      fileclose(f);
    }

    iunlockput(ip);
    end_op();
    return -1;
  }
  ip->eOffset = 0;
  ip->lOffset = 0;
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  return fd;
} // end sys_open()

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
int sys_symlink(void) {
  char *path; // symlink's "nickname"
  char *target; // the actual path to the target file the symlink accesses

  // Get target and path symlink arguments
  if (argstr(0, &target) < 0 || argstr(1, &path) < 0) {
    cprintf("\nSymlink Error: Something went wrong with the symlink() arguments.\n");
    return -1; // return -1 on failure. 
  }

  // Since we're dealing with files, make sure the path/target is less than 14 characters (aka the xv6 DIRSIZ)
  if(strlen(target) > DIRSIZ || strlen(path) > DIRSIZ) {
    cprintf("\nSymlink Error: Please make your path and/or target less than 14 characters.\n");
    return -1; // return -1 on failure.
  }

  // Create the symlink's index node, this must be done within a transaction or xv6 will throw an error >:0.
  begin_op();
    create(path, T_SYMLINK, 0, 0, target);
  end_op();
  
  return 0; // return 0 on success woot woot!
}
