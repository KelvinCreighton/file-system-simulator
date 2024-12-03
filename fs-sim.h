typedef struct {
	char name[5];        // Name of the file/directory (not necessarily null terminated)
	uint8_t used_size;   // State of inode (0 = free) and size of the file
	uint8_t start_block; // Index of the first block of the file/directory
	uint8_t dir_parent;  // Type of inode (0 = file) and index of the parent inode
} Inode;

typedef struct {
	char free_block_list[16];
	Inode inode[126];
} Superblock;

typedef struct Filenames_node {
    char name[5];
    struct Filenames_node *next;
} Filenames_node;

typedef struct {
    char *name;
    Superblock *superblock;
    uint8_t current_dir;                // Root = 127
    Filenames_node *filenames[128];     // Track the unique filenames in a directory at index i (max parents + root = 127)
    uint8_t buffer[1024];
} Disk;

void fs_mount(char *new_disk_name);
void fs_create(char name[5], int size);
void fs_delete(char name[5]);
void fs_read(char name[5], int block_num);
void fs_write(char name[5], int block_num);
void fs_buff(uint8_t buff[1024]);
void fs_ls(void);
void fs_resize(char name[5], int new_size);
void fs_defrag(void);
void fs_cd(char name[5]);

unsigned int str_to_uint(char *str, unsigned int length);
int strncmp_case(const char *s1, const char *s2, unsigned int n);
void trim_name(char name[5]);
