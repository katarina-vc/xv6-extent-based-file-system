/*
Project 4 testing user program
Authors - Katarina Capalbo, John Maurer, Azim Ibragimov
*/

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[]){
	// JTM - Test lseek system call. Variable initialization
	int fd;
	int firstWriteBytes;
	int secondWriteBytes;

	char line1[] = "Test 1: 12345\n";
	char line2[] = "Test 2: ABC\n";

	int line1Size = sizeof(line1) / sizeof(line1[0]);
	int line2Size = sizeof(line2) / sizeof(line2[0]);

	// Open file
	printf(1, "Opening new file 'lseektest'...\n");

	if((fd = open("lseektest", O_CREATE | O_RDWR)) < 0){
		printf(1, "fsproj4 - lseek test: Could not create file. Exiting.\n");
		close(fd);
		exit();
	}

	// Write to the file (super safely)
	if((firstWriteBytes = write(fd, line1, line1Size)) < 0){
		printf(1, "fsproj4 - lseek test: First write failed. Exiting.\n");
		close(fd);
		exit();
	}else{
		printf(1, "First write successful. Bytes written: %d\n", firstWriteBytes);
	}
	
	// Seek to new position
	int newOffset = lseek(fd, 10);
	printf(1, "Seek set the new location to %d.\n", newOffset);
	
	// Write to the file again (extremely safely)
	if((secondWriteBytes = write(fd, line2, line2Size)) < 0){
		printf(1, "fsproj4 - lseek test: Second write failed. Exiting.\n");
		close(fd);
		exit();
	}else{
		printf(1, "Second write successful. Bytes written: %d\n", secondWriteBytes);
	}
	
	// Close the file
	close(fd);

	exit();
}
