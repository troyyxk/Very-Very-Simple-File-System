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
 * CSC369 Assignment 1 - a1fs types, constants, and data structures header file.
 */

#pragma once

#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include <sys/stat.h>

/**
 * a1fs block size in bytes. You are not allowed to change this value.
 *
 * The block size is the unit of space allocation. Each file (and directory)
 * must occupy an integral number of blocks. Each of the file systems metadata
 * partitions, e.g. superblock, inode/block bitmaps, inode table (but not an
 * individual inode) must also occupy an integral number of blocks.
 */
#define A1FS_BLOCK_SIZE 4096

/** Block number (block pointer) type. */
typedef uint32_t a1fs_blk_t;

/** Inode number type. */
typedef uint32_t a1fs_ino_t;

/** Magic value that can be used to identify an a1fs image. */
#define A1FS_MAGIC 0xC5C369A1C5C369A1ul

/** a1fs superblock. */
typedef struct a1fs_superblock
{
	/** Must match A1FS_MAGIC. */
	uint64_t magic;
	/** File system size in bytes. */
	uint64_t size;

	/** Address */
	/** First Inode Bitmap. */
	int first_ib;
	/** First Data Bitmap. */
	int first_db;
	/** First Inode. */
	int first_inode;
	/** First Data Block. */
	int first_data;

	/** Amount */
	/** Number of Inodes. */
	int inode_count;
	/** Number of Inode Bitmap blocks. */
	int ib_count;
	/** Number of Data Bitmap blocks. */
	int db_count;
	/** Number of inode Blocks. */
	int iblock_count;
	/** Number of free inode block. */
	int free_inode_count;
	/** Number of data Blocks. */
	int dblock_count;
	/** Number of free data block. */
	int free_dblock_count;

} a1fs_superblock;

// Superblock must fit into a single block
static_assert(sizeof(a1fs_superblock) <= A1FS_BLOCK_SIZE,
			  "superblock is too large");

/** Extent - a contiguous range of blocks. */
typedef struct a1fs_extent
{
	/** Starting block of the extent. */
	a1fs_blk_t start;
	/** Number of blocks in the extent. */
	a1fs_blk_t count;

} a1fs_extent;

/** a1fs inode. */
// typedef struct a1fs_inode
// {
// 	/** File mode. */ // either directory or file
// 	mode_t mode;
// 	/** Reference count (number of hard links). */
// 	uint32_t links;
// 	/** File size in bytes. */
// 	uint64_t size;

// 	/**
// 	 * Last modification timestamp.
// 	 *
// 	 * Use the CLOCK_REALTIME clock; see "man 3 clock_gettime". Must be updated
// 	 * when the file (or directory) is created, written to, or its size changes.
// 	 */
// 	struct timespec mtime;

// 	/** Extend Block*/
// 	long ext_block;

// 	/** Number of Extend. */
// 	short ext_count;
// 	/** Number of Entry in a Directory. */
// 	short dentry_count;

// 	uint32_t buffer;
// } a1fs_inode;

/** a1fs inode. */
typedef struct a1fs_inode
{
	/** File mode. */
	mode_t mode;
	/** Reference count (number of hard links). */
	uint32_t links;
	/** File size in bytes. */
	uint64_t size;

	/**
	 * Last modification timestamp.
	 *
	 * Use the CLOCK_REALTIME clock; see "man 3 clock_gettime". Must be updated
	 * when the file (or directory) is created, written to, or its size changes.
	 */
	struct timespec mtime;

	/** Number of Extend. */
	short ext_count;
	/** Extend Block*/
	long ext_block;
	/** Number of Entry in a Directory. */
	short dentry_count;
	char buf[7];
} a1fs_inode;

// A single block must fit an integral number of inodes
static_assert(A1FS_BLOCK_SIZE % sizeof(a1fs_inode) == 0, "invalid inode size");

/** Maximum file name (path component) length. Includes the null terminator. */
#define A1FS_NAME_MAX 252

/**
 * Ext2 directory file types.  Only the low 3 bits are used.  The
 * other bits are reserved for now.
 */
#define EXT2_FT_UNKNOWN 0  /* Unknown File Type */
#define EXT2_FT_REG_FILE 1 /* Regular File */
#define EXT2_FT_DIR 2	  /* Directory File */
#define EXT2_FT_SYMLINK 7  /* Symbolic Link */
/* Other types, irrelevant for the assignment */
/* #define EXT2_FT_CHRDEV   3 */ /* Character Device */
/* #define EXT2_FT_BLKDEV   4 */ /* Block Device */
/* #define EXT2_FT_FIFO     5 */ /* Buffer File */
/* #define EXT2_FT_SOCK     6 */ /* Socket File */

/** Maximum file path length. Includes the null terminator. */
#define A1FS_PATH_MAX PATH_MAX

/** Fixed size directory entry structure. */
typedef struct a1fs_dentry
{
	/** Inode number. */
	a1fs_ino_t ino;
	/** File name. A null-terminated string. */
	char name[A1FS_NAME_MAX];

} a1fs_dentry;

static_assert(sizeof(a1fs_dentry) == 256, "invalid dentry size");
