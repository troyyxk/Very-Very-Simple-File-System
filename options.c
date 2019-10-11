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
 * CSC369 Assignment 1 - a1fs command line options parser implementation.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "options.h"


// We are using the existing option parsing infrastructure in FUSE.
// See fuse_opt.h in libfuse source code for details.

#define A1FS_OPT(t, p) { t, offsetof(a1fs_opts, p), 1 }

static const struct fuse_opt opt_spec[] = {
	A1FS_OPT("-h"    , help),
	A1FS_OPT("--help", help),

	A1FS_OPT("-V"       , version),
	A1FS_OPT("--version", version),

	A1FS_OPT("--sync"   , sync   ),
	A1FS_OPT("--verbose", verbose),

	FUSE_OPT_END
};

static const char *help_str = "\
Usage: %s image dir [options]\n\
\n\
Mount a1fs image file at given mount point. Use fusermount(1) to unmount.\n\
Only single-threaded mount is supported; -s FUSE option is implied.\n\
\n\
general options:\n\
    -o opt,[opt...]        mount options\n\
    -h   --help            print help\n\
    -V   --version         print version\n\
\n\
a1fs options:\n\
    --sync                 sync image file contents to disk on unmount\n\
    --verbose              verbose output; only useful in foreground mode (-f)\n\
\n\
";

// Callback for fuse_opt_parse()
static int opt_proc(void *data, const char *arg, int key, struct fuse_args *out)
{
	a1fs_opts *opts = (a1fs_opts*)data;
	(void)out;// unused

	if ((key == FUSE_OPT_KEY_NONOPT) && (opts->img_path == NULL)) {
		opts->img_path = strdup(arg);
		return 0;
	}
	return 1;
}


bool a1fs_opt_parse(struct fuse_args *args, a1fs_opts *opts)
{
	if (fuse_opt_parse(args, opts, opt_spec, opt_proc) != 0) return false;

	//NOTE: printing to stderr to keep it consistent with FUSE
	if (opts->help) {
		fprintf(stderr, help_str, args->argv[0]);
		fuse_opt_add_arg(args, "-ho");
	}
	if (opts->version) {
		fprintf(stderr, "a1fs 0.0\n");
		fuse_opt_add_arg(args, "-V");
	}

	if (!opts->help && !opts->version && !opts->img_path) {
		fprintf(stderr, "Missing image path\n");
		return false;
	}

	// Only single-threaded mount is supported
	fuse_opt_add_arg(args, "-s");
	return true;
}
