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
    int fileDescriptor;
    struct stat statObj;
    char buf[512];

    // User program stat: can be called like this: stat pathname
    if((fileDescriptor = open(argv[1], O_RDONLY)) < 0) {
            printf(1, "stat failed: Cannot read file: %s\n", argv[1]);
            exit();
    }

    if(fstat(fileDescriptor, &statObj) < 0) {
            printf(1, "stat failed: Cannot get status information about: %s\n", argv[1]);
            close(fileDescriptor);
            exit();
    }

    // Get the status for the current file
    if(stat(buf, &statObj) < 0) {
        printf(1, "stat failed: Cannot get the status.\n");
        close(fileDescriptor);
        exit();
    }


    printf(1, "Printing out stat information for: %s\n", argv[1]);

    printf(1, "type: %d\n", statObj.type);
    printf(1, "size: %d\n", statObj.size);

    if(statObj.type == T_EXTENT) {
         /*printf(1,"Number of extents in file: %d\n", statObj.numExtents);

        for(int i = 0; i < statObj.numExtents; i++) {
            printf(1,"Extent No. %d Start Address: %d\n", statObj.extentz[i].startingAddress);
            printf(1,"Extent No. %d Length: %d\n", statObj.extentz[i].length);
        }*/
    } else {
        for(int i = 0; i < NDIRECT; i++) {
            printf(1,"Disk Sector Block Direct Pointer Addresses for Block No. %d: %d\n", i+1, statObj.addrs[i]);
        }
    }

    
    close(fileDescriptor);
    exit();
} // end main()
