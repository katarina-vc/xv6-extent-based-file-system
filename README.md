# xv6 - Adding lseek, symlinks, and an extent based file system

## Devs
* Katarina Capalbo 
* John Maurer
* Azim Ibragimov

## System Environments
- Windows (WSL/Ubuntu)
    > No special steps other than running `make clean` and then `make qemu-nox` to prevent x-forwarding.

## Implementation Notes

Screenshots for all parts can be found in the [Screenshots](/Project%204%20Screenshots) folder.

## Part 1 - Adding `lseek`
To implement the `lseek` system call with `SEEK_SET` functionality, we first needed to add it in as a system call. The code lives in `sysfile.c`, as that is also the location of all other file-related system calls. It accepts a file descriptor and an offset, and verifies that the arguments are valid. It obtains a file pointer via `myproc()->ofile[fileDescriptor]`, which then allows us to add the offset argument to `filePointer->off`. It returns this updated ofset value.

Secondly, in `file.c`, a modification to the `filewrite()` function was necessary. In the event the file offset was bigger than the size of the file, a character array filled with null characters is written to the file, and the file size is adjusted accordingly based on the difference between the offset and the file size. Below is a picture demonstrating the `lseek` capabilities with the user program `fsproj4`:

![lseek demo](/Project%204%20Screenshots/Part%201%20-%20lseek%20Screenshots/Project4-Part1-LSeek-Demo.png)

The program writes the lines "Test 1: 12345\n" and "Test 2: ABC\n" to the file "lseektest". The first write consists of 15 bytes, then `lseek` moves the offset by 10 bytes (to position 25) before the second write, which writes 13 bytes. The file size is now 25 + 13 = 38, and `cat` returns the file contents as expected.

## Part 2 - Adding Symbolic Links
To implement symlinks in the xv6 operating system, I started by implementing the tips noted in the Part 2 project description. When the symlink system call is called, we create a new file to act as our "symlink" file, and a new index node to point to our symlink file. In our symlink's index node, we store the target's path. To work within the xv6 operating system constraints, I have limited symlink paths to being 14 characters long (the DIRSIZ constant value), and we can recursively follow a depth of 10 symlinks before you get an error.

To demonstrate that our symlink functionality meets the requirements for this project, I created the user program "testSymLink". By running "testSymLink" (by itself, no arguments), I run various tests on symlinks to display that they are working as expected. The output of "testSymLink" will describe what is happening with each test, but you can also peruse the code in "testSymLink.c" to see how it's actually testing and working. 

After running the "testSymLink" user program, you will get a large chunk of output as each test runs. See the screenshots in part 3 for output. 

For each test, we utilize the "README" file that is already provided in the xv6 operating system as our "target" file to create the symlink for, and we repeatedly print it out based on the test to prove that we properly created a symbolic link to the target file. 

### "testSymLink" User Program Output Explanation

#### SymLink Test 1 - Create a symlink and validate that it works
--------------------------------------------------------------------------------------------------------------------------------------------------
Test 1 demonstrates that we are simply able to create a symlink using our symlink() system call. In the test's code, I create a symlink called "symA" with the "README" file as our target. After creating the symlink, I open the symlink and then print out the results of what we opened to verify that our symlink is properly targeting and opening that "README" file. To print out the "README" file, I used the code from xv6's "cat.c" file. 

![image](https://user-images.githubusercontent.com/25674116/233755360-06138b85-8bcc-4192-b903-182ad3b8e6cf.png)


#### SymLink Test 2 - Create a symlink to a non-existant target and prove that it fails when we open the symlink
--------------------------------------------------------------------------------------------------------------------------------------------------
Test 2 demonstrates the requirement that we can use the symlink() system call to create symlinks regardless of whether or not the target file actually exists. This test is designed to purposefully fail and throw an error. We create the symlink to a non-existant target file, and then attempt to open and read our symlink. In the sysfile.c code, upon attempting to open the symlink's target, we end up failing as our target does not exist. An error message is printed out, which denotes a successful Test 2.

![image](https://user-images.githubusercontent.com/25674116/233755376-8f627675-9432-4ab4-b0fa-0f8a316b873c.png)


#### SymLink Test 3 - Open a symlink using the O_NOFOLLOW flag
--------------------------------------------------------------------------------------------------------------------------------------------------
For Test 3 we demonstrate using the O_NOFOLLOW flag to open the symlink, and not follow the symlink to its target. We first create a symlink with the "README" file as its target. Then we open the symlink and pass in the O_NOFOLLOW flag to our open() call. We then read the file, but since this is our symlink and we passed the O_NOFOLLOW flag, we read and print out the symlink's inode data which in our case is our target's path. In doing so our test prints out "README". This is to prove that we opened the symlink file only, and did not follow to its target path, which would've printed out the contents "README" file.

![image](https://user-images.githubusercontent.com/25674116/233755384-497b7045-91c2-4c4f-8b83-0bb796bb28fd.png)


#### SymLink Test 4 - Prove that symlinks can recurisvely open each other until they reach a target file
--------------------------------------------------------------------------------------------------------------------------------------------------
Test 4 demonstratest that we can successfully create symlinks to other symlinks, and then also recursively follow those symlinks to a final target file. We create a total of 10 symlinks (which is our depth limit of symlinks to follow recursively), each one links to each other. We then open our top level symlink, which then recursively follows down the line til it gets to our first symlink which has the "README" file as its target. To prove we made it there, we then print out the "README" file contents.

![image](https://user-images.githubusercontent.com/25674116/233755401-fb47a032-f225-4f31-9487-896a10b1e2b6.png)


#### SymLink Test 5 - Prove that symlinks can only recursively call each other until they reach a depth of 10 to avoid loops.
--------------------------------------------------------------------------------------------------------------------------------------------------
Test 5 demonstrates that we have a recursive depth to our symlinks of 10. Going off of our 10 symlinks created during test 4, we create one more symlinks, "sym11", which points to our top level symlink, "sym10", from test 4. This would make 11 symlinks that all point to each other respectively, which is past our depth limit. This test is designed to fail on purpose to validate that our depth limit is working. Upon trying to open "sym11", our call fails as it goes past our depth limit.

![image](https://user-images.githubusercontent.com/25674116/233755419-fbd3d138-3893-438a-99c5-b7dacd0efc9f.png)


#### SymLink Test 6 - Prove that link() and unlink() operate on the symbolic link itself, and not its target file
--------------------------------------------------------------------------------------------------------------------------------------------------
In Test 6 we prove that we can use the link() and unlink() system calls on our symbolic links, and that they operate on the symbolic links themselves and not their target file. We first create a hard link() to our one of our symbolic links that is a symlink for the README file. We then open the new hard link to prove that it opens the symlink, and is connected to the symbolic link and not our target file. It prints out "README" upon success as the hard link is reading our symbolic link. We will then unlink our new hard link, and repeat attempting to open and read the hard link. This should fail, as we have successfully unlinked the hard link from our symlink and reading a non-existant link should no longer work.

![image](https://user-images.githubusercontent.com/25674116/233755431-b17b5c04-8260-4303-8068-1a842b17e983.png)


## Part 3 - Adding Large File Support
In order to implement the support for the large file, we had to perform the following steps: 
- Create a function that would be able to write large files (for testing purposes)
- Verify that xv6 does not have pre-built support for large files
- Implement a double inode system that would support the large files 

<u>Function that would create large files</u>
We have created a function that can write a large file. As the guidelines state, we created a function that allocated 16,523 blocks. This function can be reviewed in the file `writebigfile. c` and can be utilized by calling `writebigfile` within the xv6 terminal. After using the function, a new file will appear. Its size would be `16523*512 = 8,459,776` bytes. 

<u>Verification that xv6 does not have pre-built support</u>
To verify that xv6 does not have pre-built support, we decided to run our command in the xv6. As expected, the xv6 failed to support the large file and panicked instead. This can be seen in the following screenshot. 
![demo](/Project%204%20Screenshots/Part%203%20-%20Big%20File%20Screenshots/Project4-Part3-BeforeImplementation.png)

<u>Doubley inode system</u>
Now is the time to implement a system capable of supporting large files. To make this possible, we changed files 'fs. h' to enable an extra bit allocated for the additional indirect block. Then, we modified the `fs. c` file, specifically, the `bmap()` function, to utilize the added bit. 
After making the modifications, we tested our code. It may take some time (about 30 seconds) to create the large file, but the file would be created successfully, and it can be stored inside of the current working directory like in the image below
![demo](/Project%204%20Screenshots/Part%203%20-%20Big%20File%20Screenshots/Project4-Part3-LargeFile-AfterImplementationPic.png)

## Part 4 - Adding Extent-based File Support
To create extents, I created an `extent` struct in `file.h` to hold both the starting address and length pair for each individual extent. When we create a file and pass the `O_EXTENT` flag, we create an extent file.

We don't start allocating anything to our extent file until we write to the extent file. When we write to the extent file, we allocate an extent. The size of the extent we allocate is based off of the size of the data we are writing. If we write to a file multiple times, we can see how more extents are added to the file, and the length of each extent is dependent again on the size of the data we are writing. Therefore, our files can have multiple extents of various length.

When we read from an extent, we read based on each extent in the file at its start address, and then traverse through the length of the extent until the read operation is complete. 

## testExtent user program
The `testExtent` user program runs tests on extent files to prove we are reading, writing, and freeing extents properly. To run `testExten`t, pass in one argument that you would like your file name to be. such as:

`textExtent test`

The user program will create and then write a chunk of content to an extent file. We know that the full chunk of content was properly written and read because we denote it's start and end by "CONTENT 1 START" and "CONTENT 1 END". We follow this pattern for the rest of the chunks of content. After we write to the extent, read it, and output it, we create a child process to call `exec()` which calls the `stat` user program on our extent file. See image below. 

![image](https://user-images.githubusercontent.com/25674116/234159792-76b8f804-e355-4dbc-b831-9a4dd853bbe4.png)

The next test that `testExtent` runs is adding a second chunk of content to the file, reading, and outputting the file. We run `stat` again as well. This is to prove we can grow our file, and that it can contain multiple extents of various sizes.

![image](https://user-images.githubusercontent.com/25674116/234160072-86e689db-f464-467c-afd6-34d4f7b49c25.png)

We perform this test one more time with a third chunk of data.

![image](https://user-images.githubusercontent.com/25674116/234160247-466a161d-b0fc-4014-909d-c2cef2b66893.png)


## stat user program
I also implemented the `stat` user program, which checks if the file type we are reading is an extent type or pointer based file. If we `stat` an extent file, then we get the file's information plus information about its extents (see screenshot below)/

![image](https://user-images.githubusercontent.com/25674116/234158993-1709b034-450f-45be-9f53-025ddd28f880.png)

If we `stat` a pointer based file, such as the README file that the xv6 operating system has, then we get information about its file and its direct pointers.

![image](https://user-images.githubusercontent.com/25674116/234159177-61dde823-1691-40de-b98a-f5869a064e44.png)
