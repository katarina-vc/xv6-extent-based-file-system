/*
    Project 4 Part 4 - Create an extent file, open the extent file, and write to the extent file. 
*/
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
    // Text to periodically write to our new extent file:
    const char *fileContentWrite1 = "CONTENT 1 START: ............................................................................................................................................................................................................................................................................................................................................................................................................CONTENT 1 END.";
    const char *fileContentWrite2 = "CONTENT 2 START: ..............................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................CONTENT 2 END.";
    const char *fileContentWrite3 = "CONTENT 3 START: ..................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................CONTENT 3 END.";

    printf(1, "\nCreating an extent file called: %s\n", argv[1]);
    // **** Creating the extent file **************************************************************************************
    int fileDescriptor;
    if((fileDescriptor = open(argv[1], O_EXTENT)) < 0) {
            printf(1, "testExtent failed: Cannot create extent: %s\n", argv[1]);
            exit();
    }
    close(fileDescriptor);

    printf(1, "Writing first chunk of content to new extent file: %s\n", argv[1]);
    // **** Write file content number 1 to our extent. **********************************************************************
    int fileDescriptor2;
    if((fileDescriptor2 = open(argv[1], O_WRONLY)) < 0) {
            printf(1, "testExtent failed: Cannot open extent %s\n", argv[1]);
            exit();
    }


    printf(1, "\nwriting content 1. content 1 length: %d, size: %d\n", strlen(fileContentWrite1), sizeof(fileContentWrite1));
    if (write(fileDescriptor2, fileContentWrite1, strlen(fileContentWrite1)) != strlen(fileContentWrite1)) {
        printf(2, "testExtent failed: failed to write the content to the extent file.\n");
        close(fileDescriptor2);
        exit();
    }
    close(fileDescriptor2);

    printf(1, "Printing out extent file: %s\n\n", argv[1]);
    // **** Read and print out the contents of our extent file to prove we did it. ******************************************
    int fileDescriptor3;

    if((fileDescriptor3 = open(argv[1], O_RDONLY)) < 0) {
            printf(1, "testExtent failed: Cannot open extent file %s\n", argv[1]);
            exit();
    }
    
    char buf[512];

    // reading and printing code below from xv6's cat.c file.
    int n;
    while ((n = read(fileDescriptor3, buf, sizeof(buf))) > 0)
    {
        if (write(1, buf, n) != n)
        {
            printf(1, "testExtent failed: Cannot write out extent file.\n");
            exit();
        }
    }

    printf(1, "\n");
    close(fileDescriptor3);


    printf(1, "\nExtent file has been successfully created, written too, and read. Calling exec() on a child process to run the stat user program on our new extent file: %s\n\n", argv[1]);
    // **** Creating a child process to run exec() for our stat user program to print out info about our new extent file ******************************************
    int pid = fork();

    if (pid == 0) {
        // run the stat user program for our new file
        char *args[] = {"stat", argv[1], NULL}; // list of arguments for stat, the last arg must always be NULL
        exec(args[0], args);
        exit();
	}

    pid = wait();


    printf(1, "\n\nWriting second chunk of content to new extent file to show we can grow it: %s\n", argv[1]);
    // **** Write file content number 1 to our extent. **********************************************************************
    int fileDescriptor4;
    if((fileDescriptor4 = open(argv[1], O_WRONLY)) < 0) {
            printf(1, "testExtent failed: Cannot open extent %s\n", argv[1]);
            exit();
    }

    printf(1, "\nwriting content 2. content 2 length: %d, size: %d\n", strlen(fileContentWrite2), sizeof(fileContentWrite2));
    if (write(fileDescriptor4, fileContentWrite2, strlen(fileContentWrite2)) != strlen(fileContentWrite2)) {
        printf(2, "testExtent failed: failed to write the content to the extent file.\n");
        close(fileDescriptor4);
        exit();
    }
    close(fileDescriptor4);

    printf(1, "Printing out extent file: %s\n\n", argv[1]);
    // **** Read and print out the contents of our extent file to prove we did it. ******************************************
    int fileDescriptor5;

    if((fileDescriptor5 = open(argv[1], O_RDONLY)) < 0) {
            printf(1, "testExtent failed: Cannot open extent file %s\n", argv[1]);
            exit();
    }

    char buf2[512];

    // reading and printing code below from xv6's cat.c file.
    int numberBytesRead;
    while ((numberBytesRead = read(fileDescriptor5, buf2, sizeof(buf2))) > 0)
    { 
        // printf(1,"\n1 n2: %d", n2);
        if (write(1, buf2, numberBytesRead) != numberBytesRead)
        {
            printf(1, "testExtent failed: Cannot write out extent file.\n");
            exit();
        }
    }
            // printf(1,"\n2 n2: %d", n2);

    printf(1, "\n");
    close(fileDescriptor5);


    printf(1, "\n\nExtent file has been successfully extended, written, and read. Calling exec() on a child process to run the stat user program on our extent file to see updates: %s...\n", argv[1]);
    // **** Creating a child process to run exec() for our stat user program to print out info about our new extent file ******************************************pid = fork();

    int pid2 = fork();
    if (pid2 == 0) {
        // run the stat user program for our new file
        char *args[] = {"stat", argv[1], NULL}; // list of arguments for stat, the last arg must always be NULL
        exec(args[0], args);
        exit();
	}

    pid2 = wait();

     printf(1, "\n\nWriting third chunk of content to new extent file to show we can grow it: %s\n", argv[1]);
    // **** Write file content number 1 to our extent. **********************************************************************
    int fileDescriptorThree;
    if((fileDescriptorThree = open(argv[1], O_WRONLY)) < 0) {
            printf(1, "testExtent failed: Cannot open extent %s\n", argv[1]);
            exit();
    }

    printf(1, "\nwriting content 3. content 3 length: %d, size: %d\n", strlen(fileContentWrite3), sizeof(fileContentWrite3));
    if (write(fileDescriptorThree, fileContentWrite3, strlen(fileContentWrite3)) != strlen(fileContentWrite3)) {
        printf(2, "testExtent failed: failed to write the content to the extent file.\n");
        close(fileDescriptorThree);
        exit();
    }
    close(fileDescriptorThree);

    printf(1, "Printing out extent file: %s\n\n", argv[1]);
    // **** Read and print out the contents of our extent file to prove we did it. ******************************************
    int fileDescriptorThree2;

    if((fileDescriptorThree2 = open(argv[1], O_RDONLY)) < 0) {
            printf(1, "testExtent failed: Cannot open extent file %s\n", argv[1]);
            exit();
    }

    char buf3[512];

    // reading and printing code below from xv6's cat.c file.
    int numberBytesRead2;
    while ((numberBytesRead2 = read(fileDescriptorThree2, buf3, sizeof(buf3))) > 0)
    { 
        // printf(1,"\n1 n2: %d", n2);
        if (write(1, buf3, numberBytesRead2) != numberBytesRead2)
        {
            printf(1, "testExtent failed: Cannot write out extent file.\n");
            exit();
        }
    }
            // printf(1,"\n2 n2: %d", n2);

    printf(1, "\n");
    close(fileDescriptorThree2);


    printf(1, "\n\nExtent file has been successfully extended, written, and read again :). Calling exec() on a child process to run the stat user program on our extent file to see updates: %s...\n", argv[1]);
    int pid3 = fork();
    if (pid3 == 0) {
        // run the stat user program for our new file
        char *args[] = {"stat", argv[1], NULL}; // list of arguments for stat, the last arg must always be NULL
        exec(args[0], args);
        exit();
	}

    pid3 = wait();


    exit();
} // end main()
