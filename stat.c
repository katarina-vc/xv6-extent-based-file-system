#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "stddef.h"
#include "syscall.h"
#include "fcntl.h"

// start main()
int main(int argc, char *argv[])
{
    int fd;
    char buf[512];
    struct stat statObj;

    // User program stat: can be called like this: stat pathname
    if((fd = open(argv[1], 0)) < 0) {
            printf(1, "stat failed: Cannot open %s\n", argv[1]);
            exit();
    }

    /*
        The stat user program should print out all the information 
        about a file, including its type, size, and information about 
        its extents (or direct pointers, if it's a pointer-based file)
    */

    // Get the status information for the starting point folder path
    if(fstat(fd, &statObj) < 0) {
            printf(1, "stat failed: Cannot get status information about: %s\n", argv[1]);
            close(fd);
            exit();
    }

    // Get the status for the current file
    if(stat(buf, &statObj) < 0) {
        printf(1, "stat failed: Cannot get the status.\n");
    }

    printf(1, "stat stuff type: %d\n", statObj.type);
    printf(1, "stat stuff size: %d\n", statObj.size);
    printf(1, "stat stuff dev: %d\n", statObj.dev);
    printf(1, "stat stuff ino: %d\n", statObj.ino);    
    printf(1, "stat stuff nlink: %d\n", statObj.nlink);

    exit();
} // end main()
