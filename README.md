# COP6611.001S23 - Operating Systems - Project 4

## Group Members (Same Team from Projects 1, 2, & 3)
* Katarina Capalbo 
* John Maurer
* Azim Ibragimov

## System Environments
- Windows (WSL/Ubuntu)
    > No special steps other than running `make clean` and then `make qemu-nox` to prevent x-forwarding.

## Implementation Notes

Screenshots for all parts can be found in the [Project 4 Screenshots](/Project%204%20Screenshots) folder.

### Part 1 - Adding `lseek`
To implement the `lseek` system call with `SEEK_SET` functionality, we first needed to add it in as a system call. The code lives in `sysfile.c`, as that is also the location of all other file-related system calls. It accepts a file descriptor and an offset, and verifies that the arguments are valid. It obtains a file pointer via `myproc()->ofile[fileDescriptor]`, which then allows us to add the offset argument to `filePointer->off`. It returns this updated ofset value.

Secondly, in `file.c`, a modification to the `filewrite()` function was necessary. In the event the file offset was bigger than the size of the file, a character array filled with null characters is written to the file, and the file size is adjusted accordingly based on the difference between the offset and the file size. Below is a picture demonstrating the `lseek` capabilities with the user program `fsproj4`:

![lseek demo](/Project%204%20Screenshots/lseek%20demo.PNG)

The program writes the lines "Test 1: 12345\n" and "Test 2: ABC\n" to the file "lseektest". The first write consists of 15 bytes, then `lseek` moves the offset by 10 bytes (to position 25) before the second write, which writes 13 bytes. The file size is now 25 + 13 = 38, and `cat` returns the file contents as expected.

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
In order to implement the support for the large file, we had to perform the following steps: 
- Create a function that would be able to write large files (for testing purposes)
- Verify that xv6 does not have pre-built support for large files
- Implement a double inode system that would support the large files 

<u>Function that would create large files</u>
We have created a function that can write a large file. As the guidelines state, we created a function that allocated 16,523 blocks. This function can be reviewed in the file `writebigfile. c` and can be utilized by calling `writebigfile` within the xv6 terminal. After using the function, a new file will appear. Its size would be `16523*512 = 8,459,776` bytes. 

<u>Verification that xv6 does not have pre-built support</u>
To verify that xv6 does not have pre-built support, we decided to run our command in the xv6. As expected, the xv6 failed to support the large file and panicked instead. This can be seen in the following screenshot. 
![demo](/Project%204%20Screenshots/before_implementation.png)

<u>Doubley inode system</u>
Now is the time to implement a system capable of supporting large files. To make this possible, we changed files 'fs. h' to enable an extra bit allocated for the additional indirect block. Then, we modified the `fs. c` file, specifically, the `bmap()` function, to utilize the added bit. 
After making the modifications, we tested our code. It may take some time (about 30 seconds) to create the large file, but the file would be created successfully, and it can be stored inside of the current working directory like in the image below
![demo](/Project%204%20Screenshots/after_implementation.png)

### Part 4 - Adding Extent-based File Support
