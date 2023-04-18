// KC - 4/9/2023 - User Program to test Project 4 Part 2 - Adding Symlink System calls
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "stddef.h"
#include "syscall.h"
#include "fcntl.h"

// Test 1: Create a symlink to an existing file. Prove the symlink works by opening and printing out the symlink. (no flags)
// We will be creating a symlink to the README file, and then proving it works.
void symLinkTest1(char *target, char *path)
{
    char buf[512];

    printf(1, "\nSymlink Test 1: Prove that the symlink works in general.\n");
    printf(1, "Symlink Test 1: To prove that symlink() works, we will create a symlink called symA to the README file. We will then open symA, and print out its contents.. which should print out the README file contents. Running Test 1...\n\n");

    int symlinkTest = symlink(target, path);

    if (symlinkTest < 0)
    {
        printf(1, "Symlink Test 1 Error: Failed to create symlink.");
    }

    int fileDescriptor;

    // Test "opening" the symlink. Read only flag specified.
    if ((fileDescriptor = open(path, O_RDONLY)) < 0)
    {
        printf(2, "Symlink Test 1 Error: cannot open %s\n", path);
        exit();
    }

    // Upon success of opening symlink (which should actually be accessing our target if all went well), print out the file.
    // Code below is from xv6's cat.c file.
    int n;
    while ((n = read(fileDescriptor, buf, sizeof(buf))) > 0)
    {
        if (write(1, buf, n) != n)
        {
            printf(1, "Symlink Test 1 Error: Cannot write out file.\n");
            exit();
        }
    }
    if (n < 0)
    {
        printf(1, "Symlink Test 1 Error: Something went wrong :(.\n");
        exit();
    }

    printf(1, "\n\nSymlink Test 1 passed! :)\n");
    close(fileDescriptor);
} // end symLinkTest1()

// Test 2: Project 4 Part 2 Requirement Demo: "Target does not need to exist for symlink to succeed, but if it does not, then open must fail."
// In this test we are creating a symlink to a non-existant file and then attempting to open it.
void symLinkTest2(char *target, char *path)
{
    printf(1, "\n\nSymlink Test 2: Prove that we can create a symlink to a non-existant file. Prove that if we try to open and read our symlink, it will fail as the target does not exist. \n");

    int symlinkTest = symlink(target, path);

    if (symlinkTest < 0)
    {
        printf(1, "\nSymlink Test 2 Error: Failed to create symlink.");
        exit();
    }

    int fileDescriptor;

    // Test "opening" the symlink. Read only flag specified.
    if ((fileDescriptor = open(path, O_RDONLY)) < 0)
    {
        printf(2, "\nSymlink Test 2 Error: cannot open file: %s.\n\n Test 2 has passed! :)\n\n", path);
    }

    close(fileDescriptor);
} // end symLinkTest2()

// Test 3: Project 4 Part 2 Requirement Demo: "O_NOFOLLOW flags should open the symlink (and not follow the symbolic link)."
void symLinkTest3(char *target, char *path)
{
    char buf[512];

    printf(1, "\nSymlink Test 3: Open a symlink using the O_NOFOLLOW flag. We will open a symlink called symB that targets the README file. Opening symB with the O_NOFOLLOW flag should print out the symB's contents.. which is README\n\n");

    int symlinkTest = symlink(target, path);

    if (symlinkTest < 0)
    {
        printf(1, "Symlink Test 3 Error: Failed to create symlink. Exiting...");
        exit();
    }

    int fileDescriptor;

    // Open the symlink. O_NOFOLLOW only flag specified.
    if ((fileDescriptor = open(path, O_NOFOLLOW)) < 0)
    {
        printf(2, "Symlink Test 3 Error: cannot open %s\n", path);
        exit();
    }

    printf(1, "Symlink Test 3: Successfully opened symlink using O_NOFOLLOW. Printing out symlink file...\n\n");

    // Upon success of opening symlink, print out the symlink file.
    // Code below is from xv6's cat.c file.
    int n;
    while ((n = read(fileDescriptor, buf, sizeof(buf))) > 0)
    {
        if (write(1, buf, n) != n)
        {
            printf(1, "Symlink Test 3 Error: Cannot write out file.\n");
            exit();
        }
    }
    if (n < 0)
    {
        printf(1, "Symlink Test 3 Error: Something went wrong :(.\n");
        exit();
    }

    printf(1, "\nSymlink Test 3 passed! :)\n");
    close(fileDescriptor);
} // end symLinkTest3()

// Test 4: Project 4 - Part 2 Requirement: Recursively follow symlinks until a non-link file is reached. If links form a cycle and depth of
// 10 is reached, then print out an error about this.
void symLinkTest4(char *target) {
    char buf[512];

    printf(1, "\nSymlink Test 4: Recursively follow symlinks to symlinks until non-symlink file is reached.\n");
    printf(1, "\nSymlink Test 4: Make up to 10 symlinks to prove we can do it first and that they recursively follow each other if we open them.\n");

    if (symlink(target, "sym1") < 0) {
        printf(1, "Symlink Test 4 Error: Failed to create symlink 1.");
        exit();
    }

    if (symlink("sym1", "sym2") < 0) {
        printf(1, "Symlink Test 4 Error: Failed to create symlink 1.");
        exit();
    }

    if (symlink("sym2", "sym3") < 0) {
        printf(1, "Symlink Test 4 Error: Failed to create symlink 1.");
        exit();
    }

    if (symlink("sym3", "sym4") < 0) {
        printf(1, "Symlink Test 4 Error: Failed to create symlink 1.");
        exit();
    }

    if (symlink("sym4", "sym5") < 0) {
        printf(1, "Symlink Test 4 Error: Failed to create symlink 1.");
        exit();
    }

    if (symlink("sym5", "sym6") < 0) {
        printf(1, "Symlink Test 4 Error: Failed to create symlink 1.");
        exit();
    }

    if (symlink("sym6", "sym7") < 0) {
        printf(1, "Symlink Test 4 Error: Failed to create symlink 1.");
        exit();
    }

    if (symlink("sym7", "sym8") < 0) {
        printf(1, "Symlink Test 4 Error: Failed to create symlink 1.");
        exit();
    }

     if (symlink("sym8", "sym9") < 0) {
        printf(1, "Symlink Test 4 Error: Failed to create symlink 1.");
        exit();
    }

     if (symlink("sym9", "sym10") < 0) {
        printf(1, "Symlink Test 4 Error: Failed to create symlink 1.");
        exit();
    } 

    int fileDescriptor; 

    // Open the top symlink, this should recursively follow the symlinks and eventually open the "target" file.
    if ((fileDescriptor = open("sym10", O_RDONLY)) < 0) {
        printf(2, "Symlink Test 4 Error: cannot open %s\n", target);
        exit();
    }

    printf(1, "Symlink Test 4: Successfully opened the target file. Printing out target file...\n\n");

    // Upon success of opening symlink, print out the symlink file.
    // Code below is from xv6's cat.c file.
    int n;
    while ((n = read(fileDescriptor, buf, sizeof(buf))) > 0)
    {
        if (write(1, buf, n) != n)
        {
            printf(1, "Symlink Test 4 Error: Cannot write out file.\n");
            exit();
        }
    }
    if (n < 0)
    {
        printf(1, "Symlink Test 4 Error: Something went wrong :(.\n");
        exit();
    }

    printf(1, "\n\nSymlink Test 4 passed! :)\n");
    close(fileDescriptor);
} // end symLinkTest4()

// Test 5: Demonstrate that if we are in a recursive symlink loop, and a depth of 10 is already reached, we print out an error and stop.
void symLinkTest5() {
    // add an 11th symlink to recursively call the symlinks from symLinkTest4.
    printf(1, "\n\nSymlink Test 5: Demonstrate a depth of 10 symlinks to recursively follow to throw an error. This test is successful upon printing out that error.\n");

    if (symlink("sym10", "sym11") < 0) {
        printf(1, "Symlink Test 5 Error: Failed to create symlink.");
        exit();
    }

    int fileDescriptor; 

    // Open the top symlink, this should recursively follow the symlinks and eventually open the "target" file.
    if ((fileDescriptor = open("sym11", O_RDONLY)) < 0) {
        printf(2, "Symlink Test 5 Error: cannot open file.\n\nTest 5 has passed :), this file should not be able to open since we have reached our symlink depth limit.\n");
        close(fileDescriptor);
    }
} // end symLinkTest5()

// Symlink Test 6 proves that system calls like link and unlink operate on the symbolic link and do not follow the symbolic link
void symLinkTest6() {
    int fileDescriptor; 
    char buf[512];

    printf(1, "\n\nSymlink Test 6: Prove that system calls like link and unlink operate on the symbolic link itself.\n");
    printf(1, "Symlink Test 6: Creating a link called hardlink to symlink symA\n");

    // Create the hardlink to our symlink "symA" whose target is the README file.
    if(link("symA", "hardlink") < 0) {
        printf(2, "Symlink Test 6: Link failed\n");
    }

    printf(1, "Successfully created the hard link to our symlink...\n");

    if ((fileDescriptor = open("hardlink", O_NOFOLLOW)) < 0) {
        printf(2, "Symlink Test 6 Error: cannot open file.\n");
        exit();
    }
    printf(1, "Successfully opened the hard link... now reading the hard link ... if successful this should print out the symbolic links target file path: README\n");

    // Upon success of opening symlink, print out the symlink file.
    // Code below is from xv6's cat.c file.
    int n;
    while ((n = read(fileDescriptor, buf, sizeof(buf))) > 0)
    {
        if (write(1, buf, n) != n)
        {
            printf(1, "Symlink Test 6 Error: Cannot write out file.\n");
            exit();
        }
    }
    if (n < 0)
    {
        printf(1, "Symlink Test 6 Error: Something went wrong :(.\n");
        exit();
    }

    printf(1, "\nSymlink Test 6 Part 1 has passed! We were able to successfully link to a symlink and open that hard link.\n");

    printf(1, "\nTest 6 Part 2: Now we will unlink our new hard link and attempt to open and read again. This part of the test should fail.\n");
    
    unlink("hardlink");

    int fileDescriptor2;
         // Open the top symlink, this should recursively follow the symlinks and eventually open the "target" file.
    if ((fileDescriptor2 = open("testLink", O_NOFOLLOW)) < 0) {
        printf(2, "Symlink Test 6 Error: cannot open file.\n\nTest 6 Part 2 has passed :). This is a success as we have unlinked this hard link and should not be able to open it. Test 6 Part 2 Passed.\n");
    }

    close(fileDescriptor);
    close(fileDescriptor2);
}// end symLinkTest6()

// start main()
int main(int argc, char *argv[])
{

    // Test 1: Create a symlink to an existing file. Prove the symlink works by opening and printing out the symlink. (no flags)
    // We will be creating a symlink to the README file, and then proving it works.
    printf(1, "\n\n---------------------------------------------------------------------------------------------------------------\n");
    symLinkTest1("README", "symA");

    // Test 2: Project 4 Part 2 Requirement Demo: "Target does not need to exist for symlink to succeed, but if it does not, then open must fail."
    // In this test we are creating a symlink to a non-existant file and then attempting to open it.
    printf(1, "\n\n---------------------------------------------------------------------------------------------------------------\n");
    symLinkTest2("DEADFILE", "symB");
    unlink("symB"); // clean up

    // Test 3: Project 4 Part 2 Requirement Demo: "O_NOFOLLOW flags should open the symlink (and not follow the symbolic link)."
    printf(1, "\n\n---------------------------------------------------------------------------------------------------------------\n");
    symLinkTest3("README", "symC");
    unlink("symC"); // clean up

    // Test 4: Project 4 - Part 2 Requirement: Recursively follow symlinks until a non-link file is reached. If links form a cycle and depth of
    // 10 is reached, then print out an error about this.
    printf(1, "\n\n---------------------------------------------------------------------------------------------------------------\n");
    symLinkTest4("README");

    // Test 5: Demonstrate that if we are in a recursive symlink loop, and a depth of 10 is already reached, we print out an error and stop.
    printf(1, "\n\n---------------------------------------------------------------------------------------------------------------\n");
    symLinkTest5();

    // Test 6: Demonstrate that linking and unlinking operate on the symbolic link itself.
    printf(1, "\n\n---------------------------------------------------------------------------------------------------------------\n");
    symLinkTest6();
    unlink("symA"); // clean up

    // Doing some final cleanup...
    unlink("sym1");
    unlink("sym2");
    unlink("sym3");
    unlink("sym4");
    unlink("sym5");
    unlink("sym6");
    unlink("sym7");
    unlink("sym8");
    unlink("sym9");
    unlink("sym10");
    unlink("sym11"); 

    exit();
} // end main()
