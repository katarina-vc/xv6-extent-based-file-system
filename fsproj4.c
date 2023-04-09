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
	char line1[] = "Test 1: 12345";
	char line2[] = "Test 2: ABC";

	// Open file
	printf(1, "Opening new file 'test1'...\n");

	if((fd = open("proj4test", O_CREATE | O_RDWR)) < 0){
		printf(1, "Could not create file. Exiting.\n");
		exit();
	}

	// Write to the file (super safely)
	write(fd, line1, 12);
	
	// Seek to new position
	int newOffset = lseek(fd, 10);
	printf(1, "Seek set the new location to %d.\n", newOffset);
	
	// Write to the file again (extremely safely)
	write(fd, line2, 10);
	
	// Close the file
	close(fd);

	exit();
}
