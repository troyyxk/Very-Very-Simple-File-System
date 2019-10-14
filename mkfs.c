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
 * CSC369 Assignment 1 - a1fs formatting tool.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>

#include "a1fs.h"
#include "map.h"

/** Command line options. */
typedef struct mkfs_opts
{
	/** File system image file path. */
	const char *img_path;
	/** Number of inodes. */
	size_t n_inodes;

	/** Print help and exit. */
	bool help;
	/** Overwrite existing file system. */
	bool force;
	/** Sync memory-mapped image file contents to disk. */
	bool sync;
	/** Verbose output. If false, the program must only print errors. */
	bool verbose;
	/** Zero out image contents. */
	bool zero;

} mkfs_opts;

static const char *help_str = "\
Usage: %s options image\n\
\n\
Format the image file into a1fs file system. The file must exist and\n\
its size must be a multiple of a1fs block size - %zu bytes.\n\
\n\
Options:\n\
    -i num  number of inodes; required argument\n\
    -h      print help and exit\n\
    -f      force format - overwrite existing a1fs file system\n\
    -s      sync image file contents to disk\n\
    -v      verbose output\n\
    -z      zero out image contents\n\
";

size_t inode_size = sizeof(a1fs_inode);

static void print_help(FILE *f, const char *progname)
{
	fprintf(f, help_str, progname, A1FS_BLOCK_SIZE);
}

static bool parse_args(int argc, char *argv[], mkfs_opts *opts)
{
	char o;
	while ((o = getopt(argc, argv, "i:hfsvz")) != -1)
	{
		switch (o)
		{
		case 'i':
			opts->n_inodes = strtoul(optarg, NULL, 10);
			break;

		case 'h':
			opts->help = true;
			return true; // skip other arguments
		case 'f':
			opts->force = true;
			break;
		case 's':
			opts->sync = true;
			break;
		case 'v':
			opts->verbose = true;
			break;
		case 'z':
			opts->zero = true;
			break;

		case '?':
			return false;
		default:
			assert(false);
		}
	}

	if (optind >= argc)
	{
		fprintf(stderr, "Missing image path\n");
		return false;
	}
	opts->img_path = argv[optind];

	if (opts->n_inodes == 0)
	{
		fprintf(stderr, "Missing or invalid number of inodes\n");
		return false;
	}
	return true;
}

/** Determine if the image has already been formatted into a1fs. */
static bool a1fs_is_present(void *image)
{
	//TODO
	(void)image;
	a1fs_superblock *as = (a1fs_superblock *)image;
	if (as->magic == A1FS_MAGIC)
	{
		return true;
	}
	else
	{
		return false;
	}
}

int init_super(a1fs_superblock *sb, int n_inode, int n_sb, int n_ib, int n_db, int n_iblock, int n_data, size_t size)
{
	sb->magic = A1FS_MAGIC;
	sb->size = size;

	// Address
	sb->first_ib = n_sb;
	sb->first_db = n_sb + n_ib;
	sb->first_inode = n_sb + n_ib + n_db;
	sb->first_data = n_sb + n_ib + n_db + n_iblock;

	// Amount
	sb->inode_count = n_inode;
	sb->ib_count = n_ib;
	sb->db_count = n_db;
	sb->iblock_count = n_iblock;
	sb->free_iblock_count = n_iblock;
	sb->dblock_count = n_data;
	sb->free_dblock_count = n_data;

	return 0;
}

/**
 * Format the image into a1fs.
 *
 * NOTE: Must update mtime of the root directory.
 *
 * @param image  pointer to the start of the image.
 * @param size   image size in bytes.
 * @param opts   command line options.
 * @return       true on success;
 *               false on error, e.g. options are invalid for given image size.
 */
static bool mkfs(void *image, size_t size, mkfs_opts *opts)
{
	//TODO
	(void)image;
	(void)size; // The size of the image in byte.
	(void)opts;

	/** Amount */

	int n_block = (double)size / (double)A1FS_BLOCK_SIZE;
	int n_sb = 1;
	int n_inode = opts->n_inodes;
	int n_ib = ceil((double)n_inode / ((double)A1FS_BLOCK_SIZE * 8));
	int n_db = ceil((double)n_block / ((double)A1FS_BLOCK_SIZE * 8)); // 需要改一下

	// in byte
	int n_iblock = ceil((double)n_inode * inode_size / (double)A1FS_BLOCK_SIZE);
	// nubmer of data block is the total number of:
	// super block
	// inode bitmap
	// data bitmap
	// inode block
	int n_data = n_block - n_sb - n_ib - n_db - n_iblock;

	// /** Address */

	// Init Super Block
	a1fs_superblock *sb = (void *)image + (A1FS_BLOCK_SIZE * 0);
	if (init_super(sb, n_inode, n_sb, n_ib, n_db, n_iblock, n_data, size) != 0)
	{
		fprintf(stderr, "Failed to Init Super Block\n");
		return false;
	}

	// Init Inode Bitmap
	unsigned char *inode_bitmap = (unsigned char *)(image + sb->first_ib * A1FS_BLOCK_SIZE);
	for (int i = 0; i < sb->ib_count; i++)
	{
		inode_bitmap[i] = 0;
	}

	// Init Data Bitmap
	unsigned char *data_bitmap = (unsigned char *)(image + sb->first_db * A1FS_BLOCK_SIZE);
	for (int i = 0; i < sb->db_count; i++)
	{
		data_bitmap[i] = 0;
	}

	// Init Inodes ?

	return true;
}

int main(int argc, char *argv[])
{
	mkfs_opts opts = {0}; // defaults are all 0
	if (!parse_args(argc, argv, &opts))
	{
		// Invalid arguments, print help to stderr
		print_help(stderr, argv[0]);
		return 1;
	}
	if (opts.help)
	{
		// Help requested, print it to stdout
		print_help(stdout, argv[0]);
		return 0;
	}

	// Map image file into memory
	size_t size;
	void *image = map_file(opts.img_path, A1FS_BLOCK_SIZE, &size);
	if (image == NULL)
		return 1;

	// Check if overwriting existing file system
	int ret = 1;
	if (!opts.force && a1fs_is_present(image))
	{
		fprintf(stderr, "Image already contains a1fs; use -f to overwrite\n");
		goto end;
	}

	if (opts.zero)
		memset(image, 0, size);
	if (!mkfs(image, size, &opts))
	{
		fprintf(stderr, "Failed to format the image\n");
		goto end;
	}

	// Sync to disk if requested
	if (opts.sync && (msync(image, size, MS_SYNC) < 0))
	{
		perror("msync");
		goto end;
	}

	ret = 0;
end:
	munmap(image, size);
	return ret;
}
