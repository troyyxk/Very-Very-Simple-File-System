#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "a1fs.h"
#include "map.h"

// Pointer to the 0th byte of the image
unsigned char *image;

/** Check in the bitmap if the bit is 0 (free) */
int checkBit(uint32_t *bitmap, uint32_t i)
{
	if((bitmap[i / 32] & 1 << (i % 32)) != 0){
		return 1;
	}
	return 0;
}

/** Print Bitmap */
int print_bitmap(a1fs_blk_t *bitmap)
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

    // Map the image image into memory so that we don't have to do any reads and writes
    image = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (image == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

    a1fs_superblock *sb = (a1fs_superblock *)(image);

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
    printf("    Number of inode: %d\n", sb->inode_count);
    printf("    Number of free inode: %d\n", sb->free_inode_count);
    printf("    Number of data Blocks: %d\n", sb->dblock_count);
    printf("    Number of free data block: %d\n", sb->free_dblock_count);

    printf("\n");

    // Print Inode Bitmap
    printf("Inode bitmap: ");
	// a1fs_blk_t *inode_bitmap = (void *)image + sb->first _ib*A1FS_BLOCK_SIZE;
    
    a1fs_blk_t *inode_bitmap = (void *)image + sb->first_ib*A1FS_BLOCK_SIZE;
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
	// a1fs_blk_t *data_bitmap = (void *)image + sb->first_db*A1FS_BLOCK_SIZE;

    a1fs_blk_t *data_bitmap = (void *)image + sb->first_db*A1FS_BLOCK_SIZE;
    // for (int bit = 0; bit < sb->dblock_count; bit++)
    // {
    //     printf("%d", (data_bitmap[bit] & (1 << bit)) > 0);
    //     if ((bit + 1) % 5 == 0)
    //     {
    //         printf(" ");
    //     }
    // }
    print_bitmap(data_bitmap);
    printf("\n");

    printf("\n");

    // Print Inode
    printf("Inode: \n");
    void *inode_block = (void *)(image + sb->first_inode * A1FS_BLOCK_SIZE);
    a1fs_inode *inode;
    for (int bit = 0; bit < sb->inode_count; bit++)
    {
        if (checkBit(inode_bitmap, bit))
        { // bit map is 1
            inode = (void *)inode_block + bit * sizeof(a1fs_inode);
            // bitmap count starts form 0
            printf("Inode: Inode#: %d, Number of Link: %ld, Number of dentry: %d\n Extend Block: %ld ", bit, inode->links, inode->dentry_count, inode->ext_block);
            if (inode->mode & S_IFDIR){
                printf("Mode: Directory\n");
            }else if (inode->mode & S_IFREG)
            {
                printf("Mode: File\n");
            }else{
                printf("Mode: Unknown !!!!!!!!!\n");
            }
            
        }
    }
    printf("\n");

    printf("Directory Block:\n");
    for (int bit = 0; bit < sb->inode_count; bit++)
    {
        if (checkBit(inode_bitmap, bit))
        { // bit map is 1
            inode = (void *)inode_block + bit * sizeof(a1fs_inode);
            if (inode->mode & S_IFDIR)
            { // this is a directory
                printf("Directory Extend Block Number: %ld (for Inode Number %d)\n", inode->ext_block, bit);
                a1fs_extent *first_extent = (void *)image + (inode->ext_block * A1FS_BLOCK_SIZE);
                a1fs_extent *cur_extent;
                for (int i = 0; i < inode->ext_count; i++)
                {
                    cur_extent = (void *)first_extent + (i * sizeof(a1fs_extent));
                    printf("Extend Number: %d, Start: %d, Count: %d\n", i, cur_extent->start, cur_extent->count);
                    a1fs_dentry *first_entry = (void *)image + (A1FS_BLOCK_SIZE * cur_extent->start);
                    a1fs_dentry *cur_entry;
                    for (int j = 0; j < inode->dentry_count; j++)
                    {
                        cur_entry = (void *)first_entry + (j * sizeof(a1fs_dentry));
                        printf("    Inode number: %d, Name: %s\n", cur_entry->ino, cur_entry->name);
                    }
                }
            }
            // bitmap count starts form 0
        }
    }

    return 0;
}
