#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fs-sim.h"

#define BLOCK_SIZE 1024         // bytes
#define FREE_BLOCK_LIST_SIZE 16 // bytes
#define INODES_LENGTH 126       // count
#define INODE_SIZE 8            // bytes
#define FILENAME_LENGTH 5       // bytes

Disk *disk;

void show_filenames() {
    printf("---- filenames ----\n");
    Filenames_node *node;
    for (unsigned int i = 0; i < INODES_LENGTH+2; i++) {
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

int nodisk = 1;

void fs_mount(char *new_disk_name) {
    if (nodisk) {
        disk = malloc(sizeof(Disk));
        disk->mounted = 0;
        nodisk = 0;
    }
    FILE *file = fopen(new_disk_name, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: File '%s' does not exist.\n", new_disk_name);
        return;
    }
    disk->current_dir = 127;
    
    // Get the superblock
    char buffer[BLOCK_SIZE];
    fread(buffer, 1, BLOCK_SIZE, file);
    fclose(file);

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
        
        //show_inode(inode);

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
        if (inode_type == 0 && (inode->start_block < 1 || inode->start_block > 127)) {
            error_code = 2;
            break;
        }
        if (inode_type == 0 && inode->start_block+inode_size-1 > 127) {
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
        // Create and organize the filenames list to track which are used for uniqueness
        Filenames_node *node = disk->filenames[inode_parent];
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
        new_node->next = disk->filenames[inode_parent];
        disk->filenames[inode_parent] = new_node;
       
    }
   


    // Error 6: Blocks that are marked free in the free-space list cannot be allocated to any file
    // Similarly, blocks that are marked in use in the free-space list must be allocated to exactly one file.

    // Build a temp free block list with the inodes to compare with the real block list
    uint8_t compare_freeBlockList[FREE_BLOCK_LIST_SIZE];
    compare_freeBlockList[0] = 0x80;
    for (int i = 1; i < FREE_BLOCK_LIST_SIZE;i ++)
        compare_freeBlockList[i] = 0;

    Inode tmp_inode;
    for (int i = 0; i < INODES_LENGTH; i++) {
        tmp_inode = disk->superblock->inode[i];
        if (tmp_inode.name[0] == '\0')
            continue;
        if ( (tmp_inode.dir_parent & 0x80) != 0)
            continue;

        int tmp_inodeSize = tmp_inode.used_size & 0x7f;
        for (int j = 0; j < tmp_inodeSize; j++) {
            int offset = tmp_inode.start_block + j;
            int byte_offset = offset / 8;
            int bit_offset = offset % 8;
            int bit = 0x80 >> bit_offset;
            compare_freeBlockList[byte_offset] |= bit;
        }
    }

    printf("compare: ");
    for (int i = 0; i < 16; i++)
        printf("%x", compare_freeBlockList[i]);

    for (int i = 0; i < FREE_BLOCK_LIST_SIZE; i++) {
        if (disk->superblock->free_block_list[i] != compare_freeBlockList[i]) {
            error_code = 6;
            break;
        }
    }

        
    
    printf("\n");
    show_filenames();

   if (error_code > 0) {
        fprintf(stderr, "Error: File system in %s is inconsistent (error code: %d)\n", new_disk_name, error_code);
        return;
    }

    disk->name = calloc(strlen(new_disk_name)+1, sizeof(char));
    strcpy(disk->name, new_disk_name);
    disk->mounted = 1;
}


void fs_create(char name[5], int size) {
    if (!disk->mounted)
        return;
    printf("name: ");
    printf("%.5s\n", name);
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
        fprintf(stderr, "Error: Superblock in disk %s is full, cannot create %.5s\n", disk->name, name);
        return;
    }
    
    // Check uniqueness of new file to the files in the current directory
    if ( (name[0] == '.' && name[1] == '\0') || (name[0] == '.' && name[1] == '.' && name[2] == '\0') ) {
       fprintf(stderr, "Error: File or directory %.5s already exists\n", name);
       return;
    }
    Filenames_node *node = disk->filenames[disk->current_dir];
    while (node != NULL) {
        if (strncmp_case(node->name, name, FILENAME_LENGTH) == 0) {
            fprintf(stderr, "Error: File or directory %.5s already exists\n", name);
            return;
        }
        node = node->next;
    }

    // Read the free block list
    FILE *file1 = fopen(disk->name, "rb");
    if (!file1) {
        return;
    }
    printf("name : %s\n", disk->name);
    uint8_t free_block_list[FREE_BLOCK_LIST_SIZE];
    fread(free_block_list, 1, FREE_BLOCK_LIST_SIZE, file1);
        printf("asdf\n");
    fclose(file1);
    // If its a file, ensure there are enough available blocks by checking the superblock
    uint8_t start_block = 0;
    if (size > 0) {
        int contiguous_count = 0;
        int start_bit = 1;     // Skip the first bit when starting the iterations
        for (unsigned int i = 0; i < FREE_BLOCK_LIST_SIZE; i++) {   // Iterate through each byte
            for (unsigned int j = start_bit; j < 8; j++) {  // Iterate throug each bit
                uint8_t bit = free_block_list[i] << j & 0x80;
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
            start_bit = 0;
        }

        if (contiguous_count != size) {
            fprintf(stderr, "Error: Cannot allocate %d blocks on %s\n", size, disk->name);
            return;
        }
    }
    printf("start_block: %02x\n", start_block);
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
    if (size == 0)    // If the inode is a directory
        inode->dir_parent |= 0x80;  // State it as a directory in the dir_parent variable
    disk->superblock->inode[inodeAvailable] = *inode;
    
    printf("\n");
    //show_inode(inode);
   

    FILE *file = fopen(disk->name, "r+b");
    if (!file) {
        return;
    }

    // Write the allocated blocks to the free_block_list space if its a file
    if (size > 0) {
        uint8_t buffer[FREE_BLOCK_LIST_SIZE];
        fread(buffer, 1, FREE_BLOCK_LIST_SIZE, file);
        int buffer_index = start_block/8;
        int bit_offset = start_block%8;
        for (int i = 0; i < size; i++) {
           buffer[buffer_index] |= 0x80 >> bit_offset;
           bit_offset++;
           if (bit_offset == 8) {
               bit_offset = 0;
               buffer_index++;
           }
        }
        fseek(file, 0, SEEK_SET);                       // Go back to the beginning of the file
        fwrite(buffer, 1, FREE_BLOCK_LIST_SIZE, file);  // Write the buffer back with the new values
    }

    // Write the new inode to the superblock
    char inode_buffer[8];
    memcpy(inode_buffer, inode->name, FILENAME_LENGTH);
    inode_buffer[5] = inode->used_size;
    inode_buffer[6] = inode->start_block;
    inode_buffer[7] = inode->dir_parent;
    unsigned int inode_offset = 16 + inodeAvailable*8;
    fseek(file, inode_offset, SEEK_SET);  // Go to the inode in the superblock
    fwrite(inode_buffer, 1, INODE_SIZE, file);

    fclose(file);
}


void delete_children(uint8_t dir, int killcurrent) {
    printf("dir before: %d\n", dir);
    // Find if the current dir has any children
    for (unsigned int i = 0; i < INODES_LENGTH; i++) {
        if (disk->superblock->inode[i].name[0] == 0)
            continue;
        int parent = disk->superblock->inode[i].dir_parent & 0x7f;
        if (parent == dir) {
            printf("i: %d\n", i);
            delete_children(i, killcurrent);
        }
    }
    printf("dir after %d\n", dir);
    // Dont kill the root directory or the current directory
    if (dir == 127 || (dir == disk->current_dir && killcurrent == 0))
        return;
    
    int inode_index = dir;
    Inode *inode = &disk->superblock->inode[inode_index];
    uint8_t size = inode->used_size & 0x7f;
    
    FILE *file = fopen(disk->name, "r+b");
    if (!file)
        return;
    uint8_t zero_buff[1024];
    for (int i = 0; i < 1024; i++) {
        zero_buff[i] = 0;
    }

    // If its a file then delete the blocks
    if ( (inode->dir_parent & 0x80) == 0) {
        for (unsigned int i = 0; i < size; i++) {
            unsigned int delete_block_offset = (inode->start_block+i)*BLOCK_SIZE;
            fseek(file, delete_block_offset, SEEK_SET);
            fwrite(zero_buff, 1, BLOCK_SIZE, file);
        }
    

        uint8_t free_block_list[FREE_BLOCK_LIST_SIZE];
        fseek(file, 0, SEEK_SET);
        fread(free_block_list, 1, FREE_BLOCK_LIST_SIZE, file);
        
        // Delete the bits in the free block list
        for (unsigned int i = inode->start_block; i < inode->start_block+size; i++) {
            int byte_offset = i/8;
            int bit_offset = i%8;
            uint8_t and_mask_1 = 0xff >> (bit_offset+1);
            uint8_t and_mask_2 = 0xff << (8-bit_offset);
            free_block_list[byte_offset] &= (and_mask_1 | and_mask_2);
        }

        fseek(file, 0, SEEK_SET);
        fwrite(free_block_list, 1, FREE_BLOCK_LIST_SIZE, file);
    }

    // Delete the inode
    uint8_t zero_inode_buff[INODE_SIZE];
    for (int i = 0; i < INODE_SIZE; i++)
        zero_inode_buff[i] = 0;
    unsigned int delete_inode_offset = FREE_BLOCK_LIST_SIZE + inode_index*INODE_SIZE;
    fseek(file, delete_inode_offset, SEEK_SET);
    fwrite(zero_inode_buff, 1, INODE_SIZE, file);

    fclose(file);

    // Delete the name in the filenames struct
    int filename_index = inode->dir_parent & 0x7f;
    Filenames_node *node = disk->filenames[filename_index];
    printf("node name: %.5s, inode name: %.5s\n", node->name, inode->name);
        // Delete the head if its the same name
    if (strncmp_case(node->name, inode->name, 5) == 0) {
        disk->filenames[filename_index] = node->next;
        free(node);
    } else {
        printf("next name: %.5s\n", node->next->name);
        while (node->next != NULL) {
            if (strncmp_case(node->next->name, inode->name, 5) == 0) {
                Filenames_node *tmp = node->next;
                node->next = node->next->next;
                free(tmp);
                break;
            }
            node = node->next;
        }
    }
    

    // Delete the inode from the disk struct superblock
    inode = NULL;
}

void fs_delete(char name[5]) {
    if (!disk->mounted)
        return;
    printf("show filenames 1\n");
    //show_filenames();
    if (name[0] == '.' && name[1] == '\0') {
        delete_children(disk->current_dir, 1);
        printf("show filenames 2\n");
        //show_filenames();
        return;
    }
    
    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
        if (disk->current_dir == 127) {
            delete_children(127, 0);
            return;
        } else {
            uint8_t parent_dir = disk->superblock->inode[disk->current_dir].dir_parent & 0x7f;
            printf("parent dir: %d\n", parent_dir);
            delete_children(parent_dir, 0);
            return;
        }
    }
    // Find the inode
    int inode_index = -1;
    Inode inode;
    for (unsigned int i = 0; i < INODES_LENGTH; i++) {
        inode = disk->superblock->inode[i];
        int isParent = (inode.dir_parent & 0x7f) == disk->current_dir;
        int sameName = strncmp_case(inode.name, name, FILENAME_LENGTH) == 0;
        if (isParent && sameName) {
            inode_index = i;
            break;
        }
    }

    if (inode_index == -1) {
        fprintf(stderr, "Error: File %.5s does not exist\n", name);
        return;
    }

    // Recursively find its children and children of children ... until there is no more children then delete the last child and work back up
    printf("inode index: %d\n", inode_index);
    delete_children(inode_index, 0);
printf("show filenames 2\n");
        //show_filenames();
        
}

void fs_read(char name[5], int block_num) {
    if (!disk->mounted)
        return;
    // Find the inode
    int inode_index = -1;
    Inode inode;
    for (unsigned int i = 0; i < INODES_LENGTH; i++) {
        inode = disk->superblock->inode[i];
        int isParent = (inode.dir_parent & 0x7f) == disk->current_dir;
        int sameName = strncmp_case(inode.name, name, FILENAME_LENGTH) == 0;
        if (isParent && sameName) {
            inode_index = i;
            break;
        }
    }
    if (inode_index == -1) {
        fprintf(stderr, "Error: File %.5s does not exist\n", name);
        return;
    }
    
    printf("used size: %02x\n", inode.used_size & 0x7f);
    printf("block_num: %d\n", block_num);
    // Find the block in the inode
    if (block_num > (inode.used_size & 0x7f)-1) {
        fprintf(stderr, "Error: %.5s does not have block %d\n", name, block_num);
        return;
    }

    // Read the block in the inode
    FILE *file = fopen(disk->name, "rb");
    if (!file)
        return;
    uint8_t buff[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++)
        buff[i] = 0;
    unsigned int seek_offset = BLOCK_SIZE*inode.start_block + BLOCK_SIZE*block_num;
    printf("seek_offset: %d\n", seek_offset/16);
    fseek(file, seek_offset, SEEK_SET);
    fread(buff, 1, BLOCK_SIZE, file);
    fclose(file);
    fs_buff(buff);
}

void fs_write(char name[5], int block_num) {
    if (!disk->mounted)
        return;
    if ( (name[0] == '.' && name[1] == '\0') || (name[0] == '.' && name[1] == '.' && name[2]== '\0') ) {
        fprintf(stderr, "Error: File %.5s does not exist\n", name);
        return;
    }

    // Find the inode
    int inode_index = -1;
    Inode inode;
    for (unsigned int i = 0; i < INODES_LENGTH; i++) {
        inode = disk->superblock->inode[i];
        int isParent = (inode.dir_parent & 0x7f) == disk->current_dir;
        int sameName = strncmp_case(inode.name, name, FILENAME_LENGTH) == 0;
        int isFile = (inode.dir_parent & 0x80) == 0;
        if (isParent && sameName && isFile) {
            inode_index = i;
            break;
        }
    }
    if (inode_index == -1) {
        fprintf(stderr, "Error: File %.5s does not exist\n", name);
        return;
    }
    
    printf("used size: %02x\n", inode.used_size & 0x7f);
    printf("block_num: %d\n", block_num);
    // Find the block in the inode
    if (block_num > (inode.used_size & 0x7f)-1) {
        fprintf(stderr, "Error: %.5s does not have block %d\n", name, block_num);
        return;
    }

    // Write the buffer to the block in the inode
    FILE *file = fopen(disk->name, "r+b");
    if (!file)
        return;
    unsigned int seek_offset = BLOCK_SIZE*inode.start_block + BLOCK_SIZE*block_num;
    printf("seek_offset: %d\n", seek_offset/16);
    fseek(file, seek_offset, SEEK_SET);
    fwrite(disk->buffer, 1, BLOCK_SIZE, file);
    fclose(file);
}

void fs_buff(uint8_t buff[1024]) {
    if (!disk->mounted)
        return;
    // Flush the current buffer
    for (unsigned int i = 0; i < 1024; i++) {
        disk->buffer[i] = 0;
    }

    for (unsigned int i = 0; i < 1024; i++) {
        disk->buffer[i] = buff[i];
    }


    // Print buff (TEMP)
    for (unsigned int i = 0; i < 1024; i++) {
        printf("%02x", disk->buffer[i]);
    }
    printf("\n");
}

void fs_ls(void) {
    if (!disk->mounted)
        return;
    // Print the current dir and the parent dir
    unsigned int current_dir_count = 0;
    unsigned int parent_dir_count = 0;
    int parent_dir = -1;
    if (disk->current_dir != 127) {
        parent_dir = disk->superblock->inode[disk->current_dir].dir_parent & 0x7f;
    }
    for (unsigned int i = 0; i < INODES_LENGTH; i++) {
        if (disk->superblock->inode[i].name[0] == '\0')
            continue;
        if ( (disk->superblock->inode[i].dir_parent & 0x7f) == disk->current_dir) {
            current_dir_count++;
        }
        if (parent_dir != -1 && (disk->superblock->inode[i].dir_parent & 0x7f) == parent_dir) {
            parent_dir_count++;
        }
    }
    if (parent_dir == -1)
        parent_dir_count = current_dir_count;

    printf(".     %3d\n", current_dir_count+2);
    printf("..    %3d\n", parent_dir_count+2);
    


    Inode *inode;
    for (unsigned int i = 0; i < INODES_LENGTH; i++) {
        inode = &disk->superblock->inode[i];
        if ( (inode->dir_parent & 0x7f) == disk->current_dir) {
            printf("%-5.5s", inode->name);
            if ( (inode->dir_parent & 0x80) == 0) {// If it is type file print the file size
                printf(" %3d KB\n", inode->used_size & 0x7f);
            } else {                               // If it is type dir print the number of children
                unsigned int child_count = 0;
                for (unsigned int j = 0; j < INODES_LENGTH; j++) {
                    if ( (disk->superblock->inode[j].dir_parent & 0x7f) == i) {
                        child_count++;
                    }
                 }
                 printf(" %3d\n", child_count);
            }
        }
    }
}

void fs_resize(char name[5], int new_size) {
    if (!disk->mounted)
        return;
    if ( (name[0] == '.' && name[1] == '\0') || (name[0] == '.' && name[1] == '.' && name[2]== '\0') ) {
        fprintf(stderr, "Error: File %.5s does not exist\n", name);
        return;
    }

    // Find the inode
    int inode_index = -1;
    Inode *inode;
    for (unsigned int i = 0; i < INODES_LENGTH; i++) {
        inode = &disk->superblock->inode[i];
        int isParent = (inode->dir_parent & 0x7f) == disk->current_dir;
        int sameName = strncmp_case(inode->name, name, FILENAME_LENGTH) == 0;
        int isFile = (inode->dir_parent & 0x80) == 0;
        if (isParent && sameName && isFile) {
            inode_index = i;
            break;
        }
    }
    if (inode_index == -1) {
        fprintf(stderr, "Error: File %.5s does not exist\n", name);
        return;
    }
    
    int current_size = inode->used_size & 0x7f;
    // If new_size is less than the current inode size
    if (new_size < current_size) {
        FILE *file = fopen(disk->name, "r+b");
        if (!file)
            return;
        unsigned int seek_block_offset = BLOCK_SIZE*inode->start_block + BLOCK_SIZE*new_size;
        unsigned int seek_freeSpaceList_offset = inode->start_block + new_size;
        uint8_t block_zero[1024];
        for (int i = 0; i < 1024; i++)
            block_zero[i] = 0;
        uint8_t buff[1];
        for (unsigned int i = 0; i < current_size-new_size; i++) {
            // Zero out the tailing blocks
            printf("seek_offset: %d\n", seek_block_offset);
            fseek(file, seek_block_offset, SEEK_SET);
            fwrite(block_zero, 1, BLOCK_SIZE, file);
            seek_block_offset += BLOCK_SIZE;
            // Resize in the free block list
            // GET THE BYTE, OR IT, THEN SET THE NEW VALUE
            int byte_offset = seek_freeSpaceList_offset / 8;
            int bit_offset = seek_freeSpaceList_offset % 8;
            fseek(file, byte_offset, SEEK_SET);
            fread(buff, 1, 1, file);
            uint8_t and_mask_1 = 0xff >> (bit_offset+1);
            uint8_t and_mask_2 = 0xff << (8-bit_offset);
            buff[0] &= (and_mask_1 | and_mask_2);
            printf("buff: %2x\n", buff[0]);
            fseek(file, byte_offset, SEEK_SET);
            fwrite(buff, 1, 1, file);
            seek_freeSpaceList_offset += 1;
        }

        // Set the inodes size to the new size
        inode->used_size = 0x80 | new_size;
        printf("new size: %02x, new used size: %02x\n", new_size, inode->used_size);
        unsigned int seek_inode_usedSize_offset = FREE_BLOCK_LIST_SIZE + inode_index*INODE_SIZE + 5;
        fseek(file, seek_inode_usedSize_offset, SEEK_SET);
        fwrite(&inode->used_size, 1, 1, file);
        fclose(file);
    }
    // If new_size is greater than current inode size
    else if (new_size > current_size) {
        // Read the free block list
        // check if there is space to increase
        FILE *file = fopen(disk->name, "r+b");
        if (!file)
            return;
        uint8_t free_block_list[FREE_BLOCK_LIST_SIZE];
        fread(free_block_list, 1, FREE_BLOCK_LIST_SIZE, file);
        int has_space = 1;
        for (unsigned int i = 0; i < new_size-current_size; i++) {
            int offset = inode->start_block+current_size+i+1;
            printf("offset: %d\n", offset);
            int byte_offset = offset/8;
            int bit_offset = offset%8;
            int bit = free_block_list[byte_offset] & (0x80 >> bit_offset);
            if (bit != 0) {
                has_space = 0;
                break;
            }
        }
        printf("has_space: %d\n", has_space);
        // Increase the size if there is space to
        if (has_space) {
            int offset = inode->start_block+current_size;
            int buffer_index = offset/8;
            int bit_offset = offset%8;
            for (int i = 0; i < new_size-current_size; i++) {
               free_block_list[buffer_index] |= 0x80 >> bit_offset;
               bit_offset++;
               if (bit_offset == 8) {
                   bit_offset = 0;
                   buffer_index++;
               }
            }
            fseek(file, 0, SEEK_SET);                       // Go back to the beginning of the file
            fwrite(free_block_list, 1, FREE_BLOCK_LIST_SIZE, file);  // Write the buffer back with the new values
            // Set the inode to the new size
            inode->used_size = 0x80 | new_size;
            printf("new size: %02x, new used size: %02x\n", new_size, inode->used_size);
            unsigned int seek_inode_usedSize_offset = FREE_BLOCK_LIST_SIZE + inode_index*INODE_SIZE + 5;
            fseek(file, seek_inode_usedSize_offset, SEEK_SET);
            fwrite(&inode->used_size, 1, 1, file);
        } else {
            // If there is no space check if there is space in another location
            uint8_t new_start_block = 0;
            int contiguous_count = 0;
            int start_bit = 1;     // Skip the first bit when starting the iterations
            for (unsigned int i = 0; i < FREE_BLOCK_LIST_SIZE; i++) {   // Iterate through each byte
                for (unsigned int j = start_bit; j < 8; j++) {  // Iterate throug each bit
                    uint8_t bit = free_block_list[i] << j & 0x80;
                    if (bit == 0) {
                        if (contiguous_count == 0) {
                            new_start_block = i*8 + j;
                        }
                        contiguous_count++;
                    } else {
                        contiguous_count = 0;
                    }
                    if (contiguous_count == new_size) {
                        break;
                    }
                }
                if (contiguous_count == new_size) {
                    break;
                }
                start_bit = 0;
            }
            // If there is no space then return
            if (contiguous_count != new_size) {
                fprintf(stderr, "Error: File %.5s cannot expand to size %d\n", inode->name, new_size);
                return;
            }
            // If there is space move the file to the new space
            printf("new_start_block: %02x\n", new_start_block);
            uint8_t block_buffer[1024];
            uint8_t zero_buffer[1024];
            for (int i = 0; i < 1024; i++)
                zero_buffer[i] = 0;
            for (int i = 0; i < current_size; i++) {
                // Get the current block
                unsigned int start_offset = BLOCK_SIZE*inode->start_block + BLOCK_SIZE*i;
                fseek(file, start_offset, SEEK_SET);
                fread(block_buffer, 1, BLOCK_SIZE, file); 
                // Copy to the new location
                unsigned int new_start_offset = BLOCK_SIZE*new_start_block + BLOCK_SIZE*i;
                fseek(file, new_start_offset, SEEK_SET);
                fwrite(block_buffer, 1, BLOCK_SIZE, file);
                // Zero out the old location
                fseek(file, start_offset, SEEK_SET);
                fwrite(zero_buffer, 1, BLOCK_SIZE, file);
            }
            // Update the free block list
                // Set the bits to 0
            uint8_t buff[1];
            for (int i = 0; i < current_size; i++) {
                int byte_offset = (inode->start_block+i) / 8;
                int bit_offset = (inode->start_block+i) % 8;
                fseek(file, byte_offset, SEEK_SET);
                fread(buff, 1, 1, file);
                uint8_t and_mask_1 = 0xff >> (bit_offset+1);
                uint8_t and_mask_2 = 0xff << (8-bit_offset);
                buff[0] &= (and_mask_1 | and_mask_2);
                printf("buff: %2x\n", buff[0]);
                fseek(file, byte_offset, SEEK_SET);
                fwrite(buff, 1, 1, file);
            }

            // Update the inode by changing its start block and size
            inode->start_block = new_start_block;
            inode->used_size = 0x80 | new_size;
            // Write this info to the inode in the superblock
            unsigned int inode_offset = FREE_BLOCK_LIST_SIZE + inode_index*INODE_SIZE + 5;
            fseek(file, inode_offset, SEEK_SET);
            uint8_t write_buffer[2] = {inode->used_size, inode->start_block};
            fwrite(write_buffer, 1, 2, file);

            // Set the new bits to 1
            for (int i = 0; i < new_size; i++) {
                int byte_offset = (inode->start_block+i) / 8;
                int bit_offset = (inode->start_block+i) % 8;
                fseek(file, byte_offset, SEEK_SET);
                fread(buff, 1, 1, file);
                buff[0] |= 0x80 >> bit_offset;
                printf("buff: %2x\n", buff[0]);
                fseek(file, byte_offset, SEEK_SET);
                fwrite(buff, 1, 1, file);
            }
        }
        fclose(file);
    }
}

void fs_defrag(void) {
    if (!disk->mounted)
        return;
    int inodes_start[FREE_BLOCK_LIST_SIZE*8];
    for (int i = 0; i < FREE_BLOCK_LIST_SIZE*8; i++) {
        inodes_start[i] = -1;
    }
    for (int i = 0; i < INODES_LENGTH; i++) {
        inodes_start[disk->superblock->inode[i].start_block] = i;
    }

    FILE *file = fopen(disk->name, "r+b");
    if (!file)
        return;
    uint8_t zero_buff[1024];
    for (int i = 0; i < 1024; i++)
        zero_buff[i] = 0;

    unsigned int totalSize = 0;

    Inode *inode;
    int new_startBlock = 1;
    int i = 1;
    while (i < FREE_BLOCK_LIST_SIZE*8) {
        if (inodes_start[i] == -1) {
            i++;
            continue;
        }
        int inode_current_startBlock = i;
        int inode_index = inodes_start[i];
        inode = &disk->superblock->inode[inode_index];
        int inode_size = inode->used_size & 0x7f;
        totalSize += inode_size;
        if (new_startBlock == inode_current_startBlock) {
            new_startBlock += inode_size;
            i += inode_size;
            continue;
        }
        printf("inode_current_startBlock: %d, new_startBlock: %d, inode_index: %d, inode_size: %d\n", inode_current_startBlock, new_startBlock, inode_index, inode_size);
        
        // Update the inodes start block to the new location
        inode->start_block = new_startBlock;
        // Write this new information to the superblock at the inodes location
        unsigned int inode_startBlock_offset = FREE_BLOCK_LIST_SIZE + inode_index*INODE_SIZE + 6;
        fseek(file, inode_startBlock_offset, SEEK_SET);
        fwrite(&new_startBlock, 1, 1, file);

        // Move the data to the new location
        uint8_t block_buff[1024];
        for (int j = 0; j < inode_size; j++) {
            // Read current block
            unsigned int current_block_offset = (inode_current_startBlock + j)*BLOCK_SIZE;
            printf("     current_block_offset: %d\n", current_block_offset/16);
            fseek(file, current_block_offset, SEEK_SET);
            fread(block_buff, 1, BLOCK_SIZE, file);
            // Write to new block
            unsigned int new_block_offset = (new_startBlock + j)*BLOCK_SIZE;
            printf("     new_block_offset %d\n", new_block_offset/16);
            fseek(file, new_block_offset, SEEK_SET);
            fwrite(block_buff, 1, BLOCK_SIZE, file);
            // Zero the current block
            fseek(file, current_block_offset, SEEK_SET);
            fwrite(zero_buff, 1, BLOCK_SIZE, file);
        }
        new_startBlock += inode_size;
        i += inode_size;
    }

    // Update the free block list
    uint8_t free_block_list[FREE_BLOCK_LIST_SIZE];
    free_block_list[0] = 0x80;
    for (int i = 1; i < FREE_BLOCK_LIST_SIZE; i++)
        free_block_list[i] = 0;

    for (int i = 1; i < totalSize+1; i++) {
        int byte_offset = i/8;
        int bit_offset = i%8;
        free_block_list[byte_offset] |= 0x80 >> bit_offset;
    }
    
    fseek(file, 0, SEEK_SET);
    fwrite(free_block_list, 1, FREE_BLOCK_LIST_SIZE, file);
    fclose(file);
}

void fs_cd(char name[5]) {
    if (!disk->mounted)
        return;
    // Skip cd into '.'
    if (name[0] == '.' && name[1] == '\0')
        return;
    // Skip cd into '..' if the current directory is root otherwise go up one level
    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
        if (disk->current_dir == 127) {
            return;
        } else {
            uint8_t parent_dir = disk->superblock->inode[disk->current_dir].dir_parent;
            disk->current_dir = parent_dir & 0x7f;
            return;
        }
    }

    Inode *inode;
    for (unsigned int i = 0; i < INODES_LENGTH; i++) {
        inode = &disk->superblock->inode[i];
        int isParent = (inode->dir_parent & 0x7f) == disk->current_dir;
        int sameName = strncmp_case(inode->name, name, FILENAME_LENGTH) == 0;
        if (isParent && sameName) {
           disk->current_dir = i;
           printf("new dir: %d\n", i);
           return;
        }
    }

    fprintf(stderr, "Error: Directory %.5s does not exist\n", name);
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
        if (s1[i] == '\0' && s2[i] == '\0')
            return 0;
        if (s1[i] == '\0' || s2[i] == '\0')
            return 1;
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
    if (!cmds)
        return 1;
    if (cmds == NULL) {
        fprintf(stderr, "Error: reading cmds file\n");
        return 1;
    }

    char buffer[2048];
    while (fgets(buffer, sizeof(buffer), cmds) != NULL) {
        // Skip any line that is only a newline
        if (buffer[0] == '\n')
            continue;
        // Remove trailing newline character if it exists
        buffer[strcspn(buffer, "\n")] = '\0';
        // Split the buffer by spaces
        char *tokens[1024];
        char *token = strtok(buffer, " ");
        int tokenCount = 0;
        printf("tokens: ");
        while (token != NULL) {
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
                    break;
                }
                // Create a name of size 5 and copy token[1] argument filling the rest of the spaces with \0 
                char create_name[FILENAME_LENGTH];
                for (int i = 0; i < FILENAME_LENGTH; i++)
                    create_name[i] = 0;
                strncpy(create_name, tokens[1], FILENAME_LENGTH);
                fs_create(create_name, size);
                break;
            // --------------------------------
            case 'd':
                if (tokenCount < 2) {
                    fprintf(stderr, "No file name given\n");
                    break;
                }
                char delete_name[FILENAME_LENGTH];
                for (int i = 0; i < FILENAME_LENGTH; i++)
                    delete_name[i] = 0;
                strncpy(delete_name, tokens[1], FILENAME_LENGTH);
                fs_delete(delete_name);
                break;
            // --------------------------------
            case 'r':
                if (tokenCount < 2) {
                    fprintf(stderr, "No filename given\n");
                    break;
                }
                if (tokenCount < 3) {
                    fprintf(stderr, "No block number given\n");
                    break;
                }
                unsigned int block_num = str_to_uint(tokens[2], 3);
                if (block_num == -1) {
                    fprintf(stderr, "Error: blocknum\n");
                    break;
                }
                char read_name[FILENAME_LENGTH];
                for (int i = 0; i < FILENAME_LENGTH; i++)
                    read_name[i] = 0;
                strncpy(read_name, tokens[1], FILENAME_LENGTH);
                fs_read(read_name, block_num);
                break;
            // --------------------------------
            case 'w':
                if (tokenCount < 2) {
                    fprintf(stderr, "No filename given\n");
                    break;
                }
                if (tokenCount < 3) {
                    fprintf(stderr, "No block number given\n");
                    break;
                }
                unsigned int write_block_num = str_to_uint(tokens[2], 3);
                if (write_block_num == -1) {
                    fprintf(stderr, "Error: blocknum\n");
                    break;
                }
                char write_name[FILENAME_LENGTH];
                for (int i = 0; i < FILENAME_LENGTH; i++)
                    write_name[i] = 0;
                strncpy(write_name, tokens[1], FILENAME_LENGTH);
                fs_write(write_name, write_block_num);
                break;
            // --------------------------------
            case 'b':
                if (tokenCount < 2) {
                    fprintf(stderr, "No buffer value was given\n");
                    break;
                }
                // Combine the rest of the tokens into one string
                printf("token Count: %d\n", tokenCount);
                char buff[1024];
                int tokenIndex = 1;
                int tokenCharIndex = 0;
                int inputSpace = 0;
                for (int i = 0; i < 1024; i++) {
                    if (tokenIndex > tokenCount-1) {
                        buff[i] = 0;
                    } else {
                        if (inputSpace) {
                            buff[i] = ' ';
                            inputSpace = 0;
                        } else {
                            buff[i] = tokens[tokenIndex][tokenCharIndex];
                            tokenCharIndex++;
                            if (tokens[tokenIndex][tokenCharIndex] == '\0') {
                                tokenIndex++;
                                tokenCharIndex = 0;
                                inputSpace = 1;
                            }
                        }
                    }
                }
                fs_buff((uint8_t *)buff);
                break;
            // --------------------------------
            case 'l':
                fs_ls();
                break;
            // --------------------------------
            case 'e':
                if (tokenCount < 2) {
                    fprintf(stderr, "No filename given\n");
                    break;
                }
                if (tokenCount < 3) {
                    fprintf(stderr, "No new size given\n");
                    break;
                }
                unsigned int new_size = str_to_uint(tokens[2], 3);
                if (new_size == -1) {
                    fprintf(stderr, "Error: newsize\n");
                    break;
                }
                char resize_name[FILENAME_LENGTH];
                for (int i = 0; i < FILENAME_LENGTH; i++)
                    resize_name[i] = 0;
                strncpy(resize_name, tokens[1], FILENAME_LENGTH);
                fs_resize(resize_name, new_size);
                break;
            // --------------------------------
            case 'o':
                fs_defrag();
                break;
            // --------------------------------
            case 'y':
                if (tokenCount < 2) {
                    fprintf(stderr, "No filename given\n");
                    break;
                }
                char cd_name[FILENAME_LENGTH];
                for (int i = 0; i < FILENAME_LENGTH; i++)
                    cd_name[i] = 0;
                strncpy(cd_name, tokens[1], FILENAME_LENGTH);
                fs_cd(cd_name);
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
