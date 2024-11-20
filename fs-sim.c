#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_CMD_ARGS 3          // How many arguments in a line will be counted sparated by spaces
#define SUPERBLOCK_SIZE 1024    // Bytes
#define FILE_MAX_SIZE 127       // Blocks (KB)
#define FILE_NAME_SIZE 5        // Bytes

FILE *disk;


void fs_mount(char *new_disk_name) {
    disk = fopen(new_disk_name, "r");
    if (disk == NULL) {
        printf("Error: File '%s' does not exist.\n", new_disk_name);
        return;
    }
    printf("done: %s\n", new_disk_name);
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
        printf("------------\n");
        printf("line: %s\n", buffer);
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
        // Return error if the first token (the command) is not exactly one character in length
        if (tokens[0][1] != '\0') {
            printf("Error: unknown command\n");
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
    }

    fclose(cmds);     
    return 0;
}
