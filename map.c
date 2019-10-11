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
 * CSC369 Assignment 1 - File mapping helper implementation.
 */

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "map.h"
#include "util.h"


void *map_file(const char *path, size_t block_size, size_t *size)
{
	// Open the file for reading and writing
	int fd = open(path, O_RDWR);
	if (fd < 0) {
		perror(path);
		return NULL;
	}

	void *addr = NULL;
	// Get file size
	struct stat s;
	if (fstat(fd, &s) < 0) {
		perror("fstat");
		goto end;
	}

	// Check that the file size is valid
	if (s.st_size == 0) {
		fprintf(stderr, "Image file is empty\n");
		goto end;
	}
	if (s.st_size % block_size != 0) {
		fprintf(stderr, "Image file size is not a multiple of block size\n");
		goto end;
	}

	// Map file contents into memory
	addr = mmap(NULL, s.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		addr = NULL;
		goto end;
	}
	assert(is_aligned((size_t)addr, block_size));
	*size = s.st_size;

end:
	//NOTE: memory mapping keeps a reference to the open file; can safely close
	// the file descriptor now; a future munmap() will close the file
	close(fd);
	return addr;
}
