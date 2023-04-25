# Project 4 Part 4 Adding extent-based file support

Screenshots demonstrating that we have properly implemented extent-based files in the xv6 operating with backwards compatibility for pointer-based files. 

We prove this by running a test user program I created called "extentTest". By running "extentTest", we perform various tests on extent files and print out their output.
In the extentTest.c code, I also create child processes that run exec() to run the stat user program on our extent files as we create them in the test program. 

All you have to do to test this is run "extentTest". 

After running "extentTest" (which creates extent files), you can run further tests by running "ls" to see the new extent files, and then running the "stat" user
program on both extent files and already existing pointer-based files (such as running "stat README"). 
