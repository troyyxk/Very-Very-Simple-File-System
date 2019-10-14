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

    printf("Super Block:\n")

    printf("    Address:\n");
    printf("    First Inode Bitmap: %d\n", sb->first_ib);
    printf("    First Data Bitmap: %d\n", sb->first_db);
    printf("    First Inode: %d\n", sb->first_inode);
    printf("    First Data Block: %d\n", sb->first_data);

    printf("\n");

    printf("    Amounts:\n");
    printf("    Nubmer of Inode Bitmap blocks: %d\n", sb->ib_count);
    printf("    Nubmer of Data Bitmap blocks: %d\n", sb->db_count);
    printf("    Number of inode Blocks: %d\n", sb->inode_count);
    printf("    Number of free inode block: %d\n", sb->free_inode_count);
    printf("    Number of data Blocks: %d\n", sb->data_count);
    printf("    Number of free data block: %d\n", sb->free_data_count);

    return 0;
}
