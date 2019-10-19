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

/** Precondition: p_path is longer than full path. 
 * return 1 if loop all the way through, which is an error
*/
int get_parent_path(const char *full_path, char *p_path)
{
	int total_entry_count = num_entry_name(full_path);
	if (total_entry_count == 1)
	{
		p_path[0] = '/';
		p_path[1] = '\0';
		return 0;
	}
	printf("full_path num_entry_name: %d\n", total_entry_count);

	int count = 0;
	for (int i = 0; i < (int)strlen(full_path); i++)
	{

		printf("Count: %d\n", count);
		if (full_path[i] == '/')
		{
			count++;
		}
		if (count == total_entry_count)
		{
			p_path[i] = '\0';
			return 0;
		}
		p_path[i] = full_path[i];
	}
	return 1;
}

int get_last_entry(const char *path, char *last_entry)
{
	printf("Start get_last_entry with path: %s\n.", path);
	int total_entry_count = num_entry_name(path);
	printf("path num_entry_name: %d\n", total_entry_count);

	int count = 0;
	int flag = 0;
	int j = 0;
	for (int i = 0; i < (int)strlen(path); i++)
	{

		printf("Count: %d\n", count);
		if (path[i] == '/')
		{
			count++;
		}
		// jump through the first
		if (flag)
		{ // the last one has been reached
			last_entry[j] = path[i];
			last_entry[j + 1] = '\0';
			j++;
		}
		if (count == total_entry_count)
		{
			flag = 1;
		}
	}
	printf("End get_last_entry with last_entry: %s\n.", last_entry);
	return 0;
}

int forward_layback_extents(a1fs_inode *cur, a1fs_dentry *first_entry, a1fs_dentry *target_entry, int target_entry_index)
{
	a1fs_dentry *modify_entry = target_entry;
	a1fs_dentry *remain_entry;
	printf("target_entry_index: %d, cur->dentry_count: %d\n", target_entry_index, cur->dentry_count);
	for (int i = target_entry_index + 1; i < cur->dentry_count; i++)
	{
		printf("for loop: %d\n", i);
		remain_entry = (void *)first_entry + i * sizeof(a1fs_dentry);
		printf("remain_entry->ino: %d\n", remain_entry->ino);
		printf("remain_entry->name: %s\n", remain_entry->name);
		modify_entry->ino = remain_entry->ino;
		strcpy(modify_entry->name, remain_entry->name);
		modify_entry = remain_entry;
		printf("remain_entry->ino: %d\n", modify_entry->ino);
		printf("remain_entry->name: %s\n", modify_entry->name);
	}

	return 0;
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
	if ((bitmap[i / 32] & 1 << (i % 32)) != 0)
	{
		return 1;
	}
	return 0;
}

/** Find a free inode from the inode bitmap. */
int find_free_from_bitmap(a1fs_blk_t *bitmap, int size)
{
	for (int i = 0; i < size; i++)
	{
		if (checkBit((uint32_t *)bitmap, i) == 0)
		{
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

int get_ext_index_from_inode(a1fs_inode *parent_inode, char *target_entry)
{
	printf("Start get_ext_index_from_inode with target_entry: %s\n", target_entry);
	fs_ctx *fs = get_fs();
	void *image = fs->image;

	a1fs_extent *ext = (void *)image + parent_inode->ext_block * A1FS_BLOCK_SIZE;
	a1fs_dentry *first_entry = (void *)image + ext->start * A1FS_BLOCK_SIZE;
	a1fs_dentry *cur_entry;
	printf("Enter for loop with upper bound parent_inode->dentry_count: %d\n", parent_inode->dentry_count);
	for (int i = 0; i < parent_inode->dentry_count; i++)
	{
		cur_entry = (void *)first_entry + i * sizeof(a1fs_dentry);
		if (strcmp(cur_entry->name, target_entry) == 0)
		{
			printf("Exit get_ext_index_from_inode with target_entry find: %s\n", target_entry);
			return i;
		}
	}
	printf("Exit for loop without finding it.\n");

	return -1;
}

/** This function will try to find inode based on path*/
int find_inode_from_path(const char *path)
{
	printf("Start find_inode_from_path with path %s.\n", path);
	fs_ctx *fs = get_fs();

	// IMPLEMENTED
	(void)path;
	// IMPLEMENT
	(void)fs;
	char cpy_path[(int)strlen(path) + 1];
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

	a1fs_extent *extent = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;
	;
	a1fs_dentry *first_dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;
	;
	a1fs_dentry *dentry = first_dentry;

	printf("Start while loop.\n");
	while (curfix != NULL)
	{
		// cur = pioneer;
		// not a directory and not the last one.
		if ((!(cur->mode & S_IFDIR)) && fix_count != cur_fix_index)
		{
			printf("Not a directory and not the last one.\n");
			return -ENOTDIR;
		}

		// indicator for whether the directory is found, 1 for ont found and 0 for found
		int flag = 1;
		extent = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;

		first_dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;
		printf("Enter for loop.\n");
		for (int i = 0; i < cur->dentry_count; i++)
		{
			printf("Enter the for loop with i == %d\n", i);
			dentry = (void *)first_dentry + i * sizeof(a1fs_dentry);
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
			return -1;
		}

		printf("fix_count: %d, cur_fix_index: %d\n", fix_count, cur_fix_index);
		if (fix_count == cur_fix_index)
		{
			// the last one is reached
			break;
		}
		cur_fix_index++;

		printf("Exit for loop.\n");

		curfix = strtok(NULL, delim);
	}

	printf("dentry->ino: %d\n", dentry->ino);
	return dentry->ino;
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
	a1fs_superblock *sb = (void *)fs->image;
	st->f_blocks = sb->size / A1FS_BLOCK_SIZE;
	st->f_bfree = sb->free_dblock_count;
	st->f_bavail = st->f_bfree;

	st->f_files = sb->inode_count;
	st->f_ffree = sb->free_inode_count;
	st->f_favail = st->f_ffree;

	st->f_namemax = A1FS_NAME_MAX;

	return 0;
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

	printf("Start a1fs_getattr with path %s.\n", path);

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
	char cpy_path[(int)strlen(path) + 1];
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
	a1fs_dentry *first_dentry;
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

		first_dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;
		for (int i = 0; i < cur->dentry_count; i++)
		{
			printf("Enter the for loop with i == %d\n", i);
			dentry = (void *)first_dentry + i * sizeof(a1fs_dentry);
			printf("Debtry Name: %s\n", dentry->name);
			if (strcmp(dentry->name, curfix) == 0)
			{ // directory/file is found
				cur = (void *)first_inode + dentry->ino * sizeof(a1fs_inode);
				flag = 0;
				break;
			}
		}

		printf("Exit for loop.\n");

		if (flag)
		{ // does not exist
			fprintf(stderr, "Does not exist.\n");
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
	printf("Start a1fs_readdir.\n");
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

	// char cpy_path[(int)strlen(path) + 1];
	// strcpy(cpy_path, path);
	// char *delim = "/";
	// char *curfix = strtok(cpy_path, delim);

	// Loop through the tokens on the path to find the location we are interested in
	void *image = fs->image;
	a1fs_superblock *sb = (void *)image;
	a1fs_inode *first_inode = (void *)image + sb->first_inode * A1FS_BLOCK_SIZE;
	a1fs_inode *cur = first_inode;

	int cur_ino = find_inode_from_path(path);
	if (cur_ino < 0)
	{
		fprintf(stderr, "Inode for the path not exist.\n");
		return 1;
	}
	cur = (void *)first_inode + sizeof(a1fs_inode);
	a1fs_extent *cur_extent = (void *)image + first_inode->ext_block * A1FS_BLOCK_SIZE;
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	a1fs_dentry *first_entry = (void *)image + cur_extent->start * A1FS_BLOCK_SIZE;
	a1fs_dentry *cur_entry;
	for (int i = 0; i < cur->dentry_count; i++)
	{
		cur_entry = (void *)first_entry + i * sizeof(a1fs_dentry);
		if (cur_entry->ino != 0)
		{
			filler(buf, cur_entry->name, NULL, 0);
		}
	}

	return 0;
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
	printf("Start a1fs_mkdir with path %s.\n", path);
	fs_ctx *fs = get_fs();

	//TODO
	(void)path;
	(void)mode;
	(void)fs;

	// IMPLEMENT
	(void)fs;
	char cpy_path[(int)strlen(path) + 1];
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
	a1fs_dentry *first_dentry;
	// int first_while =

	int cur_inode;

	while (curfix != NULL)
	{
		// cur = pioneer;
		printf("Enter the while loop with curfix: %s\n", curfix);

		// cur->mode = S_IFDIR;

		if ((!(cur->mode & S_IFDIR)))
		{
			fprintf(stderr, "Not a directory and not the last one.\n");
			return -ENOTDIR;
		}
		printf("cur->dentry_count: %d\n", cur->dentry_count);
		// indicator for whether the directory is found, 1 for ont found and 0 for found
		int flag = 1;
		extent = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;
		first_dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;
		dentry = first_dentry;

		// not a directory and not the last one.
		if (fix_count == cur_fix_index)
		{
			cur_inode = dentry->ino;
			break;
			/** At this point, cur is the inode of the parent directory and curfix is the name of the new directory to be added. */
		}
		cur_fix_index++;
		// printf("dentry->ino: %d", dentry->ino);
		// printf("Before check if the last prefix. fix_count == %d, cur_fix_index == %d.\n", fix_count, cur_fix_index);

		for (int i = 0; i < cur->dentry_count; i++)
		{
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
		printf("Exit for loop.\n");

		if (flag)
		{ // does not exist
			fprintf(stderr, "Does not exist.\n");
			return -ENOENT;
		}

		curfix = strtok(NULL, delim);
	}

	if (curfix == NULL)
	{
		fprintf(stderr, "curfis == null in mkdir, something wrong in the loop.\n");
		return 1;
	}

	/** At this point, cur is the inode of the parent directory and curfix is the name of the new directory to be added. */
	// Find position in bitmap and modify the bitmap.
	a1fs_blk_t *inode_bitmap = (void *)image + sb->first_ib * A1FS_BLOCK_SIZE;
	int new_inode_addr = find_free_from_bitmap(inode_bitmap, sb->inode_count);
	sb->free_inode_count -= 1;
	printf("Free inode location: %d (should be 1)\n", new_inode_addr);
	if (new_inode_addr < 0)
	{
		fprintf(stderr, "All inode full (all inode bitmap 1)\n");
		return 1;
	}
	setBitOn(inode_bitmap, new_inode_addr);

	a1fs_blk_t *data_bitmap = (void *)image + sb->first_db * A1FS_BLOCK_SIZE;
	int new_ext_addr = find_free_from_bitmap(data_bitmap, sb->dblock_count);
	printf("Free extent block location: %d (should be 2)\n", new_ext_addr);
	if (new_ext_addr < 0)
	{
		fprintf(stderr, "All data full (all data bitmap 1)\n");
		return 1;
	}
	setBitOn(data_bitmap, new_ext_addr);

	int new_data_attr = find_free_from_bitmap(data_bitmap, sb->dblock_count);
	printf("Free datablock location: %d (should be 3)\n", new_data_attr);
	if (new_data_attr < 0)
	{
		fprintf(stderr, "No more data block.\n");
		setBitOff(data_bitmap, new_ext_addr);
		return 1;
	}
	setBitOn(data_bitmap, new_data_attr);
	sb->free_dblock_count -= 2;

	// Modify inode.
	cur->dentry_count += 1;

	// Add inode.
	void *inode_block = (void *)(image + sb->first_inode * A1FS_BLOCK_SIZE);
	a1fs_inode *new_inode = (void *)inode_block + new_inode_addr * sizeof(a1fs_inode);
	new_inode->links = 2;
	new_inode->mode = S_IFDIR | mode;
	// new_inode->mode=mode;

	clock_gettime(CLOCK_REALTIME, &(new_inode->mtime));
	// new_inode->size=NULL;
	new_inode->dentry_count = 2;
	new_inode->ext_block = sb->first_data + new_ext_addr;
	new_inode->ext_count = 1;

	printf("\n");

	void *first_data = (void *)image + sb->first_data * A1FS_BLOCK_SIZE;
	// // Modify extent.
	a1fs_extent *cur_ext_block = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;
	// a1fs_extent *new_ext = (void *)cur_ext_block + (cur->ext_count - 1) * sizeof(a1fs_extent);
	// new_ext->start = new_inode->ext_block;
	// new_ext->count = 1;

	// Modify dentry of the parent directory.
	a1fs_dentry *first_parent_entry = (void *)image + cur_ext_block->start * A1FS_BLOCK_SIZE;
	a1fs_dentry *tareget_entry = (void *)first_parent_entry + sizeof(a1fs_dentry) * (cur->dentry_count - 1);
	tareget_entry->ino = new_inode_addr;
	strcpy(tareget_entry->name, curfix);
	printf("tareget_entry->name: %s\n", tareget_entry->name);

	// Add extent block.
	a1fs_extent *extent_block = (void *)first_data + new_ext_addr * A1FS_BLOCK_SIZE;
	extent_block->start = new_data_attr + sb->first_data;
	extent_block->count = 1;

	// Add data block.
	a1fs_dentry *self_entry = (void *)first_data + new_data_attr * A1FS_BLOCK_SIZE;
	self_entry->ino = new_inode_addr;
	strcpy(self_entry->name, ".");

	a1fs_dentry *parent_entry = (void *)self_entry + 1 * sizeof(a1fs_dentry);
	parent_entry->ino = cur_inode;
	printf("parent_entry->ino: %d\n", parent_entry->ino);
	strcpy(parent_entry->name, "..");

	// printf("\n");
	// printf("Final testing:\n");
	// printf("Inode bitmap with the one in mkdir:\n");
	// print_bitmap(inode_bitmap);
	// printf("\n");

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
	printf("Start a1fs_mkdir with path %s.\n", path);
	fs_ctx *fs = get_fs();

	// IMPLEMENTED
	(void)path;
	(void)fs;

	// IMPLEMENT
	(void)fs;
	char cpy_path[(int)strlen(path) + 1];
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
	a1fs_dentry *first_dentry;
	// int first_while =

	// int cur_inode;

	while (curfix != NULL)
	{
		// cur = pioneer;
		printf("Enter the while loop with curfix: %s\n", curfix);

		// cur->mode = S_IFDIR;

		if ((!(cur->mode & S_IFDIR)))
		{
			fprintf(stderr, "Not a directory and not the last one.\n");
			return -ENOTDIR;
		}
		printf("cur->dentry_count: %d\n", cur->dentry_count);
		// indicator for whether the directory is found, 1 for ont found and 0 for found
		int flag = 1;
		extent = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;
		first_dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;
		dentry = first_dentry;

		// not a directory and not the last one.
		if (fix_count == cur_fix_index)
		{
			// cur_inode = dentry->ino;
			break;
			/** At this point, cur is the inode of the parent directory and curfix is the name of the new directory to be added. */
		}
		cur_fix_index++;
		// printf("dentry->ino: %d", dentry->ino);
		// printf("Before check if the last prefix. fix_count == %d, cur_fix_index == %d.\n", fix_count, cur_fix_index);

		for (int i = 0; i < cur->dentry_count; i++)
		{
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
		printf("Exit for loop.\n");

		if (flag)
		{ // does not exist
			fprintf(stderr, "Does not exist.\n");
			return -ENOENT;
		}

		curfix = strtok(NULL, delim);
	}

	if (curfix == NULL)
	{
		fprintf(stderr, "curfis == null in mkdir, something wrong in the loop.\n");
		return 1;
	}

	/** At this point, cur is the inode of the parent directory and curfix is the name of the directory to be deleted. */

	a1fs_extent *cur_ext_block = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;

	a1fs_dentry *first_parent_entry = (void *)image + cur_ext_block->start * A1FS_BLOCK_SIZE;
	a1fs_dentry *target_entry;
	int target_entry_index;
	int flag2 = 1;
	printf("Before entering the for loop, curfix: %s, cur->dentry_count: %d\n", curfix, cur->dentry_count);
	for (int i = 0; i < cur->dentry_count; i++)
	{
		printf("Enter for loop.\n");
		printf("Loop invariant (i): %d\n", i);
		target_entry = (void *)first_parent_entry + i * sizeof(a1fs_dentry);
		printf("target_entry->name: %s\n", target_entry->name);

		printf("dentry->name: %s, curfix: %s\n", target_entry->name, curfix);
		if (strcmp(target_entry->name, curfix) == 0)
		{
			printf("The inode for the directory to be deleted is %d.\n", target_entry->ino);
			target_entry_index = i;
			flag2 = 0;
			break;
		}
	}
	printf("Exit for loop.\n");

	if (flag2)
	{ // The direcotory does not exist
		printf("The directory does not exist.\n");
		return 1;
	}

	a1fs_inode *target_inode = (void *)first_inode + target_entry->ino * sizeof(a1fs_inode);
	// printf("Target inode: %d, target_inode->dentry_count\n", target_entry->ino);
	if (target_inode->dentry_count > 2)
	{ // it is not empty
		printf("This directory is not empty.\n.");
		return -ENOTEMPTY;
	}

	a1fs_blk_t *data_bitmap = (void *)image + sb->first_db * A1FS_BLOCK_SIZE;
	a1fs_extent *rm_ext_block;
	a1fs_extent *first_ext_block = (void *)image + target_inode->ext_block * A1FS_BLOCK_SIZE;
	int start_data;
	printf("Before for loop the target_inode->ext_count: %d\n", target_inode->ext_count);
	for (int i = 0; i < target_inode->ext_count; i++)
	{
		rm_ext_block = (void *)first_ext_block + i * sizeof(a1fs_extent);
		start_data = rm_ext_block->start;
		printf("Loop invatiant (i): %d, rm_ext_block->count: %d", i, rm_ext_block->count);
		for (int j = 0; j < (int)(rm_ext_block->count); j++)
		{
			// delete data
			printf("Data bitmap to be set off: %d\n, overall location: %d, sb->first_data: %d\n", start_data + j - sb->first_data, start_data + j, sb->first_data);
			setBitOff(data_bitmap, (uint32_t)(start_data + j - sb->first_data));
		}
	}

	// delete extend block
	printf("Data Extent bitmap to be set off: %ld\n", target_inode->ext_block);
	setBitOff(data_bitmap, (uint32_t)(target_inode->ext_block - sb->first_data));

	// delete inode
	// delete from inode bitmap
	a1fs_blk_t *inode_bitmap = (void *)image + sb->first_ib * A1FS_BLOCK_SIZE;
	setBitOff(inode_bitmap, (uint32_t)target_entry->ino);

	// modify dentry so it is organized properly
	a1fs_dentry *modify_entry = target_entry;
	a1fs_dentry *remain_entry;
	printf("target_entry_index: %d, cur->dentry_count: %d\n", target_entry_index, cur->dentry_count);
	for (int i = target_entry_index + 1; i < cur->dentry_count; i++)
	{
		printf("for loop: %d\n", i);
		remain_entry = (void *)first_parent_entry + i * sizeof(a1fs_dentry);
		printf("remain_entry->ino: %d\n", remain_entry->ino);
		printf("remain_entry->name: %s\n", remain_entry->name);
		modify_entry->ino = remain_entry->ino;
		strcpy(modify_entry->name, remain_entry->name);
		modify_entry = remain_entry;
		printf("remain_entry->ino: %d\n", modify_entry->ino);
		printf("remain_entry->name: %s\n", modify_entry->name);
	}

	// modify inode dentry_count
	cur->dentry_count -= 1;

	return 0;
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

	// IMPLEMENTED
	(void)path;
	(void)mode;
	(void)fs;

	char cpy_path[(int)strlen(path) + 1];
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
	a1fs_dentry *first_dentry;
	// int first_while =

	// int cur_inode;

	while (curfix != NULL)
	{
		// cur = pioneer;
		printf("Enter the while loop with curfix: %s\n", curfix);

		// cur->mode = S_IFDIR;

		if ((!(cur->mode & S_IFDIR)))
		{
			fprintf(stderr, "Not a directory and not the last one.\n");
			return -ENOTDIR;
		}
		printf("cur->dentry_count: %d\n", cur->dentry_count);
		// indicator for whether the directory is found, 1 for ont found and 0 for found
		int flag = 1;
		extent = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;
		first_dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;
		dentry = first_dentry;

		// not a directory and not the last one.
		if (fix_count == cur_fix_index)
		{
			// cur_inode = dentry->ino;
			break;
			/** At this point, cur is the inode of the parent directory and curfix is the name of the new directory to be added. */
		}
		cur_fix_index++;
		// printf("dentry->ino: %d", dentry->ino);
		// printf("Before check if the last prefix. fix_count == %d, cur_fix_index == %d.\n", fix_count, cur_fix_index);

		for (int i = 0; i < cur->dentry_count; i++)
		{
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
		printf("Exit for loop.\n");

		if (flag)
		{ // does not exist
			fprintf(stderr, "Does not exist.\n");
			return -ENOENT;
		}

		curfix = strtok(NULL, delim);
	}

	if (curfix == NULL)
	{
		fprintf(stderr, "curfis == null in mkdir, something wrong in the loop.\n");
		return 1;
	}

	/** At this point, cur is the inode of the parent directory and curfix is the name of the new directory to be added. */
	// Find position in bitmap and modify the bitmap.
	a1fs_blk_t *inode_bitmap = (void *)image + sb->first_ib * A1FS_BLOCK_SIZE;
	int new_inode_addr = find_free_from_bitmap(inode_bitmap, sb->inode_count);
	printf("Free inode location: %d (should be 1)\n", new_inode_addr);
	if (new_inode_addr < 0)
	{
		fprintf(stderr, "All inode full (all inode bitmap 1)\n");
		return 1;
	}
	sb->free_inode_count -= 1;
	setBitOn(inode_bitmap, new_inode_addr);

	a1fs_blk_t *data_bitmap = (void *)image + sb->first_db * A1FS_BLOCK_SIZE;
	int new_ext_addr = find_free_from_bitmap(data_bitmap, sb->dblock_count);
	printf("Free extent block location: %d (should be 2)\n", new_ext_addr);
	if (new_ext_addr < 0)
	{
		fprintf(stderr, "All data full (all data bitmap 1)\n");
		return 1;
	}
	setBitOn(data_bitmap, new_ext_addr);

	int new_data_attr = find_free_from_bitmap(data_bitmap, sb->dblock_count);
	printf("Free datablock location: %d (should be 3)\n", new_data_attr);
	if (new_data_attr < 0)
	{
		fprintf(stderr, "No more data block.\n");
		setBitOff(data_bitmap, new_ext_addr);
		return 1;
	}
	setBitOn(data_bitmap, new_data_attr);
	sb->free_dblock_count -= 2;

	// Modify inode.
	cur->dentry_count += 1;

	// Add inode.
	void *inode_block = (void *)(image + sb->first_inode * A1FS_BLOCK_SIZE);
	a1fs_inode *new_inode = (void *)inode_block + new_inode_addr * sizeof(a1fs_inode);
	new_inode->links = 2;
	new_inode->mode = S_IFREG | mode;
	clock_gettime(CLOCK_REALTIME, &(new_inode->mtime));
	// new_inode->size=NULL;
	new_inode->dentry_count = 2;
	new_inode->ext_block = sb->first_data + new_ext_addr;
	new_inode->ext_count = 1;

	printf("\n");

	void *first_data = (void *)image + sb->first_data * A1FS_BLOCK_SIZE;
	// // Modify extent.
	a1fs_extent *cur_ext_block = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;

	// Modify dentry of the parent directory.
	a1fs_dentry *first_parent_entry = (void *)image + cur_ext_block->start * A1FS_BLOCK_SIZE;
	a1fs_dentry *tareget_entry = (void *)first_parent_entry + sizeof(a1fs_dentry) * (cur->dentry_count - 1);
	tareget_entry->ino = new_inode_addr;
	strcpy(tareget_entry->name, curfix);
	printf("tareget_entry->name: %s\n", tareget_entry->name);

	// Add extent block.
	a1fs_extent *extent_block = (void *)first_data + new_ext_addr * A1FS_BLOCK_SIZE;
	extent_block->start = new_data_attr + sb->first_data;
	extent_block->count = 1;

	return 0;
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
	printf("Start a1fs_unlink with path %s.\n", path);
	fs_ctx *fs = get_fs();

	// IMPLEMENTED
	(void)path;
	(void)fs;

	// IMPLEMENT
	(void)fs;
	char cpy_path[(int)strlen(path) + 1];
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
	a1fs_dentry *first_dentry;
	// int first_while =

	// int cur_inode;

	while (curfix != NULL)
	{
		// cur = pioneer;
		printf("Enter the while loop with curfix: %s\n", curfix);

		// cur->mode = S_IFDIR;

		if ((!(cur->mode & S_IFDIR)))
		{
			fprintf(stderr, "Not a directory and not the last one.\n");
			return -ENOTDIR;
		}
		printf("cur->dentry_count: %d\n", cur->dentry_count);
		// indicator for whether the directory is found, 1 for ont found and 0 for found
		int flag = 1;
		extent = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;
		first_dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;
		dentry = first_dentry;

		// not a directory and not the last one.
		if (fix_count == cur_fix_index)
		{
			// cur_inode = dentry->ino;
			break;
			/** At this point, cur is the inode of the parent directory and curfix is the name of the new directory to be added. */
		}
		cur_fix_index++;
		// printf("dentry->ino: %d", dentry->ino);
		// printf("Before check if the last prefix. fix_count == %d, cur_fix_index == %d.\n", fix_count, cur_fix_index);

		for (int i = 0; i < cur->dentry_count; i++)
		{
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
		printf("Exit for loop.\n");

		if (flag)
		{ // does not exist
			fprintf(stderr, "Does not exist.\n");
			return -ENOENT;
		}

		curfix = strtok(NULL, delim);
	}

	if (curfix == NULL)
	{
		fprintf(stderr, "curfis == null in mkdir, something wrong in the loop.\n");
		return 1;
	}

	/** At this point, cur is the inode of the parent directory and curfix is the name of the directory to be deleted. */

	a1fs_extent *cur_ext_block = (void *)image + cur->ext_block * A1FS_BLOCK_SIZE;

	a1fs_dentry *first_parent_entry = (void *)image + cur_ext_block->start * A1FS_BLOCK_SIZE;
	a1fs_dentry *target_entry;
	int target_entry_index;
	int flag2 = 1;
	printf("Before entering the for loop, curfix: %s, cur->dentry_count: %d\n", curfix, cur->dentry_count);
	for (int i = 0; i < cur->dentry_count; i++)
	{
		printf("Enter for loop.\n");
		printf("Loop invariant (i): %d\n", i);
		target_entry = (void *)first_parent_entry + i * sizeof(a1fs_dentry);
		printf("target_entry->name: %s\n", target_entry->name);

		printf("dentry->name: %s, curfix: %s\n", target_entry->name, curfix);
		if (strcmp(target_entry->name, curfix) == 0)
		{
			printf("The inode for the directory to be deleted is %d.\n", target_entry->ino);
			target_entry_index = i;
			flag2 = 0;
			break;
		}
	}
	printf("Exit for loop.\n");

	if (flag2)
	{ // The direcotory does not exist
		printf("The file does not exist.\n");
		return 1;
	}

	a1fs_inode *target_inode = (void *)first_inode + target_entry->ino * sizeof(a1fs_inode);
	if (target_inode->mode & S_IFDIR)
	{ // it is a file
		printf("This is not a file.\n.");
		return -ENOTEMPTY;
	}

	a1fs_blk_t *data_bitmap = (void *)image + sb->first_db * A1FS_BLOCK_SIZE;
	a1fs_extent *rm_ext_block;
	a1fs_extent *first_ext_block = (void *)image + target_inode->ext_block * A1FS_BLOCK_SIZE;
	int start_data;
	printf("Before for loop the target_inode->ext_count: %d\n", target_inode->ext_count);
	for (int i = 0; i < target_inode->ext_count; i++)
	{
		rm_ext_block = (void *)first_ext_block + i * sizeof(a1fs_extent);
		start_data = rm_ext_block->start;
		printf("Loop invatiant (i): %d, rm_ext_block->count: %d", i, rm_ext_block->count);
		for (int j = 0; j < (int)(rm_ext_block->count); j++)
		{
			// delete data
			printf("Data bitmap to be set off: %d\n, overall location: %d, sb->first_data: %d\n", start_data + j - sb->first_data, start_data + j, sb->first_data);
			setBitOff(data_bitmap, (uint32_t)(start_data + j - sb->first_data));
		}
	}

	// delete extend block
	printf("Data Extent bitmap to be set off: %ld\n", target_inode->ext_block);
	setBitOff(data_bitmap, (uint32_t)(target_inode->ext_block - sb->first_data));

	// delete inode
	// delete from inode bitmap
	a1fs_blk_t *inode_bitmap = (void *)image + sb->first_ib * A1FS_BLOCK_SIZE;
	setBitOff(inode_bitmap, (uint32_t)target_entry->ino);

	// modify dentry so it is organized properly
	a1fs_dentry *modify_entry = target_entry;
	a1fs_dentry *remain_entry;
	printf("target_entry_index: %d, cur->ext_count: %d\n", target_entry_index, cur->dentry_count);
	for (int i = target_entry_index + 1; i < cur->dentry_count; i++)
	{
		printf("for loop: %d\n", i);
		remain_entry = (void *)first_parent_entry + i * sizeof(a1fs_dentry);
		printf("remain_entry->ino: %d\n", remain_entry->ino);
		printf("remain_entry->name: %s\n", remain_entry->name);
		modify_entry->ino = remain_entry->ino;
		strcpy(modify_entry->name, remain_entry->name);
		modify_entry = remain_entry;
		printf("remain_entry->ino: %d\n", modify_entry->ino);
		printf("remain_entry->name: %s\n", modify_entry->name);
	}

	// modify inode dentry_count
	cur->dentry_count -= 1;

	return 0;
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

	// IMPLEMENT
	(void)from;
	(void)to;
	(void)fs;

	void *image = fs->image;

	// char cpy_from[(int)strlen(from)+1];
	// strcpy(cpy_from, from);
	// char *delim = "/";
	// char *curfix = strtok(cpy_from, delim);

	// clarify the confussion of treating the last one as none directory and return error
	// int fix_count = num_entry_name(from);
	// int cur_fix_index = 1;

	a1fs_superblock *sb = (void *)image;

	char from_parent[strlen(from) + 1];
	if (get_parent_path(from, from_parent) != 0)
	{
		printf("get_parent_path Error\n");
		return 1;
	}
	printf("from_parent: %s\n", from_parent);

	char to_parent[strlen(to) + 1];
	if (get_parent_path(to, to_parent) != 0)
	{
		printf("get_parent_path Error\n");
		return 1;
	}
	printf("to_parent: %s\n", to_parent);

	printf("Start find_inode_from_path.\n");
	int from_parent_inode_index = find_inode_from_path(from_parent);
	// int from_inode_index = find_inode_from_path(from);
	int to_parent_inode_index = find_inode_from_path(to_parent);
	int to_inode_index = find_inode_from_path(to);
	printf("from_parent_inode_index: %d, to_parent_inode_index: %d, to_inode_index: %d\n", from_parent_inode_index, to_parent_inode_index, to_inode_index);

	a1fs_inode *first_inode = (void *)image + sb->first_inode * A1FS_BLOCK_SIZE;

	a1fs_inode *from_parent_inode = (void *)first_inode + from_parent_inode_index * sizeof(a1fs_inode);
	a1fs_extent *from_extend = (void *)image + from_parent_inode->ext_block * A1FS_BLOCK_SIZE;
	a1fs_dentry *first_from_entry = (void *)image + from_extend->start * A1FS_BLOCK_SIZE;

	a1fs_inode *to_parent_inode = (void *)first_inode + to_parent_inode_index * sizeof(a1fs_inode);
	a1fs_extent *to_parent_extend = (void *)image + to_parent_inode->ext_block * A1FS_BLOCK_SIZE;
	a1fs_dentry *first_to_parent_entry = (void *)image + to_parent_extend->start * A1FS_BLOCK_SIZE;
	// a1fs_inode *from_inode = (void *)image + from_inode_index * A1FS_BLOCK_SIZE;
	// a1fs_inode *to_inode = (void *)first_inode + to_inode_index * sizeof(a1fs_inode);

	char entry_from_name[strlen(from) + 1];
	get_last_entry(from, entry_from_name);

	char entry_to_name[strlen(to) + 1];
	get_last_entry(to, entry_to_name);

	// entry_name may not exit
	int ext_to_ind = get_ext_index_from_inode(to_parent_inode, entry_to_name);

	if (ext_to_ind > 0)
	{ // it exists
		a1fs_dentry *target_to_entry = (void *)first_to_parent_entry + ext_to_ind * sizeof(a1fs_dentry);
		a1fs_inode *remove_inode = (void *)first_from_entry + target_to_entry->ino * sizeof(a1fs_inode);
		if (remove_inode->mode & S_IFDIR)
		{ // Directory
			a1fs_rmdir(to);
		}
		else
		{ // Regular file
			a1fs_unlink(to);
		}
	}

	printf("to_parent_inode: %d\n", to_parent_inode_index);
	printf("to_parent_inode->ext_count: %d\n", to_parent_inode->ext_count);
	if (to_parent_inode->ext_count > 512)
	{
		fprintf(stderr, "to_parent_inode has reach 512 extents extent limit.");
		return -ENOSPC;
	}

	printf("Before getting ext_ind for entry_from_name (%s) form from_parent_inode\n", entry_from_name);
	int ext_ind = get_ext_index_from_inode(from_parent_inode, entry_from_name);
	if (ext_ind < 0)
	{
		fprintf(stderr, "Error with get_ext_index_from_inode.\n");
		return 1;
	}

	a1fs_dentry *target_from_entry = (void *)first_from_entry + ext_ind * sizeof(a1fs_dentry);
	a1fs_dentry *new_entry = (void *)first_to_parent_entry + to_parent_inode->dentry_count * sizeof(a1fs_dentry);

	printf("Before moving:\n");
	printf("target_from_entry->name: %s ", target_from_entry->name);
	printf("target_from_entry->ino: %d\n", target_from_entry->ino);

	new_entry->ino = target_from_entry->ino;
	strcpy(new_entry->name, entry_to_name);

	printf("After moving:\n");
	printf("new_entry->name: %s ", new_entry->name);
	printf("new_entry->ino: %d\n", new_entry->ino);

	printf("Before forward_layback_extents:\n");
	printf("from_parent_inode->ext_block: %ld ", from_parent_inode->ext_block);
	printf("first_from_entry->ino: %d ", first_from_entry->ino);
	printf("first_from_entry->name: %s\n", first_from_entry->name);
	printf("target_from_entry->ino: %d ", target_from_entry->ino);
	printf("target_from_entry->name: %s\n", target_from_entry->name);
	printf("ext_ind: %d\n", ext_ind);

	forward_layback_extents(from_parent_inode, first_from_entry, target_from_entry, ext_ind);

	printf("from_parent_inode->ext_block:%ld, from_parent_inode->dentry_count before modification: %d\n", from_parent_inode->ext_block, from_parent_inode->dentry_count);
	from_parent_inode->dentry_count--;
	printf("from_parent_inode->ext_block:%ld, from_parent_inode->dentry_count after modification: %d\n", from_parent_inode->ext_block, from_parent_inode->dentry_count);
	to_parent_inode->dentry_count++;
	
	printf("Finish rename\n");

	return 0;
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

	// IMPLEMENT
	(void)fs;
	(void)tv;
	char cpy_path[(int)strlen(path) + 1];
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
	a1fs_dentry *first_dentry;
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

		first_dentry = (void *)image + extent->start * A1FS_BLOCK_SIZE;
		for (int i = 0; i < cur->dentry_count; i++)
		{
			printf("Enter the for loop with i == %d\n", i);
			dentry = (void *)first_dentry + i * sizeof(a1fs_dentry);
			printf("Debtry Name: %s\n", dentry->name);
			if (strcmp(dentry->name, curfix) == 0)
			{ // directory/file is found
				cur = (void *)first_inode + dentry->ino * sizeof(a1fs_inode);
				flag = 0;
				break;
			}
		}

		printf("Exit for loop.\n");

		if (flag)
		{ // does not exist
			fprintf(stderr, "Does not exist.\n");
			return -ENOENT;
		}

		curfix = strtok(NULL, delim);
	}

	cur->mtime = tv[1];

	return 0;
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
//	(void)path;
//	(void)buf;
//	(void)size;
//	(void)offset;
//	(void)fs;

	// Follow the path to find the file
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

        // Now cur should be pointing to the file we are reading
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
