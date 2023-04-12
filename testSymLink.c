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

char buf[512];

// start main()
int main(int argc, char *argv[]) {  
    char *target = "README";
    char *path = "symB";

    int symlinkTest = symlink(target, path);

    printf(1, "symlinking was successful: %d\n", symlinkTest);
  
    int fileDescriptor; 

    // Test "opening" the symlink.
    if((fileDescriptor = open(path, 0)) < 0){
        printf(2, "Symlink Test Error: cannot open %s\n", path);
        return -1;
    }

    // Upon success of opening symlink (which should actually be accessing our target if all went well), print out the file
    // Code below is from xv6's cat.c file.
    int n;
    while((n = read(fileDescriptor, buf, sizeof(buf))) > 0) {
        if (write(1, buf, n) != n) {
        printf(1, "test: write error\n");

        exit();
        }
    }
    if(n < 0){
        printf(1, "test: read error\n");
        exit();
    }

    close(fileDescriptor);
	exit();
} // end main()
