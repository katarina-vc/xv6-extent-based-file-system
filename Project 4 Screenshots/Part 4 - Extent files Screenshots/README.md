# Project 4 Part 4 Adding extent-based file support

Screenshots demonstrating that we have properly implemented extent-based files in the xv6 operating with backwards compatibility for pointer-based files. 

We prove this by running a test user program I created called "extentTest". The extentTest user program takes in one argument, which is the name of the extent file you would like to create. By running "extentTest", we perform various tests on extent files and print out their output.

First we create an extent file, and write a chunk of data to it. To prove this data was properly written to our extent file, we open our extent file, read it, and write out its content to the console.

In the extentTest.c code, I also create child processes that run exec() periodically to run the stat user program on our extent files as we create them in the test program. The stat user program will run three times, after each write, to show how we have updated the extents in our extent file.

All you have to do to test this is run:

extentTest test


After running "extentTest" (which creates your extent file), you can run further tests by running "ls" to see the new extent files, and then running the "stat" user
program on both extent files and already existing pointer-based files. For example run:

stat test

(you should see the output for your extent file "test")

stat README

(you will see output for a pointer based file, README)
