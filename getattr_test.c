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

    char *path = "/.Trash";

    // a1fs_superblock *sb = (a1fs_superblock *)(image);

	// IMPLEMENT
	char cpy_path[(int)strlen(path)+1];
	strcpy(cpy_path, path);
	char *delim = "/";
	char *curfix = strtok(cpy_path, delim);
    printf("Initial curfix: %s\n", curfix);

	// clarify the confussion of treating the last one as none directory and return error
	int fix_count = num_entry_name(path);
	int cur_fix_index = 1;

	// loop through direcotries
	a1fs_superblock *sb = (void *)image;
	a1fs_inode *first_inode = (void *)image + sb->first_inode * A1FS_BLOCK_SIZE;
	// a1fs_inode *pioneer = first_inode;
	a1fs_inode *cur = first_inode;

	a1fs_extent *extent;
	a1fs_dentry *dentry;

	// // more pioneer than pioneer
	// a1fs_inode *visioner;

	// if (curfix == null){  // no other prefix, the root directory
	// 	pioneer = null;
	// } else{
	// 	visioner =
	// }
	while (curfix != NULL)
	{
        printf("Enter the while loop with curfix: %s\n", curfix);
		// cur = pioneer;

		// not a directory and not the last one.
		if ((!(cur->mode & S_IFDIR)) && fix_count != cur_fix_index)
		{
            fprintf(stderr, "Not a directory and not the last one.\n");
			return 1;
		}
		cur_fix_index++;
		// indicator for whether the directory is found, 1 for ont found and 0 for found
		int flag = 1;
		extent = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;

		dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;
        printf("cur->dentry_count: %d\n", cur->dentry_count);
		for (int i = 0; i < cur->dentry_count; i++)
		{
            printf("Enter the for loop with i == %d\n", i);
			dentry = (void *)dentry + i * sizeof(a1fs_dentry);
            printf("Debtry Name: %s\n", dentry->name);
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

    printf("Inode: %d\n", dentry->ino);

    return 0;
}