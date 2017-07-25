/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MEMORY_PROF_H__
#define __MEMORY_PROF_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>

#include "memory_prof_util.h"

#define NUM_REPS_FOR_HEAP_PROFILING 50
#define NUM_REPS_FOR_REPEATABILITY 100
#define ION_PRE_ALLOC_SIZE_DEFAULT 0 /* 0 MB */
#define MAX_PRE_ALLOC_SIZE 5000 /* 5000 MB */
#define MAX_ALLOC_PROFILE_LINE_LEN 500
#define MAX_ALLOC_PROFILE_FIELDS 20
#define MAX_ALLOC_PROFILE_WORD_LEN 80
#define MAX_HEAP_ID_STRING_LEN 40
#define MAX_FLAGS_STRING_LEN 100
#define MAX_FLAGS 15
#define MAX_SIZE_STRING_LEN 15

#define MEMORY_PROF_DEV "/dev/memory_prof"
#define ION_DEV		"/dev/ion"

/*
 * Don't change the format of the following line strings. We need to
 * rely on them for parsing.
 */
#define ST_PREFIX_DATA_ROW	"=>"
#define ST_PREFIX_PREALLOC_SIZE "==>"

enum alloc_op_enum {
	OP_NONE,
	OP_ALLOC,
	OP_SLEEP,
};

struct alloc_profile_entry {
	enum alloc_op_enum op;
	union {
		struct alloc_op {
			int reps;
			unsigned int heap_id;
			char heap_id_string[MAX_HEAP_ID_STRING_LEN];
			unsigned int flags;
			char flags_string[MAX_FLAGS_STRING_LEN];
			unsigned long size;
			char size_string[MAX_SIZE_STRING_LEN];
			bool quiet;
			bool profile_mmap;
			bool profile_memset;
		} alloc_op;
		struct sleep_op {
			unsigned int time_us;
		} sleep_op;
	} u;
};

struct alloc_profile_entry *get_alloc_profile(const char *alloc_profile_path);
struct alloc_profile_entry *get_default_alloc_profile(void);

#endif /* __MEMORY_PROF_H__ */
