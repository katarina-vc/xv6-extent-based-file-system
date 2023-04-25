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
    // char buf[512];

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

   printf(1, "Printing out stat information for: %s\n", argv[1]);


   if(statObj.type == 1) {
       printf(1, "type: %s\n", "Directory");
       printf(1, "size: %d\n", statObj.size);
    } else if(statObj.type == 3) {
       printf(1, "type: %s\n", "Device");
       printf(1, "size: %d\n", statObj.size);
    } else if(statObj.type == 4) {
       printf(1, "type: %s\n", "Symlink");    
       printf(1, "size: %d\n", statObj.size);
    } else if(statObj.type == 5) {
        printf(1, "type: %s\n", "Extent File");    
        printf(1, "size: %d\n", statObj.size);

        for(int i = 0; i < statObj.numExtents; i++) {
            printf(1,"Extent No. %d Start Address: %d\n", i+1, statObj.extentz[i].startingAddress);
            printf(1,"Extent No. %d Length: %d\n", i+1, statObj.extentz[i].length);
        }
    } else if(statObj.type == 2){
        printf(1, "type: %s\n", "Pointer-based File");    
        printf(1, "size: %d\n", statObj.size);

        for(int i = 0; i < NDIRECT; i++) {
            printf(1,"Disk Sector Block Direct Pointer Addresses for Block No. %d: %d\n", i+1, statObj.addrs[i]);
        }
    }

    
    close(fileDescriptor);
    exit();
} // end main()
