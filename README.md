# COP6611.001S23 - Operating Systems - Project 4

## Group Members (Same Team from Projects 1, 2, & 3)
* Katarina Capalbo 
* John Maurer
* Azim Ibragimov

## System Environments
- Windows (WSL/Ubuntu)
    > No special steps other than running `make clean` and then `make qemu-nox` to prevent x-forwarding.

## Implementation Notes

### Part 1 - Adding `lseek`

### Part 2 - Adding Symbolic Links
Under a section labeled "PART 2" in your Project4-README.txt briefly describe how you implemented symbolic links and how your user program demonstrates the functionality.

To implement symlinks in the xv6 operating system, I started by implementing the tips noted in the Part 2 project description. When the symlink system call is called, we create a new file to act as our "symlink" file, and a new index node to point to our symlink file. In our symlink's index node, we store the target's path. To work within the xv6 operating system constraints, symlink paths are limited to being 14 characters long (the DIRSIZ constant value), and we can recursively follow a depth of 10 symlinks before you get an error.

To demonstrate that our symlink functionality meets the requirements for this project, I created the user program "testSymLink". By running "testSymLink", we run various tests on symlinks to display that they are working as expected. After running "testSymLink", you will get a large chunk of output as each test runs.

For each test, we utilize the "README" file that is already provided in the xv6 operating system as our "target" file to create the symlink for, and we repeatedly print it out based on the test. 

SymLink Test 1
---------------------------
Test 1 demonstrates that we are able to create a symlink using our symlink system call. We create a symlink called "symA" with the "README" file as our target. After creating the symlink, we open the symlink and then print out the results to verify that our symlink is properly targeting and opening that "README" file. To print out the "README" file, I use the code from xv6's "cat.c" file. 

SymLink Test 2
---------------------------
Test 2 demonstrates the requirement that we can use the symlink system call to create symlinks regardless of whether or not the target file actually exists. This test is designed to purposefully fail and throw an error. We create the symlink to a non-existant target file, and then attempt to open and read our symlink. In the sysfile.c code, upon attempting to open the symlink's target, we end up failing as our target does not exist. An error message is printed out, which denotes a successful Test 2.

SymLink Test 3
---------------------------
For Test 3 we demonstrate using the O_NOFOLLOW flag to open the symlink, and not follow the symlink to its target. We first create a symlink with the "README" file as its target. Then we open the symlink and pass in the O_NOFOLLOW flag to our open() call. We then read the file, but since this is our symlink, we read and print out the symlink's inode data which in our case is our target's path. In doing so our test prints out "README". This is to prove that we opened the symlink file only, and did not follow to its target path, which would've printed out the "README" file.

SymLink Test 4
---------------------------
Test 4 demonstratest that we can successfully create symlinks to other symlinks, and then also recursively follow those symlinks to a final target file. We create a total of 10 symlinks (which is out depth limit of symlinks to follow recursively), each one links to each other. We then open our top level symlink, and which then recursively follows down the line til it gets to our first symlink which has the "README" file as its target. To prove we made it there, we then print out the "README" file contents.


SymLink Test 5
---------------------------
Test 5 demonstrates that we have a recursive depth to our symlinks of 10. Going off of our 10 symlinks created during test 4, we create one more symlinks, "sym11", which points to our top level symlink, "sym10", from test 4. This would make 11 symlinks that all point to each other respectively, which is past our depth limit. This test is designed to fail on purpose to validate that our depth limit is working. Upon trying to open "sym11", our call fails as it goes past our depth limit.

### Part 3 - Adding Large File Support

### Part 4 - Adding Extent-based File Support
