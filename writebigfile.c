#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"



int main(){
    // printf(1, "Value of 16523*bsize=%d\n", 16523*BSIZE);
    char buf[BSIZE];
    int file = open("bigfile.txt", O_CREATE | O_RDWR);
    if (file < 0){
        printf(1, "writebigfile test: Could not create file. Exiting.\n");
        close(file);
		exit();
    }

    int firstWriteBytes;
    for(int i = 0; i < 16523; i++){
        firstWriteBytes= write(file, buf, BSIZE);
    }


    if(firstWriteBytes<0){
        printf(1, "writebigfile test: Could not write to the file. Exiting.\n");
        close(file);
		exit();
    }
    
    printf(1, "Yay! Write successful! Size of the new file: %d\n", 16523*BSIZE);
    
    exit();


}