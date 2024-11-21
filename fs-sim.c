#include <stdint.h>
#include "fs-sim.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_CMD_ARGS 3          // How many arguments in a line will be counted
#define FREE_BLOCK_LIST_SIZE 16 // bytes
#define INODES_LENGTH 126       // count
#define BLOCK_SIZE 1024         // bytes

FILE *disk;
Superblock *superblock;

void fs_mount(char *new_disk_name) {
    disk = fopen(new_disk_name, "rb");
    if (disk == NULL) {
        printf("Error: File '%s' does not exist.\n", new_disk_name);
        return;
    }
    
    // Get the superblock
    char buffer[BLOCK_SIZE];
    fread(buffer, 1, BLOCK_SIZE, disk);
    fclose(disk);
    // Create superblock struct
    superblock = malloc(sizeof(Superblock));
    memcpy(superblock->free_block_list, buffer, FREE_BLOCK_LIST_SIZE);
    size_t inodes_offset = FREE_BLOCK_LIST_SIZE;
    size_t inodes_size = sizeof(superblock->inode);
    memcpy(superblock->inode, buffer + inodes_offset, inodes_size);
    
    // Check consistency of the file system
    int error_code = 0;
    for (unsigned int i = 0; i < INODES_LENGTH; i++) {
        Inode inode = superblock->inode[i];
        // inode not in use
        if (*(uint64_t *)&inode == 0) {
            continue;
        }
        printf("---- i = %d ----\n", i);
        printf("inode: %016lx\n", *(uint64_t *)&inode);
        
        // Error 1: If the state of an inode is free, then all bits in this inode must be zero
        // Otherwise, the name attribute stored in the inode must have at least one bit that is not zero
        printf("inode_usedSize: %02x\n", inode.used_size);
        uint8_t inode_state = inode.used_size >> 7;  // Get the first bit
        printf("inode_state: %02x\n", inode_state);
        if (inode_state == 0) {
            error_code = 1;
            break;
        }
        // print name
        printf("inode_name: ");
        for (int j = 0; j < 5; j++) {
            char letter = inode.name[j];
            if (letter == '\0')
                break;
            printf("%c", letter);
        }
        printf("\n");

        if (inode_state == 1 && inode.name[0] == 0) {
            error_code = 1;
            break;
        }

        // Error 2: The start block of every inode pertaining to a file (i.e. when the directory bit is not set) must have a value between 1 and 127 inclusive
        // Moreover, the size of every inode pertaining to a file must be such that its last block is also between 1 and 127
        printf("inode_startBlock: %02x\n", inode.start_block);
        if (inode.start_block < 1 || inode.start_block > 127) {
            error_code = 2;
            break;
        }
        uint8_t inode_size = inode.used_size & 0x7F;    // Remove the first bit
        printf("inode_size: %02x\n", inode_size);
        printf("end block: %d\n", inode.start_block+inode_size-1);
        if (inode.start_block+inode_size-1 > 127) {
            error_code = 2;
            break;
        }

        // Error 3: The size and start block of an inode pertaining to a directory (i.e. the directory bit is set) must be zero
        printf("inode_dirParent: %02x\n", inode.dir_parent);
        uint8_t inode_type = inode.dir_parent >> 7;    // Get the first bit
        printf("inode_type: %02x\n", inode_type);
        if (inode_type == 1 && inode_size > 0) {
            error_code = 3;
            break;
        }

        // Error 4: For every inode, the index of its parent inode cannot be 126
        // Moreover, if the index of the parent inode is between 0 and 125 inclusive, then the parent inode must be in use and marked as a directory
        uint8_t inode_parent = inode.dir_parent & 0x7F;    // Remove the first bit
        printf("inode_parent: %02x\n", inode_parent);
        if (inode_parent == 126) {
            error_code = 4;
            break;
        }
        Inode parent_inode = superblock->inode[inode_parent];
        printf("parent_inode: %016lx\n", *(uint64_t *)&parent_inode);
        uint8_t parent_inode_state = parent_inode.used_size >> 7;  // Get the first bit
        printf("parent_inode_state: %02x\n", parent_inode_state);
        uint8_t parent_inode_type = parent_inode.dir_parent >> 7;  // Get the first bit
        printf("parent_inode_type: %02x\n", parent_inode_type);
        if (parent_inode_state == 0 || parent_inode_type == 0) {
            error_code = 4;
            break;
        }
    }

    /*
     * 
5. The name of every file/directory must be unique in each directory (not in the entire file system).

6. Blocks that are marked free in the free-space list cannot be allocated to any file. Similarly, blocks that
are marked in use in the free-space list must be allocated to exactly one file.
*/
    if (error_code > 0) {
        printf("Error: File system in %s is inconsistent (error code: %d)\n", new_disk_name, error_code);
        return;
    }
    printf("\nMounted: %s\n", new_disk_name);
}


void fs_create(char name[5], int size) {
    printf("name: %s\n", name);
    printf("size: %d\n", size);
   
    // Check for an available inode
    int inodeAvailable = 0;
    for (unsigned int i = 0; i < INODES_LENGTH; i++) {
        // inode not in use
        if (*(int64_t *)&superblock->inode[i] == 0) {
            inodeAvailable = 1;
            break;
        }
    }
    if (!inodeAvailable) {
        printf("Error: Superblock in disk <disk name> is full, cannot create <file name>");
    }
       
}
void fs_delete(char name[5]) {}
void fs_read(char name[5], int block_num) {}
void fs_write(char name[5], int block_num) {}
void fs_buff(char buff[1024]) {}
void fs_ls(void) {}
void fs_resize(char name[5], int new_size) {}
void fs_defrag(void) {}
void fs_cd(char name[5]) {}


int main(int argc, char *argv[]) {
    printf("================\n\n");

    if (argc < 2) {
        printf("No cmds file given\n");
        return 1;
    }

    FILE *cmds = fopen(argv[1], "r");
    if (cmds == NULL) {
        printf("Error: reading cmds file\n");
        return 1;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), cmds) != NULL) {
        // Skip any line that is only a newline
        if (buffer[0] == '\n')
            continue;
        // Remove trailing newline character if it exists
        buffer[strcspn(buffer, "\n")] = '\0';
        // Split the buffer by spaces
        char *tokens[MAX_CMD_ARGS];
        char *token = strtok(buffer, " ");
        int tokenCount = 0;
        printf("tokens: ");
        while (token != NULL && tokenCount < MAX_CMD_ARGS) {
            printf("%s, ", token);
            tokens[tokenCount] = token;
            token = strtok(NULL, " ");
            tokenCount++;
        }
        printf("\n");
        // Return error if the first token (command) is not exactly one character in length
        if (tokens[0][1] != '\0') {
            printf("Error: unknown command\n");
            continue;
        }
    
        // Convert the command to lower
        if (tokens[0][0] >= 65 && tokens[0][0] <= 90)
            tokens[0][0] += 32;
    
        // Run the function specified by the second argument
        switch (tokens[0][0]) {
            case 'm':
                if (tokenCount < 2) {
                    printf("No disk name given\n");
                    break;
                }
                fs_mount(tokens[1]);
                break;
            // --------------------------------
            case 'c':
                if (tokenCount < 2) {
                    printf("No file name given\n");
                    break;
                }
                if (tokenCount < 3) {
                    printf("No file size given\n");
                    break;
                }
                int size = 0;
                int invalid = 0;
                // Convert token to integer
                for (int i = 0; i < 3; i++) {
                    if (tokens[2][i] == '\0')
                        break;
                    if (tokens[2][i] < 48 || tokens[2][i] > 57) {
                        printf("Error: invalid character: '%c'\n", tokens[2][i]);
                        invalid = 1;
                        break;
                    }
                    size *= 10;
                    size += (int)(tokens[2][i]-48);
                }
                if (invalid)
                    break;
                fs_create(tokens[1], size);
                break;
            // --------------------------------
            default:
                printf("Error: unknown command\n");
        }


        printf("------------\n");
    }

    fclose(cmds);     
    return 0;
}
