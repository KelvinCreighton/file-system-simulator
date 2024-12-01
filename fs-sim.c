#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fs-sim.h"

#define MAX_CMD_ARGS 3          // How many arguments in a line will be counted
#define BLOCK_SIZE 1024         // bytes
#define FREE_BLOCK_LIST_SIZE 16 // bytes
#define INODES_LENGTH 126       // count
#define INODE_SIZE 8            // bytes
#define FILENAME_LENGTH 5       // bytes

Disk *disk;

void show_filenames() {
    printf("---- filenames ----\n");
    Filenames_node *node;
    for (unsigned int i = 0; i < INODES_LENGTH+1; i++) {
        node = disk->filenames[i];
        if (disk->filenames[i] == NULL)
            continue;
        printf("[%03d]->", i);
        while (node != NULL) {
            printf("[");
            for (unsigned int j = 0; j < FILENAME_LENGTH; j++) {
                if (node->name[j] == '\0') {
                    printf("\\0");
                } else {
                    printf("%c", node->name[j]);
                }
            }
            printf("]->");
            node = node->next;
        }
        printf("[NULL]\n");
    }
}

void show_inode(Inode *inode) {
        uint8_t inode_state = inode->used_size & 0x80;      // Get the first bit
        uint8_t inode_size = inode->used_size & 0x7F;       // Remove the first bit
        uint8_t inode_type = inode->dir_parent & 0x80;      // Get the first bit
        uint8_t inode_parent = inode->dir_parent & 0x7F;    // Remove the first bit
        
        printf("inode_name: ");
        for (int j = 0; j < 5; j++) {
            char letter = inode->name[j];
            if (letter == '\0')
               printf("\\%x", letter);
            else
                printf("%c", letter);
        }
        printf("\n");
        printf("inode_usedSize: %02x\n", inode->used_size);
        printf("inode_state: %02x", inode_state);
        if (inode_state != 0)
            printf(" (in use)\n");
        else
            printf(" (free)\n");
        printf("inode_size: %02x\n", inode_size);
        printf("inode_startBlock: %02x\n", inode->start_block);
        printf("end block: %d\n", inode->start_block+inode_size-1);
        printf("inode_dirParent: %02x\n", inode->dir_parent);
        printf("inode_type: %02x", inode_type);
        if (inode_type != 0)
            printf(" (dir)\n");
        else
            printf(" (file)\n");
        printf("inode_parent: %02x\n", inode_parent);
        
}

void fs_mount(char *new_disk_name) {
    disk = malloc(sizeof(Disk));
    disk->data = fopen(new_disk_name, "rb");
    if (disk->data == NULL) {
        fprintf(stderr, "Error: File '%s' does not exist.\n", new_disk_name);
        return;
    }
    disk->name = malloc(strlen(new_disk_name)+1);
    strcpy(disk->name, new_disk_name);
    disk->current_dir = 126;
    
    // Get the superblock
    char buffer[BLOCK_SIZE];
    fread(buffer, 1, BLOCK_SIZE, disk->data);
    fclose(disk->data);

    // Create superblock struct
    Superblock *superblock = malloc(sizeof(Superblock));
    memcpy(superblock->free_block_list, buffer, FREE_BLOCK_LIST_SIZE);
    disk->superblock = superblock;
    // Set the pointer for all filenames_node in each directory to null
    for (unsigned int i = 0; i < INODES_LENGTH+1; i++) {
        disk->filenames[i] = NULL;
    }

    // Check consistency of the file system
    int error_code = 0;
    for (unsigned int i = 0; i < INODES_LENGTH; i++) {
        // Create and organize the inode structure
        unsigned int inode_offset = FREE_BLOCK_LIST_SIZE + i*INODE_SIZE;
        Inode *inode = malloc(sizeof(Inode));
        memcpy(inode->name, buffer + inode_offset, FILENAME_LENGTH);
        trim_name(inode->name);
        inode->used_size = *(uint8_t *)(buffer + inode_offset + FILENAME_LENGTH);
        inode->start_block = *(uint8_t *)(buffer + inode_offset + FILENAME_LENGTH+1);
        inode->dir_parent = *(uint8_t *)(buffer + inode_offset + FILENAME_LENGTH+2);
        superblock->inode[i] = *inode;

        // Continue if the entire inode is 0 meaning it is not in use
        int name_is_0 = 1;
        for (unsigned int j = 0; j < FILENAME_LENGTH; j++) {
            if (inode->name[j] != 0) {
                name_is_0 = 0;
                break;
            }
        }

        if (name_is_0 && inode->used_size == 0 && inode->start_block == 0 && inode->dir_parent == 0) {
            continue;
        }
        printf("\n---- i = %d ----\n", i);
        
        uint8_t inode_state = inode->used_size & 0x80;      // Get the first bit
        uint8_t inode_size = inode->used_size & 0x7F;       // Remove the first bit
        uint8_t inode_type = inode->dir_parent & 0x80;      // Get the first bit
        uint8_t inode_parent = inode->dir_parent & 0x7F;    // Remove the first bit
        
        show_inode(inode);

        // Error 1: If the state of an inode is free, then all bits in this inode must be zero
        // Otherwise, the name attribute stored in the inode must have at least one bit that is not zero
        if (inode_state == 0) {
            error_code = 1;
            break;
        }
        int name_empty = 1;
        for (unsigned int j = 0; j < FILENAME_LENGTH; j++) {
            if (inode->name[j] != '\0') {
                name_empty = 0;
            }
        }
        if (inode_state != 0 && name_empty) {
            error_code = 1;
            break;
        }

        // Error 2: The start block of every inode pertaining to a file (i.e. when the directory bit is not set) must have a value between 1 and 127 inclusive
        // Moreover, the size of every inode pertaining to a file must be such that its last block is also between 1 and 127
        if (inode->start_block < 1 || inode->start_block > 127) {
            error_code = 2;
            break;
        }
        if (inode->start_block+inode_size-1 > 127) {
            error_code = 2;
            break;
        }

        // Error 3: The size and start block of an inode pertaining to a directory (i.e. the directory bit is set) must be zero
        if (inode_type != 0 && inode_size > 0) {
            error_code = 3;
            break;
        }

        // Error 4: For every inode, the index of its parent inode cannot be 126
        // Moreover, if the index of the parent inode is between 0 and 125 inclusive, then the parent inode must be in use and marked as a directory
        if (inode_parent == 126) {
            error_code = 4;
            break;
        }
        if (inode_parent != 127) {  // Ignore root directory
            unsigned int parent_inode_offset = FREE_BLOCK_LIST_SIZE + inode_parent*INODE_SIZE;
            uint8_t parent_inode_state = *(uint8_t *)(buffer + parent_inode_offset + FILENAME_LENGTH) & 0x80;
            uint8_t parent_inode_type = *(uint8_t *)(buffer + parent_inode_offset + FILENAME_LENGTH+2) & 0x80;

            // -----------------------------
            printf("\n");
            printf("parent_inode_state: %02x", parent_inode_state);
            if (parent_inode_state != 0)
                printf(" (in use)\n");
            else
                printf(" (free)\n");
            printf("parent_inode_type: %02x", parent_inode_type); 
            if (parent_inode_type != 0)
                printf(" (dir)\n");
            else
                printf(" (file)\n");
            // -----------------------------

            if (parent_inode_state == 0 || parent_inode_type == 0) {
                error_code = 4;
                break;
            }
        }
        
        // Error 5: The name of every file/directory must be unique in each directory (not in the entire file system).
        uint8_t filename_index = inode_parent;
        if (filename_index == 127)    // Set root to 126 for filenames
            filename_index = 126;
        printf("filename_index: %d\n", filename_index);
        // Create and organize the filenames list to track which are used for uniqueness
        Filenames_node *node = disk->filenames[filename_index];
        while (node != NULL) {
            if (strncmp_case(node->name, inode->name, FILENAME_LENGTH) == 0) {
                error_code = 5;
                break;
            }
            node = node->next;
        }
        // Append the new filename to the head of the list
        Filenames_node *new_node = malloc(sizeof(Filenames_node));
        memcpy(new_node->name, inode->name, FILENAME_LENGTH); 
        new_node->next = disk->filenames[filename_index];
        disk->filenames[filename_index] = new_node;
   }
    
    printf("\n");
    show_filenames();
    /*
     * 

6. Blocks that are marked free in the free-space list cannot be allocated to any file. Similarly, blocks that
are marked in use in the free-space list must be allocated to exactly one file.
*/
    if (error_code > 0) {
        fprintf(stderr, "Error: File system in %s is inconsistent (error code: %d)\n", new_disk_name, error_code);
        return;
    }
    fprintf(stderr, "\nMounted: %s\n", new_disk_name);
}


void fs_create(char name[5], int size) {
    printf("name: ");
    for (int i = 0; i < FILENAME_LENGTH; i++) {
        printf("%02x", name[i]);
    }
    printf(" | %s\n", name);
    printf("size: %d\n", size);
   
    int inodeAvailable = -1;
    for (unsigned int i = 0; i < INODES_LENGTH; i++) {
        // inode not in use
        if ( (disk->superblock->inode[i].used_size & 0x80) == 0) {
            inodeAvailable = i;
            break;
        }
    }
    if (inodeAvailable == -1) {
        fprintf(stderr, "Error: Superblock in disk %s is full, cannot create %s\n", disk->name, name);
        return;
    }
    
    // Check uniqueness of new file to the files in the current directory
    if ( (name[0] == '.' && name[1] == '\0') || (name[0] == '.' && name[1] == '.' && name[2] == '\0') ) {
       fprintf(stderr, "Error: File or directory %s already exists\n", name);
       return;
    }
    Filenames_node *node = disk->filenames[disk->current_dir];
    while (node != NULL) {
        if (strncmp_case(node->name, name, FILENAME_LENGTH) == 0) {
            fprintf(stderr, "Error: File or directory %s already exists\n", name);
            return;
        }
        node = node->next;
    }
 
    // If its a file, ensure there are enough available blocks by checking the superblock
    uint8_t start_block = 0;
    if (size > 0) {
        int contiguous_count = 0;
        for (unsigned int i = 0; i < FREE_BLOCK_LIST_SIZE; i++) {   // Iterate through each byte
            for (unsigned int j = 0; j < 8; j++) {  // Iterate throug each bit
                uint8_t bit = disk->superblock->free_block_list[i] >> j & 0x01;
                if (bit == 0) {
                    if (contiguous_count == 0) {
                        start_block = i*8 + j;
                    }
                    contiguous_count++;
                } else {
                    contiguous_count = 0;
                }
                if (contiguous_count == size) {
                    break;
                }
            }
            if (contiguous_count == size) {
                break;
            }
        }

        if (contiguous_count != size) {
            fprintf(stderr, "Error: Cannot allocate %d blocks on %s\n", size, name);
            return;
        }
    }

    // Append the new filename to the head of the list
    Filenames_node *new_node = malloc(sizeof(Filenames_node));
    memcpy(new_node->name, name, FILENAME_LENGTH);
    new_node->next = disk->filenames[disk->current_dir];
    disk->filenames[disk->current_dir] = new_node;
    
    // Create the inode for the file
    Inode *inode = malloc(sizeof(Inode));
    memcpy(inode->name, name, FILENAME_LENGTH);
    inode->used_size = 0x80 | size;
    inode->start_block = start_block;
    inode->dir_parent = disk->current_dir;
    if (disk->current_dir == 126)
        inode->dir_parent += 1;
    if (size == 0)
        inode->dir_parent |= 0x80;
    
    printf("\n");
    show_inode(inode);
   

    FILE *file = fopen(disk->name, "r+b");
    // Write the allocated blocks to the free_block_list space
    char buffer[FREE_BLOCK_LIST_SIZE];
    fread(buffer, 1, FREE_BLOCK_LIST_SIZE, file);
    int buffer_index = start_block/8;
    int bit_offset = start_block%8;
    for (int i = 0; i < size; i++) {
       buffer[buffer_index] |= 1 << bit_offset;
       bit_offset++;
       if (bit_offset == 8) {
           bit_offset = 0;
           buffer_index++;
       }
    }

    fseek(file, 0, SEEK_SET); // Go back to the beginning of the file
    fwrite(buffer, 1, FREE_BLOCK_LIST_SIZE, file);
    fclose(file);
}

void fs_delete(char name[5]) {}
void fs_read(char name[5], int block_num) {}
void fs_write(char name[5], int block_num) {}
void fs_buff(char buff[1024]) {}
void fs_ls(void) {}
void fs_resize(char name[5], int new_size) {}
void fs_defrag(void) {}
void fs_cd(char name[5]) {
    // loop through all inodes comparing the names and checking if the parent matches the current directory
}

void trim_name(char name[5]) {
    // Handle leading spaces
    int start = 0;
    while (start < FILENAME_LENGTH && name[start] == ' ') {
        start++;
    }
    
    if (start > 0) {
        for (int i = start; i < FILENAME_LENGTH; i++) {
            name[i-start] = name[i];
        }
        // Set the rest to \0
        for (int i = FILENAME_LENGTH-start; i < FILENAME_LENGTH; i++) {
            name[i] = ' ';
        }
    }

    // Handle trailing spaces
    int end = FILENAME_LENGTH-1;
    while (end >= 0 && (name[end] == ' ' || name[end] == '\0')) {
        name[end] = '\0';
        end--;
    }
}

unsigned int str_to_uint(char *str, unsigned int length) {
    unsigned int size = 0;
    for (int i = 0; i < length; i++) {
        if (str[i] == '\0')
            break;
        if (str[i] < 48 || str[i] > 57) {
            return -1;
        }
        size *= 10;
        size += (int)(str[i]-48);
    }
    return size;
}

// 0 for success (like strncmp) and 1 for failure
int strncmp_case(const char *s1, const char *s2, unsigned int n) {
    for (unsigned int i = 0; i < n; i++) {
        // Convert both to lower
        char c1 = s1[i] >= 65 && s1[i] <= 90 ? s1[i]+32 : s1[i];
        char c2 = s2[i] >= 65 && s2[i] <= 90 ? s2[i]+32 : s2[i];
        if (c1 != c2)
            return 1;
    }
    return 0;
}


int main(int argc, char *argv[]) {
    printf("================\n\n");

    if (argc < 2) {
        fprintf(stderr, "Error: No cmds file given\n");
        return 1;
    }

    FILE *cmds = fopen(argv[1], "r");
    if (cmds == NULL) {
        fprintf(stderr, "Error: reading cmds file\n");
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
            fprintf(stderr, "Error: unknown command\n");
            continue;
        }
    
        // Convert the command to lower
        if (tokens[0][0] >= 65 && tokens[0][0] <= 90)
            tokens[0][0] += 32;
        // Run the function specified by the second argument
        switch (tokens[0][0]) {
            case 'm':
                if (tokenCount < 2) {
                    fprintf(stderr, "No disk name given\n");
                    break;
                }
                fs_mount(tokens[1]);
                break;
            // --------------------------------
            case 'c':
                if (tokenCount < 2) {
                    fprintf(stderr, "No file name given\n");
                    break;
                }
                if (tokenCount < 3) {
                    fprintf(stderr, "No file size given\n");
                    break;
                }
                unsigned int size = str_to_uint(tokens[2], 3);
                if (size == -1) {
                    fprintf(stderr, "Error: size\n");
                }
                // Create a name of size 5 and copy token[1] argument filling the rest of the spaces with \0
                char name[FILENAME_LENGTH];
                int reached_end_of_arg = 0;
                for (unsigned int i = 0; i < FILENAME_LENGTH; i++) {
                    if (tokens[1][i] == '\0')
                        reached_end_of_arg = 1;
                    if (reached_end_of_arg)
                        name[i] = '\0';
                    else
                        name[i] = tokens[1][i];
                }
                fs_create(name, size);
                break;
            // --------------------------------
            default:
                fprintf(stderr, "Error: unknown command\n");
        }


        printf("------------\n");
    }

    fclose(cmds);     
    return 0;
}
