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

    printf("Super Block:\n");

    // in byte
    printf("Size: %d\n", (int)sb->size);

    printf("    Address:\n");
    printf("    First Inode Bitmap: %d\n", sb->first_ib);
    printf("    First Data Bitmap: %d\n", sb->first_db);
    printf("    First Inode: %d\n", sb->first_inode);
    printf("    First Data Block: %d\n", sb->first_data);

    printf("\n");

    printf("    Amounts:\n");
    printf("    Number of Inodes: %d\n", sb->inode_count);
    printf("    Number of Inode Bitmap blocks: %d\n", sb->ib_count);
    printf("    Number of Data Bitmap blocks: %d\n", sb->db_count);
    printf("    Number of inode Blocks: %d\n", sb->iblock_count);
    printf("    Number of free inode block: %d\n", sb->free_iblock_count);
    printf("    Number of data Blocks: %d\n", sb->dblock_count);
    printf("    Number of free data block: %d\n", sb->free_dblock_count);

    // Print Inode Bitmap
    printf("Inode bitmap: ");
    unsigned char *inode_bitmap = (unsigned char *)(disk + sb->first_ib * A1FS_BLOCK_SIZE);
    for (int bit = 0; bit < sb->inode_count; bit++)
    {
        printf("%d", (inode_bitmap[bit] & (1 << bit)) > 0);
        if ((bit + 1) % 5 == 0)
        {
            printf(" ");
        }
    }
    printf("\n");

    // Print Block Bitmap
    printf("Block bitmap: ");
    unsigned char *block_bitmap = (unsigned char *)(disk + sb->first_db * A1FS_BLOCK_SIZE);
    for (int bit = 0; bit < sb->dblock_count; bit++)
    {
        printf("%d", (block_bitmap[bit] & (1 << bit)) > 0);
        if ((bit + 1) % 5 == 0)
        {
            printf(" ");
        }
    }
    printf("\n");

    // Print Inode
    printf("Inode: ");
    void *inode_block = (void *)(disk + sb->first_inode * A1FS_BLOCK_SIZE);
    a1fs_inode *inode;
    for (int bit = 0; bit < sb->inode_count; bit++)
    {
        if ((inode_bitmap[bit] & (1 << bit)) > 0)
        { // bit map is 1
            inode = (void *)inode_block + bit * sizeof(a1fs_inode);
            // bitmap count starts form 0
            printf("Inode: Inode#: %d Number of Link: %ld\n Extend Block: %ld\n", bit, inode->links, inode->ext_block);
        }
    }
    printf("\n");

    printf("Directory Block:\n");
    for (int bit = 0; bit < sb->inode_count; bit++)
    {
        if ((inode_bitmap[bit] & (1 << bit)) > 0)
        { // bit map is 1
            inode = (void *)inode_block + bit * sizeof(a1fs_inode);
            if (inode->mode & S_IFDIR)
            { // this is a directory
                printf("Directory Extend Block Number: %ld (for Inode Number %d)\n", inode->ext_block, bit);
                a1fs_extent *first_extent = (void *)disk + (inode->ext_block * A1FS_BLOCK_SIZE);
                a1fs_extent *cur_extent;
                for (int i = 0; i < inode->ext_count; i++)
                {
                    cur_extent = (void *)first_extent + (i * sizeof(a1fs_extent));
                    printf("Inode number: %d, Name: %s", cur_extent->ino, cur_extent->name);
                }
            }
            // bitmap count starts form 0
        }
    }

    return 0;
}
