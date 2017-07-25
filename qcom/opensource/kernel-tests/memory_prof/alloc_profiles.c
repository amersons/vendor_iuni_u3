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

#include <stdio.h>
#include <string.h>
#include <err.h>
#include <linux/msm_ion.h>
#include "memory_prof.h"
#include "memory_prof_util.h"

#define MAKE_ALLOC_PROFILES(size_mb, reps, quiet)			\
	MAKE_ALLOC_PROFILE(ION_CP_MM_HEAP_ID, ION_FLAG_SECURE, size_mb, reps, quiet, false, false), \
		MAKE_ALLOC_PROFILE(ION_IOMMU_HEAP_ID, 0, size_mb, reps, quiet, true, true), \
		MAKE_ALLOC_PROFILE(ION_IOMMU_HEAP_ID, ION_FLAG_CACHED, size_mb, reps, quiet, true, true), \
		MAKE_ALLOC_PROFILE(ION_SYSTEM_HEAP_ID, 0, size_mb, reps, quiet, false, false), \
		MAKE_ALLOC_PROFILE(ION_SYSTEM_HEAP_ID, ION_FLAG_CACHED, size_mb, reps, quiet, true, true)

#define MAKE_ALLOC_PROFILE(_heap_id, _flags, _size_mb, _reps, _quiet,	\
			_profile_mmap, _profile_memset)			\
			{ .op = OP_ALLOC,				\
			.u.alloc_op.heap_id = ION_HEAP(_heap_id),	\
			.u.alloc_op.heap_id_string = #_heap_id,		\
			.u.alloc_op.flags = _flags,			\
			.u.alloc_op.flags_string = #_flags,		\
			.u.alloc_op.size = _size_mb * SZ_1M,		\
			.u.alloc_op.size_string = #_size_mb "MB",	\
			.u.alloc_op.reps = _reps,			\
			.u.alloc_op.quiet = _quiet,			\
			.u.alloc_op.profile_mmap = _profile_mmap,	\
			.u.alloc_op.profile_memset = _profile_memset,	\
			}

static struct alloc_profile_entry default_alloc_profile[] = {
	MAKE_ALLOC_PROFILES(1, NUM_REPS_FOR_HEAP_PROFILING, false),
	MAKE_ALLOC_PROFILES(3, NUM_REPS_FOR_HEAP_PROFILING, false),
	MAKE_ALLOC_PROFILES(5, NUM_REPS_FOR_HEAP_PROFILING, false),
	MAKE_ALLOC_PROFILES(8, NUM_REPS_FOR_HEAP_PROFILING, false),
	MAKE_ALLOC_PROFILES(10, NUM_REPS_FOR_HEAP_PROFILING, false),
	MAKE_ALLOC_PROFILES(13, NUM_REPS_FOR_HEAP_PROFILING, false),
	MAKE_ALLOC_PROFILES(20, NUM_REPS_FOR_HEAP_PROFILING, false),
	MAKE_ALLOC_PROFILES(50, NUM_REPS_FOR_HEAP_PROFILING, true),
	MAKE_ALLOC_PROFILES(100, NUM_REPS_FOR_HEAP_PROFILING, true),
	{
		/* sentinel */
		.op = OP_NONE,
	}
};

/**
 * split_string() - split a string by a delimiter
 *
 * @string - string to split
 * @delim - delimiter char
 * @output - output array of C-strings. Must have enough room for us.
 * @output_lens - how much room we have in each entry of @output
 *
 * Returns the number of entries found
 */
static int split_string(const char * const string, char delim, char *output[],
			int output_sizes)
{
	char *word, *string_cpy;
	int nentries = 0;
	size_t len = strlen(string) + 1;
	const char delim_string[2] = { delim, '\0' };

	MALLOC(char *, string_cpy, len);
	STRNCPY_SAFE(string_cpy, string, len);

	word = strtok(string_cpy, delim_string);
	if (!word)
		errx(1, "Malformed line: %s", string_cpy);
	STRNCPY_SAFE(*output, word, output_sizes);
	output++;
	nentries++;
	for (word = strtok(NULL, delim_string);
	     word;
	     word = strtok(NULL, delim_string)) {
		STRNCPY_SAFE(*output, word, output_sizes);
		output++;
		nentries++;
	}

	free(string_cpy);

	return nentries;
}

#define MAKE_HEAP_INFO(heap) { .heap_id = heap, .heap_id_string = #heap }

struct heap_info {
	unsigned int heap_id;
	const char * const heap_id_string;
} heap_info[] = {
	MAKE_HEAP_INFO(ION_CP_MM_HEAP_ID),
	MAKE_HEAP_INFO(ION_CP_MFC_HEAP_ID),
	MAKE_HEAP_INFO(ION_CP_WB_HEAP_ID),
	MAKE_HEAP_INFO(ION_CAMERA_HEAP_ID),
	MAKE_HEAP_INFO(ION_SYSTEM_CONTIG_HEAP_ID),
	MAKE_HEAP_INFO(ION_ADSP_HEAP_ID),
	MAKE_HEAP_INFO(ION_PIL1_HEAP_ID),
	MAKE_HEAP_INFO(ION_SF_HEAP_ID),
	MAKE_HEAP_INFO(ION_IOMMU_HEAP_ID),
	MAKE_HEAP_INFO(ION_PIL2_HEAP_ID),
	MAKE_HEAP_INFO(ION_QSECOM_HEAP_ID),
	MAKE_HEAP_INFO(ION_AUDIO_HEAP_ID),
	MAKE_HEAP_INFO(ION_MM_FIRMWARE_HEAP_ID),
	MAKE_HEAP_INFO(ION_SYSTEM_HEAP_ID),
	MAKE_HEAP_INFO(ION_HEAP_ID_RESERVED),
	/* sentinel */
	{ .heap_id_string = NULL }
};

static int find_heap_id_value(const char * const heap_id_string,
			unsigned int *val)
{
	struct heap_info *h;
	for (h = &heap_info[0]; h->heap_id_string; ++h) {
		if (0 == strcmp(heap_id_string, h->heap_id_string)) {
			*val = h->heap_id;
			return 0;
		}
	}
	return 1;
}

#define MAKE_FLAG_INFO(flag) { .flag_value = flag, .flag_string = #flag }

struct flag_info {
	unsigned int flag_value;
	const char * const flag_string;
} flag_info[] = {
	MAKE_FLAG_INFO(ION_FLAG_CACHED),
	MAKE_FLAG_INFO(ION_FLAG_CACHED_NEEDS_SYNC),
	MAKE_FLAG_INFO(ION_FLAG_FORCE_CONTIGUOUS),
	MAKE_FLAG_INFO(ION_SECURE),
	MAKE_FLAG_INFO(ION_FORCE_CONTIGUOUS),
	MAKE_FLAG_INFO(ION_FLAG_SECURE),
	/* sentinel */
	{ .flag_string = NULL }
};

static int find_flag_value(const char * const flag, int *val)
{
	struct flag_info *f;
	if (0 == strcmp(flag, "0")) {
		*val = 0;
		return 0;
	}
	for (f = &flag_info[0]; f->flag_string; ++f) {
		if (0 == strcmp(flag, f->flag_string)) {
			*val = f->flag_value;
			return 0;
		}
	}
	return 1;
}

static unsigned int parse_flags(const char * const word)
{
	unsigned int ret = 0;
	int i, nflags;
	char *flags_words[MAX_FLAGS];
	size_t len = strlen(word) + 1;

	for (i = 0; i < MAX_FLAGS; ++i)
		MALLOC(char *, flags_words[i], MAX_FLAGS_STRING_LEN);

	nflags = split_string(word, '|', flags_words, MAX_FLAGS_STRING_LEN);

	for (i = 0; i < nflags; ++i) {
		int f;
		if (find_flag_value(flags_words[i], &f))
			errx(1, "Unknown flag: %s\n", flags_words[i]);
		else
			ret |= f;
	}

	for (i = 0; i < MAX_FLAGS; ++i)
		free(flags_words[i]);

	return ret;
}

static bool parse_bool(const char * const word)
{
	return 0 == strcmp("true", word);
}

enum alloc_op_line_idx {
	LINE_IDX_REPS = 1,
	LINE_IDX_HEAP_ID,
	LINE_IDX_FLAGS,
	LINE_IDX_ALLOC_SIZE_BYTES,
	LINE_IDX_ALLOC_SIZE_LABEL,
	LINE_IDX_QUIET_ON_FAILURE,
	LINE_IDX_PROFILE_MMAP,
	LINE_IDX_PROFILE_MEMSET,
};

enum sleep_op_line_idx {
	LINE_IDX_TIME_US = 1,
};

/* how many more alloc profile entries to {re-,m}alloc when we need more */
#define MORE_PROFILE_ENTRIES 30

/**
 * get_alloc_profile() - Get allocation profile
 *
 * @alloc_profile_path: Filename of alloc profile (or NULL to use the
 *                 default). Should already have been verified that
 *                 the file exists.
 */
struct alloc_profile_entry *get_alloc_profile(const char *alloc_profile_path)
{
	FILE *fp;
	char *buf;
	char *words[MAX_ALLOC_PROFILE_FIELDS];
	int i;
	struct alloc_profile_entry *current = NULL, *base = NULL;
	int nentries = 0;
	int current_capacity = 0;
	const size_t more_alloc_size = sizeof(struct alloc_profile_entry)
		* MORE_PROFILE_ENTRIES;

	printf("Using allocation profile: %s\n", alloc_profile_path);

	MALLOC(char *, buf, MAX_ALLOC_PROFILE_LINE_LEN);

	for (i = 0; i < MAX_ALLOC_PROFILE_FIELDS; ++i)
		MALLOC(char *, words[i], MAX_ALLOC_PROFILE_WORD_LEN);

	fp = fopen(alloc_profile_path, "r");
	if (!fp)
		err(1, "Couldn't read %s\n", alloc_profile_path);

	for (;;) {
		struct alloc_profile_entry new;
		int nwords;
		if (current_capacity == nentries) {
			size_t current_size = (current_capacity
					* sizeof(struct alloc_profile_entry));
			REALLOC(struct alloc_profile_entry *,
				base, current_size + more_alloc_size);
			current = base + nentries;
			current_capacity += MORE_PROFILE_ENTRIES;
		}

		buf = fgets(buf, MAX_ALLOC_PROFILE_LINE_LEN, fp);
		if (!buf) {
			if (feof(fp))
				break;
			err(1, "Error reading line %d from %s",
				nentries + 1, alloc_profile_path);
		}

		if (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r')
			continue;

		/* strip off trailing newline */
		buf[strlen(buf) - 1] = '\0';

		nwords = split_string(buf, ',', words,
				MAX_ALLOC_PROFILE_WORD_LEN);
		if (nwords < 1)
			errx(1, "Malformed line: `%s'", buf);

		if (0 == strcmp(words[0], "alloc")) {
			unsigned int heap_id;
			new.op = OP_ALLOC;

			STRTOL(new.u.alloc_op.reps, words[LINE_IDX_REPS], 0);

			if (find_heap_id_value(words[LINE_IDX_HEAP_ID],
						&heap_id))
				errx(1, "Unknown heap_id: %s",
					words[LINE_IDX_HEAP_ID]);

			new.u.alloc_op.heap_id = ION_HEAP(heap_id);
			STRNCPY_SAFE(new.u.alloc_op.heap_id_string,
				words[LINE_IDX_HEAP_ID],
				MAX_HEAP_ID_STRING_LEN);

			new.u.alloc_op.flags
				= parse_flags(words[LINE_IDX_FLAGS]);
			STRNCPY_SAFE(new.u.alloc_op.flags_string,
				words[LINE_IDX_FLAGS],
				MAX_FLAGS_STRING_LEN);

			STRTOL(new.u.alloc_op.size,
				words[LINE_IDX_ALLOC_SIZE_BYTES], 0);
			STRNCPY_SAFE(new.u.alloc_op.size_string,
				words[LINE_IDX_ALLOC_SIZE_LABEL],
				MAX_SIZE_STRING_LEN);

			new.u.alloc_op.quiet
				= parse_bool(words[LINE_IDX_QUIET_ON_FAILURE]);
			new.u.alloc_op.profile_mmap
				= parse_bool(words[LINE_IDX_PROFILE_MMAP]);
			new.u.alloc_op.profile_memset
				= parse_bool(words[LINE_IDX_PROFILE_MEMSET]);
		} else if (0 == strcmp(words[0], "sleep")) {
			new.op = OP_SLEEP;
			STRTOL(new.u.sleep_op.time_us,
				words[LINE_IDX_TIME_US], 0);
		} else {
			errx(1, "Malformed line: `%s'", buf);
		}

		*current++ = new;
		nentries++;
	}

	free(buf);
	for (i = 0; i < MAX_ALLOC_PROFILE_FIELDS; ++i)
		free(words[i]);

	return base;
}

struct alloc_profile_entry *get_default_alloc_profile(void)
{
	return default_alloc_profile;
}
