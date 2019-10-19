//
// Created by Chenhao Gong on 2019-10-18.
//

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


int main(int argc, char **argv) {
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

    // Test using the current path
    char *path = ".";

    // IMPLEMENTATION
    char cpy_path[(int)strlen(path)+1];
    strcpy(cpy_path, path);
    char *delim = "/";
    char *curfix = strtok(cpy_path, delim);

    // clarify the confussion of treating the last one as none directory and return error
    // int fix_count = num_entry_name(path);
    int cur_fix_index = 1;

    // Loop through the tokens on the path to find the location we are interested in
    a1fs_superblock *sb = (void *)image;
    a1fs_inode *first_inode = (void *)image + sb->first_inode * A1FS_BLOCK_SIZE;
    a1fs_inode *cur = first_inode;

    a1fs_extent *extent;
    a1fs_dentry *dentry;

    while (curfix != NULL) {
        // not a directory
        if (!(cur->mode & S_IFDIR))
        {
            return -1;
        }
        cur_fix_index++;

        // Find the total number of extents in the directory
//        int extent_count = cur->ext_count;
        // For tracking the extents in the directory
//        int cur_ext_index = 0;
        extent = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;
        dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;

        for (int i = 0; i < cur->dentry_count; i++)
        {
            dentry = (void *)dentry + i * sizeof(a1fs_dentry);
            if (strcmp(dentry->name, curfix) == 0)
            { // directory/file is found
                cur = (void *)first_inode + dentry->ino * sizeof(a1fs_inode);
                break;
            }
        }

        // Now cur should be pointing to the directory we are reading
        a1fs_dentry *entries = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;
        for (int i = 0; i < cur->dentry_count; i++) {
//            filler(buf, entries[i].name, NULL, 0);
            printf("Current entry:\n");
	        printf("name: %s\n", entries[i].name);
            printf("curfix: %s\n", curfix);
            printf("cur_i: %d/%d\n\n", i, cur->dentry_count);
        }

        curfix = strtok(NULL, delim);
    }


    // Success
    return 0;

//	return -ENOSYS;
}
