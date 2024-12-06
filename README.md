

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# Name : Kelvin Creighton
# SID : 1695352
# CCID : kcreight
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


Syscalls:
    
    - in my script i used the fopen, fread, fwrite, fseek, fclose library functions which call the file read and write ... etc system calls
    - additionally i used the printf and fprintf to print to stdout for the fs_ls (and testing) and stderr for error messages
    - other system calls like malloc and calloc were used to create and free memory



Design choices:

    - i started by creating my own struct called disk of type Disk which was used as a general global variable throughout the script. i decided that having multiple global variables is generally not advised so i put everything related to the disk in the Disk struct which includes its name, if it is mounted, a pointer to the superblock, and a list of used names in a directory.
    - to track the unique names in each directory i used an array of the total amount of possible directories as pointers to each linked list which if another file name was added to the director it would be added to the linked list. this would allow me to check the current (or whichever) directory and cycle throught the linked list to know which names were already used.
    - half way through this assignment i realized i should have used a tree structure but since i mostly completed major functions i didnt want to change it half way through. in a future episode i might


Testing:
    - for testing i used the test scripts given
    - additionally i used my own test scripts to check each little edge case i could think of to verify each error message was properly used and that no other error message would occur. this helped me track down small issues especially with stack overflow when using recursion for the fs_delete function and also multiple segfaults for memory issues


Sources used:
    - stack overflow for the fopen and fclose for file reading since these library functions are similar but slightly different than what i know
    - the assignment for structure
    - thats pretty much it yall didnt really give any resources or help for this assignment...
