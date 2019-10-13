#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>

#include "a1fs.h"
#include "map.h"

// Pointer to the 0th byte of the disk
unsigned char *disk;

int main(int argc, char **argv)
{

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);
    if (fd == -1)
    {
        perror("open");
        exit(1);
    }

    // Map the disk image into memory so that we don't have to do any reads and writes
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

    a1fs_superblock *sb = (a1fs_superblock *)(disk);
    printf("Inodes: %d\n", sb->ib_count);
    printf("Blocks: %d\n", sb->db_count);

    // Add the caption for the block group:
    // printf("Block group:\n");

    // Actual printing of block numbers
    // struct ext2_group_desc *sb = (struct ext2_group_desc *)(disk + 1024 * 2);
    printf("    inode bitmap: %d\n", sb->first_ib);
    printf("    block bitmap: %d\n", sb->first_db);
    printf("    inode table: %d\n", sb->first_inode);
    printf("    data start: %d\n", sb->first_data);
    printf("    free inode block: %d\n", sb->free_inode_count);
    printf("    free data block: %d\n", sb->free_data_count);
    // printf("    used_dirs: %d\n", sb->bg_used_dirs_count);

    return 0;
}
