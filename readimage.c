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

/** HELPER FUNCTION **/

int timespec2str(char *buf, uint len, struct timespec *ts) {
    int ret;
    struct tm t;

    tzset();
    if (localtime_r(&(ts->tv_sec), &t) == NULL)
        return 1;

    ret = strftime(buf, len, "%F %T", &t);
    if (ret == 0)
        return 2;
    len -= ret - 1;

    ret = snprintf(&buf[strlen(buf)], len, ".%09ld", ts->tv_nsec);
    if (ret >= len)
        return 3;

    return 0;
}

// Pointer to the 0th byte of the disk
unsigned char *disk;

/** Print Bitmap */
int print_bitmap(unsigned char *bitmap)
{
    for (int byte = 0; byte < 16; byte++)
    {
        // Print the bits within the current byte
        for (int bit = 0; bit < 8; bit++)
        {
            printf("%d", (bitmap[byte] & (1 << bit)) > 0);
        }
        // Print the space to separate bytes
        printf(" ");
    }
    // Line break
    printf("\n");
	return 0;
}

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

    printf("\n");

    // Print Inode Bitmap
    printf("Inode bitmap: ");
    unsigned char *inode_bitmap = (unsigned char *)(disk + sb->first_ib * A1FS_BLOCK_SIZE);
    // for (int bit = 0; bit < sb->inode_count; bit++)
    // {
    //     printf("%d", (inode_bitmap[bit] & (1 << bit)) > 0);
    //     if ((bit + 1) % 5 == 0)
    //     {
    //         printf(" ");
    //     }
    // }
    print_bitmap(inode_bitmap);
    printf("\n");

    // Print Block Bitmap
    printf("Block bitmap: ");
    unsigned char *block_bitmap = (unsigned char *)(disk + sb->first_db * A1FS_BLOCK_SIZE);
    // for (int bit = 0; bit < sb->dblock_count; bit++)
    // {
    //     printf("%d", (block_bitmap[bit] & (1 << bit)) > 0);
    //     if ((bit + 1) % 5 == 0)
    //     {
    //         printf(" ");
    //     }
    // }
    print_bitmap(block_bitmap);
    printf("\n");

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
                    printf("Extend Number: %d, Start: %d, Count: %d\n", i, cur_extent->start, cur_extent->count);
                    a1fs_dentry *first_entry = (void *)disk + (A1FS_BLOCK_SIZE * cur_extent->start);
                    a1fs_dentry *cur_entry;
                    for (int j = 0; j < inode->dentry_count; j++)
                    {
                        cur_entry = (void *)first_entry + (j * sizeof(a1fs_dentry));
                        printf("    Inode number: %d, Name: %s, ", cur_entry->ino, cur_entry->name);
			// Print mtime for the file
			a1fs_inode *entry_inode = (void *)inode_block + cur_entry->ino * sizeof(a1fs_inode);
			struct timespec mtime = entry_inode->mtime;
			printf("Mutation time: %lld.%.9ld\n", (long long) mtime.tv_sec, mtime.tv_nsec);
                    }
                }
            }
            // bitmap count starts form 0
        }
    }

    return 0;
}
