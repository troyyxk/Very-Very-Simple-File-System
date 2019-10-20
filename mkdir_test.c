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

/**
 * Return the number of entry names in a path
 *
 * Precondition: The path is correctly formatted
 *
 * @paran path   the path to be analyzed
 * @return       the number of entry names in the path, not including '/' (root)
 */
int num_entry_name(const char *path)
{
	// No entry names are there if length <= 1
	if (strlen(path) <= 1)
		return 0;

	// For paths reaching here, there are at least 1 entry names in here;
	// Each time a '/' is found, it is starting a new entry name provided
	// that the path is formatted correctly
	int count = 1;
	for (int i = 1; i < (int)strlen(path); i++)
	{
		if (path[i] == '/')
		{
			count++;
		}
	}

	return count;
}

// Set the i-th index of the bitmap to 1
void setBitOn(uint32_t *A, uint32_t i)
{
	// int int_bits = sizeof(uint32_t) * 8;
	A[i / 32] |= 1 << (i % 32);
}

// Set the i-th index of the bitmap to 0
void setBitOff(uint32_t *A, uint32_t i)
{
	// int int_bits = sizeof(uint32_t) * 8;
	A[i / 32] &= ~(1 << (i % 32));
}


/** Check in the bitmap if the bit is 0 (free) */
int checkBit(uint32_t *bitmap, uint32_t i)
{
	if((bitmap[i / 32] & 1 << (i % 32)) != 0){
		return 1;
	}
	return 0;
}

/** Find a free inode from the inode bitmap. */
int find_free_from_bitmap(a1fs_blk_t *bitmap, int size){
    printf("Start finding free inode with inode bitmap.\n");
	for (int i = 0; i < size; i++){
		if(checkBit((uint32_t *)bitmap, i) == 0){
			return i;
		}
	}

	// All inode full.
	return -1;
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
    void *image = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (image == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

    char *path = "/test";

    // a1fs_superblock *sb = (a1fs_superblock *)(image);

    char cpy_path[(int)strlen(path)+1];
	strcpy(cpy_path, path);
	char *delim = "/";
	char *curfix = strtok(cpy_path, delim);

	// clarify the confussion of treating the last one as none directory and return error
	int fix_count = num_entry_name(path);
	int cur_fix_index = 1;

	// loop through direcotries
	a1fs_superblock *sb = (void *)image;
	a1fs_inode *first_inode = (void *)image + sb->first_inode * A1FS_BLOCK_SIZE;
	a1fs_inode *cur = first_inode;

	a1fs_extent *extent;
	a1fs_dentry *dentry;
	a1fs_dentry *first_dentry;

	int cur_inode;

	printf("fix_count: %d\n", fix_count);
	while (curfix != NULL)
	{
		extent = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;
		first_dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;
		dentry = first_dentry;
		// cur = pioneer;
        printf("Enter the while loop with curfix: %s\n", curfix);
		printf("cur_fix_index: %d\n", cur_fix_index);


		// not a directory and not the last one.
		if (fix_count == cur_fix_index)
		{
			cur_inode = (int)dentry->ino;
			printf("cur_inode == dentry->ino == %d\n", cur_inode);
			break;
			/** At this point, cur is the inode of the parent directory and curfix is the name of the new directory to be added. */
		}
		cur_fix_index++;
		// printf("dentry->ino: %d", dentry->ino);
		// printf("Before check if the last prefix. fix_count == %d, cur_fix_index == %d.\n", fix_count, cur_fix_index);

		// cur->mode = S_IFDIR;
		if ((!(cur->mode & S_IFDIR)))
		{
            fprintf(stderr, "Not a directory and not the last one.\n");
			return 1;
		}
		// indicator for whether the directory is found, 1 for ont found and 0 for found
		int flag = 1;
        printf("cur->dentry_count: %d\n", cur->dentry_count);
		for (int i = 0; i < cur->dentry_count; i++)
		{
			// printf();
            printf("Enter the for loop with i == %d\n", i);
			dentry = (void *)first_dentry + i * sizeof(a1fs_dentry);
            printf("Dentry Inode : %d, Debtry Name: %s\n", dentry->ino, dentry->name);
			if (strcmp(dentry->name, curfix) == 0)
			{ // directory/file is found
				cur = (void *)first_inode + dentry->ino * sizeof(a1fs_inode);
				flag = 0;
				break;
			}
		}

		if (flag)
		{ // does not exist
            fprintf(stderr, "Does not exist.\n");
			return 1;
		}

		curfix = strtok(NULL, delim);
	}
	printf("Exit for loop.\n");
	

	if (curfix == NULL){
		fprintf(stderr, "curfis == null in mkdir, something wrong in the loop.\n");
		return 1;
	}

	/** At this point, cur is the inode of the parent directory and curfix is the name of the new directory to be added. */
	// Find position in bitmap and modify the bitmap.
	a1fs_blk_t *inode_bitmap = (void *)image + sb->first_ib*A1FS_BLOCK_SIZE;
	int new_inode_addr = find_free_from_bitmap(inode_bitmap, sb->inode_count);
    sb->free_inode_count -= 1;
    printf("Free inode location: %d (should be 1)\n", new_inode_addr);
	if (new_inode_addr < 0){
		fprintf(stderr, "All inode full (all inode bitmap 1)\n");
		return 1;
	}
	setBitOn(inode_bitmap, new_inode_addr);
    printf("Inode bitmap: ");
    // print_bitmap(inode_bitmap);

	a1fs_blk_t *data_bitmap = (void *)image + sb->first_db*A1FS_BLOCK_SIZE;
        printf("Block bitmap: ");
    // print_bitmap(data_bitmap);
	int new_ext_addr = find_free_from_bitmap(data_bitmap, sb->dblock_count);
    printf("Free extent block location: %d (should be 2)\n", new_ext_addr);
	if (new_ext_addr < 0){
		fprintf(stderr, "All data full (all data bitmap 1)\n");
		return 1;
	}
	setBitOn(data_bitmap, new_ext_addr);
        printf("Inode bitmap: ");
    // print_bitmap(inode_bitmap);
            printf("Block bitmap: ");
    // print_bitmap(data_bitmap);

	int new_data_attr = find_free_from_bitmap(data_bitmap, sb->dblock_count);
    printf("Free datablock location: %d (should be 3)\n", new_data_attr);
	if(new_data_attr<0){
		fprintf(stderr,"No more data block.\n");
		setBitOff(data_bitmap, new_ext_addr);
		return 1;
	}
	setBitOn(data_bitmap, new_data_attr);
        printf("Inode bitmap: ");
    // print_bitmap(inode_bitmap);
            printf("Block bitmap: ");
    // print_bitmap(data_bitmap);
    sb->free_dblock_count -=2;

	// Modify inode.
	cur->dentry_count += 1;

	// Add inode.
    void *inode_block = (void *)(image + sb->first_inode * A1FS_BLOCK_SIZE);
	a1fs_inode *new_inode = (void *)inode_block + new_inode_addr*sizeof(a1fs_inode);
	new_inode->links=2;
	new_inode->mode=S_IFDIR;
	// new_inode->mtime=NULL;
	// new_inode->size=NULL;
	new_inode->dentry_count=2;
	new_inode->ext_block=sb->first_data + new_ext_addr;
	new_inode->ext_count=1;
	
	printf("\n");
    printf("Final testing:\n");
    printf("Inode bitmap with the one in mkdir:\n");
    // print_bitmap(inode_bitmap);
    printf("\n");

	void *first_data = (void *)image + sb->first_data*A1FS_BLOCK_SIZE;
	// // Modify extent.
	a1fs_extent *cur_ext_block = (void *)image + cur->ext_block*A1FS_BLOCK_SIZE;
	// a1fs_extent *new_ext = (void *)cur_ext_block + (cur->ext_count - 1) * sizeof(a1fs_extent);
	// new_ext->start = new_inode->ext_block;
	// new_ext->count = 1;

	// Modify dentry of the parent directory.
	a1fs_dentry *first_parent_entry = (void *)image + cur_ext_block->start*A1FS_BLOCK_SIZE;
	a1fs_dentry *tareget_entry = (void *)first_parent_entry + sizeof(a1fs_dentry)*(cur->dentry_count - 1);
	tareget_entry->ino =  new_inode_addr;
	strcpy(tareget_entry->name, curfix);
	printf("tareget_entry->name: %s\n", tareget_entry->name);
	printf("tareget_entry->ino: %d\n", tareget_entry->ino);



	// Add extent block.
	a1fs_extent *extent_block = (void *)first_data + new_ext_addr*A1FS_BLOCK_SIZE;
	extent_block->start = new_data_attr + sb->first_data;
	extent_block->count = 1;


	// Add data block.
	a1fs_dentry *self_entry = (void *)first_data + new_data_attr*A1FS_BLOCK_SIZE;
	self_entry->ino = new_inode_addr;
	strcpy(self_entry->name, ".");

	a1fs_dentry *parent_entry = (void *)self_entry + 1*sizeof(a1fs_dentry);
	parent_entry->ino = cur_inode;
	strcpy(parent_entry->name, "..");

    // printf("\n");
    // printf("Final testing:\n");
    // printf("Inode bitmap with the one in mkdir:\n");
    // // print_bitmap(inode_bitmap);
    // printf("\n");
    

    // printf("Inode bitmap with readimage method:\n");    
    // a1fs_blk_t *in_bitmap = (void *)image + sb->first_ib*A1FS_BLOCK_SIZE;
    // for (int bit = 0; bit < sb->inode_count; bit++)
    // {
    //     printf("%d", (inode_bitmap[bit] & (1 << bit)) > 0);
    //     if ((bit + 1) % 5 == 0)
    //     {
    //         printf(" ");
    //     }
    // }
    // print_bitmap(in_bitmap);
    printf("\n");

    return 0;
}