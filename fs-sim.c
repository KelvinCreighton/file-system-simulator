#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define SIZE_OF_BYTE 8          // In bits
#define MAX_CMD_ARGS 3          // How many arguments in a line will be counted, sparated by spaces
#define FREE_SPACE_LIST_SIZE 16 // In bytes
#define INODE_SIZE 8            // In bytes
#define BLOCK_SIZE 1024         // In bytes
#define FILE_MAX_SIZE 127       // In blocks (KB)
#define FILE_NAME_SIZE 5        // In bytes

FILE *disk;


void fs_mount(char *new_disk_name) {
    disk = fopen(new_disk_name, "rb");
    if (disk == NULL) {
        printf("Error: File '%s' does not exist.\n", new_disk_name);
        return;
    }
    
    // Get the superblock
    char superblock[BLOCK_SIZE];
    fread(superblock, 1, BLOCK_SIZE, disk);
    fclose(disk);

    // Check consistency of the file system
    int error_code = 0;
    
    // Skip the free-space list and iterate through every inode
    for (unsigned int i = FREE_SPACE_LIST_SIZE; i < BLOCK_SIZE; i += INODE_SIZE) {
        uint64_t inode = *(uint64_t *)(superblock + i);
        printf("inode: %016lx\n", inode);

        // Error 1: If the state of an inode is free, then all bits in this inode must be zero
        // Otherwise, the name attribute stored in the inode must have at least one bit that is not zero
        uint8_t inode_usedSize = (uint8_t)(inode >> 5*SIZE_OF_BYTE);
        printf("inode_usedSize: %02x\n", inode_usedSize);
        uint8_t inode_state = inode_usedSize & 0x80;    // Get the first bit
        printf("inode_state: %02x\n", inode_state);
        if (inode_state == 0 && inode != 0) {
            error_code = 1;
            break;
        }
        uint64_t inode_name = inode & 0x000000FFFFFFFFFF;
        printf("inode_name: %010lx\n", inode_name);
        if (inode_state == 1 && inode_name == 0) {
            error_code = 1;
            break;
        }


        // Error 2: The start block of every inode pertaining to a file (i.e. when the directory bit is not set) must have a value between 1 and 127 inclusive
        // Moreover, the size of every inode pertaining to a file must be such that its last block is also between 1 and 127
        uint8_t inode_startBlock = (uint8_t)(inode >> 6*SIZE_OF_BYTE);
        printf("inode_startBlock: %02x\n", inode_startBlock);
        if (inode_startBlock < 1 || inode_startBlock > 127) {
            error_code = 2;
            break;
        }
        uint8_t inode_size = inode_usedSize & 0x7F;     // Get all 7 bits except the first
        printf("inode_size: %02x\n", inode_size);
        printf("end block: %d\n", inode_startBlock+inode_size-1);
        if (inode_startBlock+inode_size-1 > 127) {
            error_code = 2;
            break;
        }

        // Error 3: The size and start block of an inode pertaining to a directory (i.e. the directory bit is set) must be zero
        uint8_t inode_dirParent = (uint8_t)(inode >> 7*SIZE_OF_BYTE);
        printf("inode_dirParent: %02x\n", inode_dirParent);
        uint8_t inode_type = inode_dirParent & 0x80;    // Get the first bit
        printf("inode_type: %02x\n", inode_type);
        if (inode_type != 0 && inode_size > 0) {
            error_code = 3;
            break;
        }

        // Error 4: For every inode, the index of its parent inode cannot be 126
        // Moreover, if the index of the parent inode is between 0 and 125 inclusive, then the parent inode must be in use and marked as a directory
        uint8_t inode_parent = inode_dirParent & 0x7F;  // Get all 7 bits except the first
        printf("inode_parent: %02x\n", inode_parent);
        if (inode_parent == 126) {
            error_code = 4;
            break;
        }
        uint64_t parent_inode = *(uint64_t *)(superblock+FREE_SPACE_LIST_SIZE + inode_parent*SIZE_OF_BYTE);
        printf("parent_inode: %016lx\n", parent_inode);

        break;
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

void fs_create(char name[5], int size) {}
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

            case 'c':
                if (tokenCount < 2) {
                    printf("No file name given\n");
                    break;
                }
                if (tokenCount < 3) {
                    printf("No file size given\n");
                    break;
                }
                
                printf("case: create\n");
                break;

            default:
                printf("Error: unknown command\n");
        }


        printf("------------\n");
    }

    fclose(cmds);     
    return 0;
}
