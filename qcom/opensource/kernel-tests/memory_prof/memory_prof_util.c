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
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <err.h>

#include "memory_prof_util.h"

void set_oom_score_adj_path(const char *path, int adj)
{
	char *val;
	int fd;

	asprintf(&val, "%d", adj);

	fd = open(path, O_WRONLY);
	if (fd < 0)
		err(1, "Couldn't open %s", path);

	if (write(fd, val, strlen(val)) < 0)
		err(1, "Couldn't write to %s", path);

	close(fd);
	free(val);
}

void set_oom_score_adj_pid(int pid, int adj)
{
	char *path;
	asprintf(&path, "/proc/%d/oom_score_adj", pid);
	set_oom_score_adj_path(path, adj);
	free(path);
}

void set_oom_score_adj_parent(int adj)
{
	set_oom_score_adj_pid(getppid(), adj);
}

void set_oom_score_adj_self(int adj)
{
	set_oom_score_adj_path("/proc/self/oom_score_adj", adj);
}

/**
 * timeval_sub - subtracts t2 from t1
 *
 * Returns a timeval with the result
 */
struct timeval timeval_sub(struct timeval t1, struct timeval t2)
{
	struct timeval diff;

	diff.tv_sec = t1.tv_sec - t2.tv_sec;

	if (t1.tv_usec < t2.tv_usec) {
		diff.tv_usec = t1.tv_usec + S_TO_US(1) - t2.tv_usec;
		diff.tv_sec--;
	} else {
		diff.tv_usec = t1.tv_usec - t2.tv_usec;
	}

	return diff;
}

/**
 * timeval_ms_diff - gets the difference (in MS) between t1 and t2
 *
 * Returns the MS diff between t1 and t2 (t1 - t2)
 */
double timeval_ms_diff(struct timeval t1, struct timeval t2)
{
	struct timeval tv_result = timeval_sub(t1, t2);
	return US_TO_MS(tv_result.tv_usec) + S_TO_MS(tv_result.tv_sec);
}

void mprof_tick(struct timeval *tv)
{
	if (gettimeofday(tv, NULL)) {
		/* BAIL */
		errx(1, "couldn't get time of day");
		return;
	}
	return;
}

double mprof_tock(struct timeval *tv)
{
	struct timeval tv_after;
	mprof_tick(&tv_after);
	return timeval_ms_diff(tv_after, *tv);
}

/**
 * Returns true if the given buffers have the same content. If they
 * differ, the offset to the first differing byte is returned in
 * *fail_index.
 */
bool buffers_are_equal(char *src, char *dst, size_t len, int *fail_index)
{
	int i;
	for (i = 0; i < (int) len; ++i)
		if (dst[i] != src[i]) {
			printf("ERROR: buffer integrity check failed at "\
				"offset %d. dst[i]=0x%x, src[i]=0x%x\n",
				i, dst[i], src[i]);
			*fail_index = i;
			return false;
		}
	return true;
}

