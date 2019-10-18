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

    char *path = "/./test_dir";

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

	int cur_inode;

	while (curfix != NULL)
	{
		// cur = pioneer;

		// not a directory and not the last one.
		if (fix_count == cur_fix_index)
		{
			cur_inode = dentry->ino;
			break;
			/** At this point, cur is the inode of the parent directory and curfix is the name of the new directory to be added. */
		}
		cur_fix_index++;

		if ((!(cur->mode & S_IFDIR)))
		{
            fprintf(stderr, "Not a directory and not the last one.\n");
			return 1;
		}
		// indicator for whether the directory is found, 1 for ont found and 0 for found
		int flag = 1;
		extent = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;
		dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;
		for (int i = 0; i < cur->dentry_count; cur++)
		{
			dentry = (void *)dentry + i * sizeof(a1fs_dentry);
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

	if (curfix == NULL){
		fprintf(stderr, "curfis == null in mkdir, something wrong in the loop.\n");
		return 1;
	}

	/** At this point, cur is the inode of the parent directory and curfix is the name of the new directory to be added. */
	// Find position in bitmap and modify the bitmap.
	a1fs_blk_t *inode_bitmap = (void *)image + sb->first_ib*A1FS_BLOCK_SIZE;
	int new_inode_addr = find_free_from_bitmap(inode_bitmap, sb->inode_count);
	if (new_inode_addr < 0){
		fprintf(stderr, "All inode full (all inode bitmap 1)\n");
		return 1;
	}
	setBitOn(inode_bitmap, new_inode_addr);

	a1fs_blk_t *data_bitmap = (void *)image + sb->first_db*A1FS_BLOCK_SIZE;
	int new_ext_addr = find_free_from_bitmap(data_bitmap, sb->dblock_count);
	if (new_ext_addr < 0){
		fprintf(stderr, "All data full (all data bitmap 1)\n");
		return 1;
	}
	setBitOn(data_bitmap, new_ext_addr);

	int new_data_attr = find_free_from_bitmap(data_bitmap, sb->dblock_count);
	if(new_data_attr<0){
		fprintf(stderr,"No more data block.\n");
		setBitOff(data_bitmap, new_ext_addr);
		return 1;
	}
	setBitOn(data_bitmap, new_data_attr);

	// Add inode.
	a1fs_inode *new_inode = (void *)image + new_inode_addr*A1FS_BLOCK_SIZE;
	new_inode->links=2;
	new_inode->mode=S_IFDIR;
	// new_inode->mtime=NULL;
	// new_inode->size=NULL;
	new_inode->dentry_count=2;
	new_inode->ext_block=new_ext_addr;
	new_inode->ext_count=1;

	// Add extent block.
	a1fs_extent *extent_block = (void *)image + new_ext_addr*A1FS_BLOCK_SIZE;
	extent_block->start = new_data_attr;
	extent_block->count = 1;

	// Add data block.
	a1fs_dentry *self_entry = (void *)image + new_data_attr*A1FS_BLOCK_SIZE;
	self_entry->ino = new_inode_addr;
	strcpy(self_entry->name, ".");

	a1fs_dentry *parent_entry = (void *)self_entry + 1*sizeof(a1fs_dentry);
	parent_entry->ino = cur_inode;
	strcpy(parent_entry->name, "..");


    return 0;
}