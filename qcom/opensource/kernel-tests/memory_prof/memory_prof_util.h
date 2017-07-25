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

#ifndef __MEMORY_PROF_UTIL_H__
#define __MEMORY_PROF_UTIL_H__

#include <stdbool.h>
#include <sys/time.h>

void set_oom_score_adj_path(const char *path, int adj);
void set_oom_score_adj_pid(int pid, int adj);
void set_oom_score_adj_parent(int adj);
void set_oom_score_adj_self(int adj);

struct timeval timeval_sub(struct timeval t1, struct timeval t2);
double timeval_ms_diff(struct timeval t1, struct timeval t2);
void mprof_tick(struct timeval *tv);
double mprof_tock(struct timeval *tv);
bool buffers_are_equal(char *src, char *dst, size_t len, int *fail_index);

#define US_TO_MS(us) (us / 1000.0)
#define MS_TO_S(ms) (ms / 1000.0)
#define S_TO_MS(s) (s * 1000.0)
#define MS_TO_US(ms) (ms * 1000.0)
#define S_TO_US(s) (s * 1000 * 1000.0)
#define TV_TO_MS(tv) (S_TO_MS(tv.tv_sec) + US_TO_MS(tv.tv_usec))


#define SZ_1K				0x00000400
#define SZ_2K				0x00000800
#define SZ_4K				0x00001000
#define SZ_8K				0x00002000
#define SZ_16K				0x00004000
#define SZ_32K				0x00008000
#define SZ_64K				0x00010000
#define SZ_128K				0x00020000
#define SZ_256K				0x00040000
#define SZ_512K				0x00080000

#define SZ_1M				0x00100000
#define SZ_2M				0x00200000
#define SZ_4M				0x00400000
#define SZ_8M				0x00800000
#define SZ_16M				0x01000000
#define SZ_32M				0x02000000
#define SZ_64M				0x04000000
#define SZ_128M				0x08000000
#define SZ_256M				0x10000000
#define SZ_512M				0x20000000

#define KB_TO_BYTES(x)	(x << 10)
#define MB_TO_KB(x)	(x << 10)
#define BYTES_TO_KB(x)	(x >> 10)
#define KB_TO_MB(x)	(x >> 10)
#define BYTES_TO_MB(x)	KB_TO_MB(BYTES_TO_KB(x))
#define MB_TO_BYTES(x)	KB_TO_BYTES(MB_TO_KB(x))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define _MALLOC(ptr_type, ptr, size, file, line) do {			\
		ptr = (ptr_type) malloc(size);				\
		if (!ptr)						\
			err(1, "Couldn't get memory for malloc! %s:%d", file, line); \
	} while(0)

#define _REALLOC(ptr_type, ptr, size, file, line) do {			\
		ptr = (ptr_type) realloc(ptr, size);			\
		if (!ptr)						\
			err(1, "Couldn't get memory for realloc! %s:%d", file, line); \
	} while(0)

#define _STRTOL(var, word, base, file, line) do {			\
		char *endchar;						\
		var = strtol(word, &endchar, base);			\
		if (!endchar || endchar == word)			\
			errx(1, "Couldn't convert %s to a number. %s:%d", word, file, line); \
	} while(0)

#define MALLOC(ptr_type, ptr, size) _MALLOC(ptr_type, ptr, size, __FILE__, __LINE__)
#define REALLOC(ptr_type, ptr, size) _REALLOC(ptr_type, ptr, size, __FILE__, __LINE__)
#define STRTOL(var, word, base) _STRTOL(var, word, base, __FILE__, __LINE__)
/*
 * strncpy is considered "unsafe" and strlcpy doesn't exist on all
 * systems (notably glibc-based ones). Here's a strncpy that
 * guarantees null termination.
 */
#define STRNCPY_SAFE(dst, src, n) do {				\
		char *p;					\
		int l = MIN((int) n, (int) strlen(src));	\
		p = (char *) memcpy(dst, src, l);		\
		*(p + l) = '\0';				\
	} while (0)

#endif /* __MEMORY_PROF_UTIL_H__ */
