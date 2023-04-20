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
    const char *fileContentWrite1 = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

/*
    const char *fileContentWrite2 = "Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque laudantium, 
    totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi architecto beatae vitae dicta sunt explicabo. Nemo enim
     ipsam voluptatem quia voluptas sit aspernatur aut odit aut fugit, sed quia consequuntur magni dolores eos qui ratione voluptatem 
     sequi nesciunt. Neque porro quisquam est, qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit, sed quia non numquam 
     eius modi tempora incidunt ut labore et dolore magnam aliquam quaerat voluptatem. Ut enim ad minima veniam, quis 
     nostrum exercitationem ullam corporis suscipit laboriosam, nisi ut aliquid ex ea commodi consequatur? Quis autem vel eum iure 
     reprehenderit qui in ea voluptate velit esse quam nihil molestiae consequatur, vel illum qui dolorem eum fugiat quo voluptas nulla 
     pariatur?";

     const char *fileContentWrite3 = "At vero eos et accusamus et iusto odio dignissimos ducimus qui blanditiis 
     praesentium voluptatum deleniti atque corrupti quos dolores et quas molestias excepturi sint occaecati cupiditate 
     non provident, similique sunt in culpa qui officia deserunt mollitia animi, id est laborum et dolorum fuga. 
     Et harum quidem rerum facilis est et expedita distinctio. Nam libero tempore, cum soluta nobis est eligendi optio 
     cumque nihil impedit quo minus id quod maxime placeat facere possimus, omnis voluptas assumenda est, omnis dolor 
     repellendus. Temporibus autem quibusdam et aut officiis debitis aut rerum necessitatibus saepe eveniet ut et voluptates 
     repudiandae sint et molestiae non recusandae. Itaque earum rerum hic tenetur a sapiente delectus, ut 
     aut reiciendis voluptatibus maiores alias consequatur aut perferendis doloribus asperiores repellat.";
*/

    // **** Creating the extent file **************************************************************************************
    int fileDescriptor;
    if((fileDescriptor = open(argv[1], O_EXTENT)) < 0) {
            printf(1, "testExtent failed: Cannot create extent: %s\n", argv[1]);
            exit();
    }
    close(fileDescriptor);

    // **** Write file content number 1 to our extent. **********************************************************************
    int fileDescriptor2;
    if((fileDescriptor2 = open(argv[1], O_WRONLY)) < 0) {
            printf(1, "testExtent failed: Cannot open extent %s\n", argv[1]);
            exit();
    }

    if (write(fileDescriptor2, fileContentWrite1, strlen(fileContentWrite1)) != strlen(fileContentWrite1)) {
        printf(2, "testExtent failed: failed to write the content to the extent file.\n");
        close(fileDescriptor2);
        exit();
    }
    close(fileDescriptor2);

    // **** Read and print out the contents of our extent file to prove we did it. ******************************************
    int fileDescriptor3;
    char buf[512];
    if((fileDescriptor3 = open(argv[1], O_RDONLY)) < 0) {
            printf(1, "testExtent failed: Cannot open extent file %s\n", argv[1]);
            exit();
    }

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
    if (n < 0)
    {
        printf(1, "testExtent failed: Something went wrong reading extent file.\n");
        exit();
    }
    close(fileDescriptor3);

    exit();
} // end main()
