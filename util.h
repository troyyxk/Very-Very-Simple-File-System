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
 * CSC369 Assignment 1 - Miscellaneous utility functions.
 */

#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>


/** Check if x is a power of 2. */
static inline bool is_powerof2(size_t x)
{
	return (x & (x - 1)) == 0;
}

/** Check if x is a multiple of alignment (which must be a power of 2). */
static inline bool is_aligned(size_t x, size_t alignment)
{
	assert(is_powerof2(alignment));
	return (x & (alignment - 1)) == 0;
}

/** Align x up to a multiple of alignment (which must be a power of 2). */
static inline size_t align_up(size_t x, size_t alignment)
{
	assert(is_powerof2(alignment));
	return (x + alignment - 1) & (~alignment + 1);
}
