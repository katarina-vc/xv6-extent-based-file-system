// KC - 4/9/2023 - User Program to test Project 4 Part 2 - Adding Symlink System calls
/*
Testing steps to verify that the symlink is working:

As a user, follow these steps
1. Create a folder:
    mkdir a
2. Create a file in that folder:
    echo > a/b
3. Run this user program "testSymLink". The "testSymLink" user program takes two args, the target and the path, to be passed to our symlink system call.
    testSymLink a/b symB
In this example, our target is the file "b", so send it's path "a/b" as the first argument. Thes second argument is the name we are giving our symlink and how we
will reference the symlink. 

The user program, testSymLink, will then create the symlink, and perform various tests to prove it is working correctly. 

1. Create the symlink. 
2. Open the file.
3. Write to the file.
4. Read the file.
4. Close the file. 
5. link and unlink the symlink
*/

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "stddef.h"
#include "syscall.h"

// start main()
int main(int argc, char *argv[]) {
 
    char *target = "README";
    char *path = "symB";
    struct stat statObj; 
    char bufferStr[512];

    int symlinksucc = symlink(target, path);

    printf(1, "symlinking was successful: %d\n", symlinksucc);

    int fd = open(path, 0);
        if (fd < 0) {
            printf(2, "Cannot open :((((())))) %s\n", path);
            exit();
        }

            // Get the status information for the starting point folder path
    if(fstat(fd, &statObj) < 0) {
            printf(1, "find failed: Cannot get status information about: %s\n", path);
            close(fd);
            exit();
    }
    if (stat(bufferStr, &statObj) < 0) {
      printf(2, "find failed: Cannot get the status of %s\n", bufferStr);
                  close(fd);
            exit();
    }

    printf(1, "bufferStr: %s\n", bufferStr);

        close(fd);

	exit();
} // end main()
