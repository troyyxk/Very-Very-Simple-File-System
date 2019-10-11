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
 * CSC369 Assignment 1 - a1fs command line options parser header file.
 */

#pragma once

#include <stdbool.h>

#include <fuse_opt.h>


/** a1fs command line options. */
typedef struct a1fs_opts {
	/** a1fs image file path. */
	const char *img_path;

	/** Print help and exit. FUSE option. */
	int help;
	/** Print version and exit. FUSE option. */
	int version;

	/** Sync memory-mapped image file contents to disk on unmount. */
	int sync;
	/** Verbose output. Only print logging/debug info if this flag is set. */
	int verbose;

} a1fs_opts;

/**
 * Parse a1fs command line options.
 *
 * @param args  pointer to 'struct fuse_args' with the program arguments.
 * @param args  pointer to the options struct that receives the result.
 * @return      true on success; false on failure.
 */
bool a1fs_opt_parse(struct fuse_args *args, a1fs_opts *opts);
