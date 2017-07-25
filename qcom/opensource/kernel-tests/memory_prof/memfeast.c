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

/*
 * memfeast - reliably force the device into a state of low memory
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <limits.h>
#include <stdarg.h>
#include <err.h>
#include <getopt.h>
#include <libgen.h>		/* for basename */

#include "memory_prof_util.h"

#define FEAST_GRANULE_MB 1
#define DEFAULT_FEAST_TARGET_MB 80


/**
 * Some fields from /proc/meminfo (see proc(5)). All values are stored
 * in bytes.
 */
struct meminfo {
	unsigned long memtotal;
	unsigned long memfree;
	unsigned long buffers;
	unsigned long cached;
	unsigned long swap_cached;
};

static void wait_for_enter(void)
{
	char enter;
	for ( enter = getchar();
	      !(enter == '\n' || enter == '\r');
	      enter = getchar())
		;
}

static void bail(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vwarn(fmt, args);
	va_end(args);
	puts("Press enter to exit. All memory will be released.");
	wait_for_enter();
	exit(1);
}


/**
 * Returns true if string starts with prefix
 */
static bool startswith(const char *string, const char *prefix)
{
	size_t l1 = strlen(string);
	size_t l2 = strlen(prefix);
	return strncmp(string, prefix, MIN(l1, l2)) == 0;
}

static void get_meminfo(struct meminfo *meminfo)
{
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	FILE *fp;

	fp = fopen("/proc/meminfo", "r");
	if (!fp)
		bail("Couldn't open /proc/meminfo");

	while ((read = getline(&line, &len, fp)) != -1) {
		if (startswith(line, "MemTotal:")) {
			sscanf(line, "MemTotal: %lu kB\n", &meminfo->memtotal);
			meminfo->memtotal = KB_TO_BYTES(meminfo->memtotal);
			continue;
		}
		if (startswith(line, "MemFree:")) {
			sscanf(line, "MemFree: %lu kB\n", &meminfo->memfree);
			meminfo->memfree = KB_TO_BYTES(meminfo->memfree);
			continue;
		}
		if (startswith(line, "Buffers:")) {
			sscanf(line, "Buffers: %lu kB\n", &meminfo->buffers);
			meminfo->buffers = KB_TO_BYTES(meminfo->buffers);
			continue;
		}
		if (startswith(line, "Cached:")) {
			sscanf(line, "Cached: %lu kB\n", &meminfo->cached);
			meminfo->cached = KB_TO_BYTES(meminfo->cached);
			continue;
		}
		if (startswith(line, "SwapCached:")) {
			sscanf(line, "SwapCached: %lu kB\n",
				&meminfo->swap_cached);
			meminfo->swap_cached =
				KB_TO_BYTES(meminfo->swap_cached);
			continue;
		}
	}

	free(line);
	fclose(fp);
}

/**
 * Returns the total number of bytes up for grabs (memfree + cached +
 * swap_cached + buffers)
 */
static unsigned long up_for_grabs(const struct meminfo *meminfo)
{
	return meminfo->memfree
		+ meminfo->cached
		+ meminfo->swap_cached
		+ meminfo->buffers;
}

static bool reached_target(const struct meminfo *meminfo, unsigned int goal_MB)
{
	return up_for_grabs(meminfo) < MB_TO_BYTES(goal_MB);
}

/**
 * Feast memory until memfree + cache + buffers is < goal_MB
 *
 * The feasted memory is also mlock'd so that it can't be swapped out,
 * which would defeat the purpose of feasting it.
 */
static void feast(unsigned int goal_MB)
{
	struct meminfo meminfo;
	int allocated = 0;

	putchar('\n');
	for (get_meminfo(&meminfo);
	     !reached_target(&meminfo, goal_MB);
	     get_meminfo(&meminfo)) {
		char *buf = malloc(SZ_1M);

		if (!buf)
			bail("malloc failed after %u MB", allocated);

		if (mlock(buf, SZ_1M))
			bail("mlock failed after %u MB", allocated);

		memset(buf, allocated, SZ_1M);

		printf("\r  Allocated: %u MB | Up-for-grabs: %lu MB | memfree: %lu MB | buffers: %lu MB | cached: %lu MB | swap_cached: %lu MB   ",
			allocated,
			BYTES_TO_MB(up_for_grabs(&meminfo)),
			BYTES_TO_MB(meminfo.memfree),
			BYTES_TO_MB(meminfo.buffers),
			BYTES_TO_MB(meminfo.cached),
			BYTES_TO_MB(meminfo.swap_cached));
		fflush(stdout);

		allocated++;
	}
	putchar('\n');
}

#define USAGE_STRING							\
	"%s\n"								\
	"\n"								\
	"Reliably forces the device into a state of low memory\n"	\
	"\n"								\
	"Supported options:\n"						\
	"\n"								\
	"  -h         Print this message and exit\n"			\
	"  -t MB      Set target memfree + cache + buffer size\n"	\
	"             (in MB) (default = %u MB)\n"

static void usage(char *progname)
{
	printf(USAGE_STRING, progname, DEFAULT_FEAST_TARGET_MB);
}

extern char *optarg;

int main(int argc, char *argv[])
{
	int opt;
	unsigned int target_MB = DEFAULT_FEAST_TARGET_MB;

	while (-1 != (opt = getopt(argc, argv, "ht:"))) {
		switch (opt) {
		case 't':
			target_MB = atoi(optarg);
			printf("target_MB: %d\n", target_MB);
			break;
		case 'h':
		default:
			usage(basename(argv[0]));
			exit(1);
		}
	}

	/* make sure we don't get killed: */
	set_oom_score_adj_self(-1000);
	set_oom_score_adj_parent(-1000);

	/* the feasting: */
	feast(target_MB);
	printf("\nFeasted all but %d MB. Press enter to release and exit.\n",
		target_MB);
	fflush(stdout);
	wait_for_enter();

	return 0;
}
