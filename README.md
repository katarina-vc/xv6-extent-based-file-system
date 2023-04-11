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

### Part 3 - Adding Large File Support

### Part 4 - Adding Extent-based File Support
