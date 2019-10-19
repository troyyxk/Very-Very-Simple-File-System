/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Alexey Khrabrov, Karen Reid
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019 Karen Reid
 */

/**
 * CSC369 Assignment 1 - a1fs driver implementation.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

// Using 2.9.x FUSE API
#define FUSE_USE_VERSION 29
#include <fuse.h>

#include "a1fs.h"
#include "fs_ctx.h"
#include "options.h"
#include "map.h"

//NOTE: All path arguments are absolute paths within the a1fs file system and
// start with a '/' that corresponds to the a1fs root directory.
//
// For example, if a1fs is mounted at "~/my_csc369_repo/a1b/mnt/", the path to a
// file at "~/my_csc369_repo/a1b/mnt/dir/file" (as seen by the OS) will be
// passed to FUSE callbacks as "/dir/file".
//
// Paths to directories (except for the root directory - "/") do not end in a
// trailing '/'. For example, "~/my_csc369_repo/a1b/mnt/dir/" will be passed to
// FUSE callbacks as "/dir".

/** HELPER FUNCTIONS **/

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

/**
 * Get a list of names of directory entries from a path, and fill them
 * into the address in the parameter
 *
 * Precondition: The path is correctly formatted
 *
 * @param path    the path to be truncated
 * @param dest    the destination to store the array of entry names
 */
// static void truncate_path(const char *path, char **dest)
// {

// 	int name_index = 0; // Dest index tracker
// 	int i = 1;			// path index tracker
// 	int j = 0;			// Name index tracker

// 	while (i < strlen(path))
// 	{
// 		if (path[i] != '/')
// 		{
// 			// Reading chars of a name
// 			dest[name_index][j] = path[i];
// 			j++;
// 		}
// 		else
// 		{
// 			// End of a name; terminate current name and prepare for
// 			// reading next name
// 			dest[name_index][j] = '\0';
// 			name_index++;
// 			j = 0;
// 		}
// 		// Check the next index
// 		i++;
// 	}
// }

/**
 * Initialize the file system.
 *
 * Called when the file system is mounted. NOTE: we are not using the FUSE
 * init() callback since it doesn't support returning errors. This function must
 * be called explicitly before fuse_main().
 *
 * @param fs    file system context to initialize.
 * @param opts  command line options.
 * @return      true on success; false on failure.
 */
static bool a1fs_init(fs_ctx *fs, a1fs_opts *opts)
{
	// Nothing to initialize if only printing help or version
	if (opts->help || opts->version)
		return true;

	size_t size;
	void *image = map_file(opts->img_path, A1FS_BLOCK_SIZE, &size);
	if (!image)
		return false;

	return fs_ctx_init(fs, image, size, opts);
}

/**
 * Cleanup the file system.
 *
 * Called when the file system is unmounted. Must cleanup all the resources
 * created in a1fs_init().
 */
static void a1fs_destroy(void *ctx)
{
	fs_ctx *fs = (fs_ctx *)ctx;
	if (fs->image)
	{
		if (fs->opts->sync && (msync(fs->image, fs->size, MS_SYNC) < 0))
		{
			perror("msync");
		}
		munmap(fs->image, fs->size);
		fs_ctx_destroy(fs);
	}
}

/** Get file system context. */
static fs_ctx *get_fs(void)
{
	return (fs_ctx *)fuse_get_context()->private_data;
}

/**
 * Get file system statistics.
 *
 * Implements the statvfs() system call. See "man 2 statvfs" for details.
 * The f_bfree and f_bavail fields should be set to the same value.
 * The f_ffree and f_favail fields should be set to the same value.
 * The f_fsid and f_flag fields are ignored.
 *
 * @param path  path to any file in the file system. Can be ignored.
 * @param st    pointer to the struct statvfs that receives the result.
 * @return      0 on success; -errno on error.
 */
static int a1fs_statfs(const char *path, struct statvfs *st)
{
	(void)path; // unused
	fs_ctx *fs = get_fs();

	memset(st, 0, sizeof(*st));
	st->f_bsize = A1FS_BLOCK_SIZE;
	st->f_frsize = A1FS_BLOCK_SIZE;
	//TODO
	(void)fs;
	st->f_namemax = A1FS_NAME_MAX;

	return -ENOSYS;
}

/**
 * Get file or directory attributes.
 *
 * Implements the stat() system call. See "man 2 stat" for details.
 * The st_dev, st_blksize, and st_ino fields are ignored.
 *
 * NOTE: the st_blocks field is measured in 512-byte units (disk sectors).
 *
 * Errors:
 *   ENAMETOOLONG  the path or one of its components is too long.
 *   ENOENT        a component of the path does not exist.
 *   ENOTDIR       a component of the path prefix is not a directory.
 *
 * From Piazza:
 * https://piazza.com/class/k001adza4dz2ja?cid=151
 * You can just set values for the fields that you are using (mode, links, size, time)
 * 
 * 
 *
 * @param path  path to a file or directory.
 * @param st    pointer to the struct stat that receives the result.
 * @return      0 on success; -errno on error;
 */
static int a1fs_getattr(const char *path, struct stat *st)
{
	// unsigned short st_mode;
	// short st_nlink;
	// _off_t st_size;
	// // time_t st_atime;
	// time_t st_mtime;
	// // time_t st_ctime;

	if (strlen(path) >= A1FS_PATH_MAX)
		return -ENAMETOOLONG;
	fs_ctx *fs = get_fs();

	memset(st, 0, sizeof(*st));

	// //NOTE: This is just a placeholder that allows the file system to be mounted
	// // without errors. You should remove this from your implementation.
	// if (strcmp(path, "/") == 0)
	// {
	// 	st->st_mode = S_IFDIR | 0777;
	// 	return 0;
	// }

	// IMPLEMENT
	(void)fs;
	char cpy_path[(int)strlen(path)+1];
	strcpy(cpy_path, path);
	char *delim = "/";
	char *curfix = strtok(cpy_path, delim);

	// clarify the confussion of treating the last one as none directory and return error
	int fix_count = num_entry_name(path);
	int cur_fix_index = 1;

	// loop through direcotries
	void *image = fs->image;
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
		// cur = pioneer;

		// not a directory and not the last one.
		if ((!(cur->mode & S_IFDIR)) && fix_count != cur_fix_index)
		{
			return -ENOTDIR;
		}
		cur_fix_index++;
		// indicator for whether the directory is found, 1 for ont found and 0 for found
		int flag = 1;
		extent = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;

		dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;
		for (int i = 0; i < cur->dentry_count; i++)
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
			return -ENOENT;
		}

		curfix = strtok(NULL, delim);
	}

	st->st_mode = cur->mode;
	st->st_nlink = cur->links;
	// st->st_size = cur->size;
	st->st_mtime = cur->mtime.tv_sec;

	return 0;
}

/**
 * Read a directory.
 *
 * Implements the readdir() system call. Should call filler() for each directory
 * entry. See fuse.h in libfuse source code for details.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a directory.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a filler() call failed).
 *
 * @param path    path to the directory.
 * @param buf     buffer that receives the result.
 * @param filler  function that needs to be called for each directory entry.
 *                Pass 0 as offset (4th argument). 3rd argument can be NULL.
 * @param offset  unused.
 * @param fi      unused.
 * @return        0 on success; -errno on error.
 */
static int a1fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
						off_t offset, struct fuse_file_info *fi)
{
	(void)offset; // unused
	(void)fi;	 // unused
	fs_ctx *fs = get_fs();

	//NOTE: This is just a placeholder that allows the file system to be mounted
	// without errors. You should remove this from your implementation.
//	if (strcmp(path, "/") == 0)
//	{
//		filler(buf, ".", NULL, 0);
//		filler(buf, "..", NULL, 0);
//		return 0;
//	}

	// IMPLEMENTATION
	(void)fs;
    char cpy_path[(int)strlen(path)+1];
    strcpy(cpy_path, path);
    char *delim = "/";
    char *curfix = strtok(cpy_path, delim);

    // clarify the confussion of treating the last one as none directory and return error
    // int fix_count = num_entry_name(path);
    int cur_fix_index = 1;

    // Loop through the tokens on the path to find the location we are interested in
    void *image = fs->image;
    a1fs_superblock *sb = (void *)image;
    a1fs_inode *first_inode = (void *)image + sb->first_inode * A1FS_BLOCK_SIZE;
    a1fs_inode *cur = first_inode;

	a1fs_extent *extent;
    a1fs_dentry *dentry;

    while (curfix != NULL) {
        // not a directory
        if (!(cur->mode & S_IFDIR))
        {
            return -ENOTDIR;
        }
        cur_fix_index++;

		extent = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;
		dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;

        for (int i = 0; i < cur->dentry_count; cur++)
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
            if (filler(buf, entries[i].name, NULL, 0) != 0) {
                return -ENOMEM;
            };
        }
    }


    // Success
    return 0;

//	return -ENOSYS;
}

/**
 * Create a directory.
 *
 * Implements the mkdir() system call.
 *
 * NOTE: the mode argument may not have the type specification bits set, i.e.
 * S_ISDIR(mode) can be false. To obtain the correct directory type bits use
 * "mode | S_IFDIR".
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" doesn't exist.
 *   The parent directory of "path" exists and is a directory.
 *   "path" and its components are not too long.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the directory to create.
 * @param mode  file mode bits.
 * @return      0 on success; -errno on error.
 */
static int a1fs_mkdir(const char *path, mode_t mode)
{
	fs_ctx *fs = get_fs();

	//TODO
	(void)path;
	(void)mode;
	(void)fs;

	// IMPLEMENT
	(void)fs;
    char cpy_path[(int)strlen(path)+1];
	strcpy(cpy_path, path);
	char *delim = "/";
	char *curfix = strtok(cpy_path, delim);

	// clarify the confussion of treating the last one as none directory and return error
	int fix_count = num_entry_name(path);
	int cur_fix_index = 1;

	// loop through direcotries
	void *image = fs->image;
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
		cur->mode = S_IFDIR;

		if ((!(cur->mode & S_IFDIR)))
		{
            fprintf(stderr, "Not a directory and not the last one.\n");
			return -ENOTDIR;
		}
		// indicator for whether the directory is found, 1 for ont found and 0 for found
		int flag = 1;
		extent = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;
		dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;

		for (int i = 0; i < cur->dentry_count; i++)
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
			return -ENOENT;
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
    sb->free_inode_count -= 1;
    printf("Free inode location: %d (should be 1)\n", new_inode_addr);
	if (new_inode_addr < 0){
		fprintf(stderr, "All inode full (all inode bitmap 1)\n");
		return 1;
	}
	setBitOn(inode_bitmap, new_inode_addr);
    printf("Inode bitmap: ");
    print_bitmap(inode_bitmap);

	a1fs_blk_t *data_bitmap = (void *)image + sb->first_db*A1FS_BLOCK_SIZE;
        printf("Block bitmap: ");
    print_bitmap(data_bitmap);
	int new_ext_addr = find_free_from_bitmap(data_bitmap, sb->dblock_count);
    printf("Free extent block location: %d (should be 2)\n", new_ext_addr);
	if (new_ext_addr < 0){
		fprintf(stderr, "All data full (all data bitmap 1)\n");
		return 1;
	}
	setBitOn(data_bitmap, new_ext_addr);
        printf("Inode bitmap: ");
    print_bitmap(inode_bitmap);
            printf("Block bitmap: ");
    print_bitmap(data_bitmap);

	int new_data_attr = find_free_from_bitmap(data_bitmap, sb->dblock_count);
    printf("Free datablock location: %d (should be 3)\n", new_data_attr);
	if(new_data_attr<0){
		fprintf(stderr,"No more data block.\n");
		setBitOff(data_bitmap, new_ext_addr);
		return 1;
	}
	setBitOn(data_bitmap, new_data_attr);
        printf("Inode bitmap: ");
    print_bitmap(inode_bitmap);
            printf("Block bitmap: ");
    print_bitmap(data_bitmap);
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
    print_bitmap(inode_bitmap);
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
	tareget_entry->ino =  new_inode_addr + sb->first_data;
	strcpy(tareget_entry->name, curfix);

	// Add extent block.
	a1fs_extent *extent_block = (void *)first_data + new_ext_addr*A1FS_BLOCK_SIZE;
	extent_block->start = new_data_attr + sb->first_data;
	extent_block->count = 1;


	// Add data block.
	a1fs_dentry *self_entry = (void *)first_data + new_data_attr*A1FS_BLOCK_SIZE;
	self_entry->ino = cur_inode;
	strcpy(self_entry->name, ".");

	a1fs_dentry *parent_entry = (void *)self_entry + 1*sizeof(a1fs_dentry);
	parent_entry->ino = new_inode_addr;
	strcpy(parent_entry->name, "..");

    // printf("\n");
    // printf("Final testing:\n");
    // printf("Inode bitmap with the one in mkdir:\n");
    // print_bitmap(inode_bitmap);
    // printf("\n");
    

    printf("Inode bitmap with readimage method:\n");    
    a1fs_blk_t *in_bitmap = (void *)image + sb->first_ib*A1FS_BLOCK_SIZE;
    // for (int bit = 0; bit < sb->inode_count; bit++)
    // {
    //     printf("%d", (inode_bitmap[bit] & (1 << bit)) > 0);
    //     if ((bit + 1) % 5 == 0)
    //     {
    //         printf(" ");
    //     }
    // }
    print_bitmap(in_bitmap);
    printf("\n");

    return 0;
}

/**
 * Remove a directory.
 *
 * Implements the rmdir() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a directory.
 *
 * Errors:
 *   ENOTEMPTY  the directory is not empty.
 *
 * @param path  path to the directory to remove.
 * @return      0 on success; -errno on error.
 */
static int a1fs_rmdir(const char *path)
{
	fs_ctx *fs = get_fs();

	//TODO
	(void)path;
	(void)fs;
	return -ENOSYS;
}

/**
 * Create a file.
 *
 * Implements the open()/creat() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" doesn't exist.
 *   The parent directory of "path" exists and is a directory.
 *   "path" and its components are not too long.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the file to create.
 * @param mode  file mode bits.
 * @param fi    unused.
 * @return      0 on success; -errno on error.
 */
static int a1fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	(void)fi; // unused
	assert(S_ISREG(mode));
	fs_ctx *fs = get_fs();

	//TODO
	(void)path;
	(void)mode;
	(void)fs;
	return -ENOSYS;
}

/**
 * Remove a file.
 *
 * Implements the unlink() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * @param path  path to the file to remove.
 * @return      0 on success; -errno on error.
 */
static int a1fs_unlink(const char *path)
{
	fs_ctx *fs = get_fs();

	//TODO
	(void)path;
	(void)fs;
	return -ENOSYS;
}

/**
 * Rename a file or directory.
 *
 * Implements the rename() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "from" exists.
 *   The parent directory of "to" exists and is a directory.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param from  original file path.
 * @param to    new file path.
 * @return      0 on success; -errno on error.
 */
static int a1fs_rename(const char *from, const char *to)
{
	fs_ctx *fs = get_fs();

	//TODO
	(void)from;
	(void)to;
	(void)fs;
	return -ENOSYS;
}

/**
 * Change the access and modification times of a file or directory.
 *
 * Implements the utimensat() system call. See "man 2 utimensat" for details.
 *
 * NOTE: You only have to implement the setting of modification time (mtime).
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists.
 *
 * @param path  path to the file or directory.
 * @param tv    timestamps array. See "man 2 utimensat" for details.
 * @return      0 on success; -errno on failure.
 */
static int a1fs_utimens(const char *path, const struct timespec tv[2])
{
	fs_ctx *fs = get_fs();

	//TODO
	(void)path;
	(void)tv;
	(void)fs;
	return -ENOSYS;
}

/**
 * Change the size of a file.
 *
 * Implements the truncate() system call. Supports both extending and shrinking.
 * If the file is extended, future reads from the new uninitialized range must
 * return zero data.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the file to set the size.
 * @param size  new file size in bytes.
 * @return      0 on success; -errno on error.
 */
static int a1fs_truncate(const char *path, off_t size)
{
	fs_ctx *fs = get_fs();

	//TODO
	(void)path;
	(void)size;
	(void)fs;
	return -ENOSYS;
}

/**
 * Read data from a file.
 *
 * Implements the pread() system call. Should return exactly the number of bytes
 * requested except on EOF (end of file) or error, otherwise the rest of the
 * data will be substituted with zeros. Reads from file ranges that have not
 * been written to must return zero data.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * @param path    path to the file to read from.
 * @param buf     pointer to the buffer that receives the data.
 * @param size    buffer size - number of bytes requested.
 * @param offset  offset from the beginning of the file to read from.
 * @param fi      unused.
 * @return        number of bytes read on success; 0 if offset is beyond EOF;
 *                -errno on error.
 */
static int a1fs_read(const char *path, char *buf, size_t size, off_t offset,
					 struct fuse_file_info *fi)
{
	(void)fi; // unused
	fs_ctx *fs = get_fs();

	//TODO
	(void)path;
	(void)buf;
	(void)size;
	(void)offset;
	(void)fs;
	return -ENOSYS;
}

/**
 * Write data to a file.
 *
 * Implements the pwrite() system call. Should return exactly the number of
 * bytes requested except on error. If the offset is beyond EOF (end of file),
 * the file must be extended. If the write creates a "hole" of uninitialized
 * data, future reads from the "hole" must return zero data.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * @param path    path to the file to write to.
 * @param buf     pointer to the buffer containing the data.
 * @param size    buffer size - number of bytes requested.
 * @param offset  offset from the beginning of the file to write to.
 * @param fi      unused.
 * @return        number of bytes written on success; -errno on error.
 */
static int a1fs_write(const char *path, const char *buf, size_t size,
					  off_t offset, struct fuse_file_info *fi)
{
	(void)fi; // unused
	fs_ctx *fs = get_fs();

	//TODO
	(void)path;
	(void)buf;
	(void)size;
	(void)offset;
	(void)fs;
	return -ENOSYS;
}

static struct fuse_operations a1fs_ops = {
	.destroy = a1fs_destroy,
	.statfs = a1fs_statfs,
	.getattr = a1fs_getattr,
	.readdir = a1fs_readdir,
	.mkdir = a1fs_mkdir,
	.rmdir = a1fs_rmdir,
	.create = a1fs_create,
	.unlink = a1fs_unlink,
	.rename = a1fs_rename,
	.utimens = a1fs_utimens,
	.truncate = a1fs_truncate,
	.read = a1fs_read,
	.write = a1fs_write,
};

int main(int argc, char *argv[])
{
	a1fs_opts opts = {0}; // defaults are all 0
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	if (!a1fs_opt_parse(&args, &opts))
		return 1;

	fs_ctx fs = {0};
	if (!a1fs_init(&fs, &opts))
	{
		fprintf(stderr, "Failed to mount the file system\n");
		return 1;
	}

	return fuse_main(args.argc, args.argv, &a1fs_ops, &fs);
}
