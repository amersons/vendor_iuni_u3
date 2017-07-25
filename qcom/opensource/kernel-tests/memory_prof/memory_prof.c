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
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <libgen.h>		/* for basename */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <getopt.h>
#include <linux/msm_ion.h>
#include "memory_prof.h"
#include "memory_prof_module.h"
#include "memory_prof_util.h"

static unsigned int sleepiness;

static void sleepy()
{
	usleep(sleepiness);
}

static void print_n_slow(char *st)
{
	puts(st);
	sleepy();
}

static void hr()
{
	puts("---------------");
}

/**
 * @ionfd [out] ion fd. On success, user must close.
 * @alloc_data [in/out] alloc data. On success, user must free.
 * returns 0 on success, 1 otherwise
 */
static int alloc_me_up_some_ion(int ionfd,
				struct ion_allocation_data *alloc_data)
{
	int rc = 0;

	rc = ioctl(ionfd, ION_IOC_ALLOC, alloc_data);
	if (rc)
		perror("couldn't do ion alloc");

	return rc;
}

/**
 * mmaps an Ion buffer to *buf.
 *
 * @ionfd - the ion fd
 * @handle - the handle to the Ion buffer to map
 * @len - the length of the buffer
 * @buf - [output] the userspace buffer where the ion buffer will be
 * mapped
 * @map_fd - [output] the fd corresponding to the ION_IOC_MAP call
 * will be saved in *map_fd. *You should close it* when you're done
 * with it (and have munmap'd) *buf.
 *
 * Returns 0 on success, 1 otherwise
 */
static int mmap_my_ion_buffer(int ionfd, struct ion_handle *handle, size_t len,
			char **buf, int *map_fd)
{
	struct ion_fd_data fd_data;
	int rc;

	fd_data.handle = handle;

	rc = ioctl(ionfd, ION_IOC_MAP, &fd_data);
	if (rc) {
		perror("couldn't do ION_IOC_MAP");
		return 1;
	}

	*map_fd = fd_data.fd;

	*buf = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED,
		*map_fd, 0);

	return 0;
}

/**
 * Allocates an ion buffer and mmaps it into *buf. See
 * alloc_me_up_some_ion and mmap_my_ion_buffer for an explanation of
 * the parameters here.
 */
static int alloc_and_map_some_ion(int ionfd,
				struct ion_allocation_data *alloc_data,
				char **buf, int *map_fd)
{
	int rc;

	rc = alloc_me_up_some_ion(ionfd, alloc_data);
	if (rc)
		return rc;
	rc = mmap_my_ion_buffer(ionfd, alloc_data->handle, alloc_data->len,
				buf, map_fd);
	if (rc) {
		rc |= ioctl(ionfd, ION_IOC_FREE, &alloc_data->handle);
	}
	return rc;
}

static int basic_ion_sanity_test(struct ion_allocation_data alloc_data,
				unsigned long size_mb)
{
	uint8_t *buf;
	int ionfd, rc = 0;
	unsigned long i, squelched = 0;
	bool integrity_good = true;
	struct ion_fd_data fd_data;
	struct ion_custom_data custom_data;
	struct ion_flush_data flush_data;

	ionfd = open(ION_DEV, O_RDONLY);
	if (ionfd < 0) {
		perror("couldn't open " ION_DEV);
		rc = ionfd;
		goto out;
	}

	rc = alloc_me_up_some_ion(ionfd, &alloc_data);
	if (rc)
		goto err0;

	fd_data.handle = alloc_data.handle;

	rc = ioctl(ionfd, ION_IOC_MAP, &fd_data);
	if (rc) {
		perror("couldn't do ION_IOC_MAP");
		goto err1;
	}

	buf = mmap(NULL, size_mb, PROT_READ | PROT_WRITE, MAP_SHARED,
		fd_data.fd, 0);

	if (buf == MAP_FAILED) {
		perror("couldn't do mmap");
		rc = (int) MAP_FAILED;
		goto err2;
	}

	memset(buf, 0xA5, size_mb);
	flush_data.handle = alloc_data.handle;
	flush_data.vaddr = buf;
	flush_data.length = size_mb;
	custom_data.cmd = ION_IOC_CLEAN_INV_CACHES;
	custom_data.arg = (unsigned long) &flush_data;
	if (ioctl(ionfd, ION_IOC_CUSTOM, &custom_data)) {
		perror("Couldn't flush caches");
		rc = 1;
		goto err3;
	} else {
		puts("flushed caches");
	}

	for (i = 0; i < size_mb; ++i) {
		if (buf[i] != 0xA5) {
			if (!integrity_good) {
				squelched++;
				continue;
			}
			printf("  Data integrity error at "
				"offset 0x%lx from 0x%p!!!!\n", i, buf);
			integrity_good = false;
		}
	}
	if (squelched)
		printf("  (squelched %ld additional integrity error%s)\n",
			squelched, squelched == 1 ? "" : "s");

	if (integrity_good)
		printf("  Buffer integrity check succeeded\n");

err3:
	if (munmap(buf, size_mb)) {
		rc = 1;
		perror("couldn't do munmap");
	}

err2:
	close(fd_data.fd);
err1:
	rc |= ioctl(ionfd, ION_IOC_FREE, &alloc_data.handle);
err0:
	close(ionfd);
out:
	return rc;
}

static void basic_sanity_tests(unsigned long size_mb)
{
	int lrc, rc = 0;

	struct ion_allocation_data iommu_alloc_data = {
		.align	   = SZ_1M,
		.len	   = SZ_1M,
		.heap_mask = ION_HEAP(ION_IOMMU_HEAP_ID),
		.flags	   = 0,
	};

	struct ion_allocation_data system_alloc_data = {
		.align	   = SZ_1M,
		.len	   = SZ_1M,
		.heap_mask = ION_HEAP(ION_SYSTEM_HEAP_ID),
		.flags	   = 0,
	};

	struct ion_allocation_data system_contig_alloc_data = {
		.align	   = SZ_1M,
		.len	   = SZ_1M,
		.heap_mask = ION_HEAP(ION_SYSTEM_CONTIG_HEAP_ID),
		.flags	   = 0,
	};

	puts("testing IOMMU without caching...");
	lrc = basic_ion_sanity_test(iommu_alloc_data, size_mb);
	puts(lrc ? "FAILED!" : "PASSED");
	hr();
	sleepy();
	rc |= lrc;

	puts("testing IOMMU with caching...");
	iommu_alloc_data.flags |= ION_FLAG_CACHED;
	lrc = basic_ion_sanity_test(iommu_alloc_data, size_mb);
	puts(lrc ? "FAILED!" : "PASSED");
	hr();
	sleepy();
	rc |= lrc;

	puts("testing system without caching (should fail)...");
	lrc = !basic_ion_sanity_test(system_alloc_data, size_mb);
	puts(lrc ? "FAILED! (failed to fail)" : "PASSED (successfully failed)");
	hr();
	sleepy();
	rc |= lrc;

	puts("testing system with caching...");
	system_alloc_data.flags |= ION_FLAG_CACHED;
	lrc = basic_ion_sanity_test(system_alloc_data, size_mb);
	puts(lrc ? "FAILED!" : "PASSED");
	hr();
	sleepy();
	rc |= lrc;

	puts("testing system contig without caching (should fail)...");
	lrc = !basic_ion_sanity_test(system_contig_alloc_data, size_mb);
	puts(lrc ? "FAILED! (failed to fail)" : "PASSED (successfully failed)");
	hr();
	sleepy();
	rc |= lrc;

	puts("testing system contig with caching...");
	system_contig_alloc_data.flags |= ION_FLAG_CACHED;
	lrc = basic_ion_sanity_test(system_contig_alloc_data, size_mb);
	puts(lrc ? "FAILED!" : "PASSED");
	hr();
	sleepy();
	rc |= lrc;

	if (rc)
		puts("BASIC SANITY TESTS FAILED!!!!!!!! WOOOWWWW!!!!!!");
	else
		puts("All basic sanity tests passed");

}

static int do_map_extra_test(void)
{
	int rc = 0;
	int ionfd, memory_prof_fd;
	struct memory_prof_map_extra_args args;
	size_t buffer_length = SZ_1M;
	struct ion_allocation_data alloc_data = {
		.align	   = SZ_1M,
		.len	   = buffer_length,
		.heap_mask = ION_HEAP(ION_IOMMU_HEAP_ID),
		.flags	   = 0,
	};
	struct ion_fd_data fd_data;

	ionfd = open(ION_DEV, O_RDONLY);
	if (ionfd < 0) {
		perror("couldn't open " ION_DEV);
		goto out;
	}

	memory_prof_fd = open(MEMORY_PROF_DEV, 0);
	if (memory_prof_fd < 0) {
		perror("couldn't open " MEMORY_PROF_DEV);
		rc = memory_prof_fd;
		goto close_ion_fd;
	}

	rc = ioctl(memory_prof_fd, MEMORY_PROF_IOC_CLIENT_CREATE);
	if (rc) {
		perror("couldn't do MEMORY_PROF_IOC_CLIENT_CREATE");
		goto close_memory_prof_fd;
	}

	rc = alloc_me_up_some_ion(ionfd, &alloc_data);
	if (rc)
		goto destroy_ion_client;

	fd_data.handle = alloc_data.handle;

	rc = ioctl(ionfd, ION_IOC_SHARE, &fd_data);
	if (rc) {
		perror("Couldn't do ION_IOC_SHARE");
		goto ion_free_and_close;
	}

	args.ionfd = fd_data.fd;
	args.iommu_map_len = buffer_length * 2;

	print_n_slow("Doing MEMORY_PROF_IOC_TEST_MAP_EXTRA");
	rc = ioctl(memory_prof_fd, MEMORY_PROF_IOC_TEST_MAP_EXTRA, &args);
	if (rc) {
		perror("couldn't do MEMORY_PROF_IOC_TEST_MAP_EXTRA");
		goto ion_free_and_close;
	}

ion_free_and_close:
	rc |= ioctl(ionfd, ION_IOC_FREE, &alloc_data.handle);
	close(ionfd);
destroy_ion_client:
	rc |= ioctl(memory_prof_fd, MEMORY_PROF_IOC_CLIENT_DESTROY);
close_memory_prof_fd:
	close(memory_prof_fd);
close_ion_fd:
	close(ionfd);
out:
	return rc;
}

/**
 * Free memory in alloc_list
 */
void free_mem_list(char **alloc_list)
{
	int i = 0;
	for (i = 0; i < MAX_PRE_ALLOC_SIZE; i++) {
		if (alloc_list[i] != NULL) {
			free(alloc_list[i]);
			alloc_list[i] = NULL;
		}
	}
}


/**
 * Allocate sizemb in alloc_list
 */
int alloc_mem_list(char **alloc_list, int sizemb)
{
	int i = 0;
	int alloc_size = 1*1024*1024 * sizeof(char);

	if (sizemb > MAX_PRE_ALLOC_SIZE) {
		return -1;
	}

	// Break allocation into 1 MB pieces to ensure
	// we easily find enough virtually contigous memory
	for (i = 0; i < sizemb; i++)
	{
		alloc_list[i] =(char *) malloc(alloc_size);
		if (alloc_list[i] == NULL)
		{
		    perror("couldn't allocate 1MB");
		    free_mem_list(alloc_list);
		    return -1;
		}

		// Memory must be accessed for it to be page backed.
		// We may want to randomize the data in the future
		// to prevent features such as KSM returning memory
		// to the system.
		memset(alloc_list[i], 1, alloc_size);
	}
	return 0;
}


/**
 * Returns the total time taken for ION_IOC_ALLOC
 *
 * @heap_mask - passed to ION_IOC_ALLOC
 * @flags - passed to ION_IOC_ALLOC
 * @size - passed to ION_IOC_ALLOC
 * @pre_alloc_size - size of memory to malloc before doing the ion alloc
 * @alloc_ms - [output] time taken (in MS) to complete the ION_IOC_ALLOC
 * @map_ms - [output] time taken (in MS) to complete the ION_IOC_MAP + mmap
 * @memset_ms - [output] time taken (in MS) to memset the Ion buffer
 * @free_ms - [output] time taken (in MS) to complete the ION_IOC_FREE
 *
 * Returns 0 on success, 1 on failure.
 */
static int profile_alloc_for_heap(unsigned int heap_mask,
				unsigned int flags, unsigned int size,
				int pre_alloc_size,
				double *alloc_ms, double *map_ms,
				double *memset_ms, double *free_ms)
{
	int ionfd, rc = 0, rc2;
	uint8_t *buf = MAP_FAILED;
	struct ion_fd_data fd_data;
	struct timeval tv_before, tv_after, tv_result;
	struct ion_allocation_data alloc_data = {
		.align	   = SZ_4K,
		.len	   = size,
		.heap_mask = heap_mask,
		.flags	   = flags,
	};
	char *pre_alloc_list[MAX_PRE_ALLOC_SIZE];
	memset(pre_alloc_list, 0, MAX_PRE_ALLOC_SIZE * sizeof(char *));

	*alloc_ms = 0;
	*free_ms = 0;
	if (map_ms) *map_ms = 0;
	if (memset_ms) *memset_ms = 0;

	if (alloc_mem_list(pre_alloc_list, pre_alloc_size) < 0) {
		rc = 1;
		perror("couldn't create pre-allocated buffer");
		goto out;
	}

	ionfd = open(ION_DEV, O_RDONLY);
	if (ionfd < 0) {
		rc = 1;
		perror("couldn't open " ION_DEV);
		goto free_mem_list;
	}

	rc = gettimeofday(&tv_before, NULL);
	if (rc) {
		perror("couldn't get time of day");
		goto close_ion;
	}

	rc2 = ioctl(ionfd, ION_IOC_ALLOC, &alloc_data);
	rc = gettimeofday(&tv_after, NULL);
	if (rc2) {
		rc = rc2;
		goto close_ion;
	}
	if (rc) {
		perror("couldn't get time of day");
		goto free;
	}

	*alloc_ms = timeval_ms_diff(tv_after, tv_before);

	if (map_ms) {
		rc = gettimeofday(&tv_before, NULL);
		if (rc) {
			perror("couldn't get time of day");
			goto free;
		}

		fd_data.handle = alloc_data.handle;

		rc = ioctl(ionfd, ION_IOC_MAP, &fd_data);
		if (rc) {
			perror("couldn't do ION_IOC_MAP");
			goto free;
		}

		buf = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
			fd_data.fd, 0);

		rc2 = gettimeofday(&tv_after, NULL);

		if (buf == MAP_FAILED) {
			perror("couldn't do mmap");
			goto fd_data_close;
		}

		if (rc2) {
			perror("couldn't get time of day");
			goto munmap;
		}

		*map_ms = timeval_ms_diff(tv_after, tv_before);
	}

	if (memset_ms && buf != MAP_FAILED) {
		rc = gettimeofday(&tv_before, NULL);
		if (rc) {
			perror("couldn't get time of day");
			goto munmap;
		}

		memset(buf, 0xA5, size);
		rc2 = gettimeofday(&tv_after, NULL);
		if (rc2) {
			perror("couldn't get time of day");
			goto munmap;
		}
		*memset_ms = timeval_ms_diff(tv_after, tv_before);
	}

munmap:
	if (buf != MAP_FAILED)
		if (munmap(buf, size))
			perror("couldn't do munmap");

fd_data_close:
	close(fd_data.fd);

	/*
	 * Okay, things are about to get messy. We're profiling our
	 * cleanup, but we might fail some stuff and need to
	 * cleanup... during cleanup.
	 */

	rc = gettimeofday(&tv_before, NULL);
	if (rc)
		perror("couldn't get time of day");
free:
	ioctl(ionfd, ION_IOC_FREE, &alloc_data.handle);
	rc2 = gettimeofday(&tv_after, NULL);
	if (!rc) {
		if (rc2) {
			perror("couldn't get time of day");
			goto close_ion;
		}
		*free_ms = timeval_ms_diff(tv_after, tv_before);
	}
close_ion:
	close(ionfd);
free_mem_list:
	free_mem_list(pre_alloc_list);
out:
	return rc;
}

static void map_extra_test(void)
{
	int rc = 0;
	puts("testing msm_iommu_map_extra...");
	rc = do_map_extra_test();
	if (rc) puts("FAILED!");
	hr();
}

static void profile_kernel_alloc(void)
{
	int rc, memory_prof_fd;

	memory_prof_fd = open(MEMORY_PROF_DEV, 0);
	if (memory_prof_fd < 0) {
		perror("couldn't open " MEMORY_PROF_DEV);
		return;
	}
	rc = ioctl(memory_prof_fd, MEMORY_PROF_IOC_TEST_KERNEL_ALLOCS);
	if (rc)
		perror("couldn't do MEMORY_PROF_IOC_TEST_KERNEL_ALLOCS");
	close(memory_prof_fd);
}

static void print_stats_results(const char *name, const char *flags_label,
				const char *size_string,
				double stats[], int reps)
{
	int i;
	struct timeval tv;
	unsigned long long time_ms;
	double sum = 0, sum_of_squares = 0, average, std_dev;

	for (i = 0; i < reps; ++i) {
		sum += stats[i];
	}
	average = sum / reps;

	for (i = 0; i < reps; ++i) {
		sum_of_squares += pow(stats[i] - average, 2);
	}
	std_dev = sqrt(sum_of_squares / reps);

	gettimeofday(&tv, NULL);
	time_ms = TV_TO_MS(tv);
	printf("%s %llu %s %s %s average: %.2f std_dev: %.2f reps: %d\n",
		ST_PREFIX_DATA_ROW, time_ms,
		name, flags_label, size_string, average, std_dev, reps);
}

static void print_a_bunch_of_stats_results(const char *name,
					const char *flags_label,
					const char *size_string,
					double alloc_stats[],
					double map_stats[],
					double memset_stats[],
					double free_stats[],
					int reps)
{
	char sbuf[70];
	snprintf(sbuf, 70, "ION_IOC_ALLOC %s", name);
	print_stats_results(sbuf, flags_label, size_string, alloc_stats, reps);
	if (map_stats) {
		snprintf(sbuf, 70, "mmap %s", name);
		print_stats_results(sbuf, flags_label, size_string,
				map_stats, reps);
	}
	if (memset_stats) {
		snprintf(sbuf, 70, "memset %s", name);
		print_stats_results(sbuf, flags_label, size_string,
				memset_stats, reps);
	}
	snprintf(sbuf, 70, "ION_IOC_FREE %s", name);
	print_stats_results(sbuf, flags_label, size_string, free_stats, reps);
}

static void heap_profiling(int pre_alloc_size, const int nreps,
			const char *alloc_profile_path)
{
	int max_reps = 0;
	double *alloc_stats;
	double *map_stats;
	double *memset_stats;
	double *free_stats;
	bool using_default = false;
	struct alloc_profile_entry *pp;
	struct alloc_profile_entry *alloc_profile;
	if (alloc_profile_path) {
		alloc_profile = get_alloc_profile(alloc_profile_path);
	} else {
		alloc_profile = get_default_alloc_profile();
		using_default = true;
	}

	puts("All times are in milliseconds unless otherwise indicated");
	printf(ST_PREFIX_PREALLOC_SIZE " pre-alloc size (MB): %d\n",
		pre_alloc_size);
	/* printf(ST_PREFIX_NUM_REPS "num reps: %d\n", nreps); */

	/*
	 * Rather than malloc'ing and free'ing just enough space on
	 * each iteration, let's get the max needed up front. This
	 * will help keep things a little more equal during the actual
	 * timings.
	 */
	for (pp = alloc_profile; pp->op != OP_NONE; ++pp)
		if (pp->op == OP_ALLOC)
			max_reps = MAX(max_reps, pp->u.alloc_op.reps);
	if (max_reps == 0)
		errx(1, "No data lines found in profile: %s",
			alloc_profile_path);

	MALLOC(double *, alloc_stats, sizeof(double) * max_reps);
	MALLOC(double *, map_stats, sizeof(double) * max_reps);
	MALLOC(double *, memset_stats, sizeof(double) * max_reps);
	MALLOC(double *, free_stats, sizeof(double) * max_reps);

	for (pp = alloc_profile; pp->op != OP_NONE; ++pp) {
		int i;
		int reps;

		switch (pp->op) {
		case OP_ALLOC:
		{
			if (using_default && nreps > 0)
				reps = nreps;
			else
				reps = pp->u.alloc_op.reps;
			for (i = 0; i < reps; ++i) {
				if (profile_alloc_for_heap(
					pp->u.alloc_op.heap_id,
					pp->u.alloc_op.flags,
					pp->u.alloc_op.size,
					pre_alloc_size,
					&alloc_stats[i],
					pp->u.alloc_op.profile_mmap
					? &map_stats[i] : NULL,
					pp->u.alloc_op.profile_memset
					? &memset_stats[i] : NULL,
					&free_stats[i])
					&& !pp->u.alloc_op.quiet)
					warn("Couldn't allocate %s from %s",
						pp->u.alloc_op.size_string,
						pp->u.alloc_op.heap_id_string);
			}
			print_a_bunch_of_stats_results(
				pp->u.alloc_op.heap_id_string,
				pp->u.alloc_op.flags_string,
				pp->u.alloc_op.size_string,
				alloc_stats,
				pp->u.alloc_op.profile_mmap
				? map_stats : NULL,
				pp->u.alloc_op.profile_memset
				? memset_stats : NULL,
				free_stats,
				reps);
			putchar('\n');
			fflush(stdout);
			break;
		}
		case OP_SLEEP:
		{
			usleep(pp->u.sleep_op.time_us);
			break;
		}
		default:
			errx(1, "Unknown op: %d", pp->op);
		}
	}

	free(alloc_stats);
	free(map_stats);
	free(memset_stats);
	free(free_stats);
}

static void oom_test(void)
{
	int rc, ionfd, cnt = 0;
	struct ion_allocation_data alloc_data = {
		.len	   = SZ_8M,
		.heap_mask = ION_HEAP(ION_IOMMU_HEAP_ID),
		.flags	   = 0,
	};

	LIST_HEAD(handle_list, ion_handle_node) head;
	struct handle_list *headp;
	struct ion_handle_node {
		struct ion_handle *handle;
		LIST_ENTRY(ion_handle_node) nodes;
	} *np;
	LIST_INIT(&head);

	ionfd = open(ION_DEV, O_RDONLY);
	if (ionfd < 0) {
		perror("couldn't open " ION_DEV);
		return;
	}


	for (;; cnt++) {
		rc = ioctl(ionfd, ION_IOC_ALLOC, &alloc_data);
		if (rc) {
			/* game over! */
			break;
		} else {
			np = malloc(sizeof(struct ion_handle_node));
			np->handle = alloc_data.handle;
			LIST_INSERT_HEAD(&head, np, nodes);
		}
		printf("Allocated %d MB so far...\n", cnt * 8);
	}

	printf("Did %d 8M allocations before dying\n", cnt);

	while (head.lh_first != NULL) {
		np = head.lh_first;
		ioctl(ionfd, ION_IOC_FREE, np->handle);
		LIST_REMOVE(head.lh_first, nodes);
		free(np);
	}
}

static void leak_test(void)
{
	int ionfd;
	struct ion_fd_data fd_data;
	struct ion_allocation_data alloc_data = {
		.len	   = SZ_4K,
		.heap_mask = ION_HEAP(ION_SYSTEM_HEAP_ID),
		.flags	   = 0,
	};

	puts("About to leak a handle...");
	fflush(stdout);

	ionfd = open(ION_DEV, O_RDONLY);
	if (ionfd < 0) {
		perror("couldn't open " ION_DEV);
		return;
	}

	alloc_me_up_some_ion(ionfd, &alloc_data);

	fd_data.handle = alloc_data.handle;

	if (ioctl(ionfd, ION_IOC_MAP, &fd_data))
		perror("Couldn't ION_IOC_MAP the buffer");

	close(ionfd);

	puts("closed ionfd w/o free'ing handle.");
	puts("If you have CONFIG_ION_LEAK_CHECK turned on in");
	puts("your kernel and have already done:");
	puts("echo 1 > /debug/ion/check_leaks_on_destroy");
	puts("then you should have seen a warning on your");
	puts("console.");
	puts("We will now sleep for 5 seconds for you to check");
	puts("<debugfs>/ion/check_leaked_fds");
	sleep(5);
}

/**
 * Allocate two ion buffers, mmap them, memset each of them to
 * something different, memcpy from one to the other, then check that
 * they are equal.

 * We time the memcpy operation and return the number of milliseconds
 * it took in *time_elapsed_memcpy_ms. When time_elapsed_flush_ms is
 * not NULL, we flush the dst buffer and return the number of
 * milliseconds it took to do so in *time_elapsed_flush_ms.
 *
 * @src_alloc_data - source allocation data
 * @dst_alloc_data - source allocation data
 * @time_elapsed_memcpy_ms - [output] time taken (in MS) to do the memcpy
 * @time_elapsed_flush_ms - [output] time taken (in MS) to do the
 * cache flush. Pass NULL if you don't want to do the flushing.
 *
 */
static int do_ion_memcpy(struct ion_allocation_data src_alloc_data,
			struct ion_allocation_data dst_alloc_data,
			double *time_elapsed_memcpy_ms,
			double *time_elapsed_flush_ms)
{
	int ionfd, i, src_fd, dst_fd, fail_index = 0, rc = 0;
	char *src, *dst;
	struct timeval tv;
	size_t len = src_alloc_data.len;

	ionfd = open(ION_DEV, O_RDONLY);
	if (ionfd < 0) {
		perror("couldn't open " ION_DEV);
		rc = 1;
		goto out;
	}

	if (alloc_and_map_some_ion(ionfd, &src_alloc_data, &src, &src_fd)) {
		perror("Couldn't alloc and map src buffer");
		rc = 1;
		goto close_ionfd;
	}
	if (alloc_and_map_some_ion(ionfd, &dst_alloc_data, &dst, &dst_fd)) {
		perror("Couldn't alloc and map dst buffer");
		rc = 1;
		goto free_src;
	}

	memset(src, 0x5a, len);
	memset(dst, 0xa5, len);
	mprof_tick(&tv);
	memcpy(dst, src, len);
	*time_elapsed_memcpy_ms = mprof_tock(&tv);

	if (time_elapsed_flush_ms) {
		struct ion_flush_data flush_data;
		struct ion_custom_data custom_data;
		flush_data.handle = dst_alloc_data.handle;
		flush_data.vaddr = dst;
		flush_data.length = len;
		custom_data.cmd = ION_IOC_CLEAN_CACHES;
		custom_data.arg = (unsigned long) &flush_data;
		mprof_tick(&tv);
		if (ioctl(ionfd, ION_IOC_CUSTOM, &custom_data)) {
			perror("Couldn't flush caches");
			rc = 1;
		}
		*time_elapsed_flush_ms = mprof_tock(&tv);
	}

	if (!buffers_are_equal(src, dst, len, &fail_index)) {
		printf("WARNING: buffer integrity check failed\n"
			"dst[%d]=0x%x, src[%d]=0x%x\n",
			fail_index, dst[fail_index],
			fail_index, src[fail_index]);
		rc = 1;
	}

munmap_and_close:
	munmap(src, len);
	munmap(dst, len);
	close(src_fd);
	close(dst_fd);
free_dst:
	ioctl(ionfd, ION_IOC_FREE, &dst_alloc_data.handle);
free_src:
	ioctl(ionfd, ION_IOC_FREE, &src_alloc_data.handle);
close_ionfd:
	close(ionfd);
out:
	return rc;
}

/**
 * Profiles the time it takes to copy between various types of Ion
 * buffers. Namely, copies between every permutation of
 * cached/uncached buffer (4 permutations total). Also profiles the
 * amount of time it takes to flush the destination buffer on a
 * cached->cached copy.
 */
static void ion_memcpy_test(void)
{
	struct ion_allocation_data src_alloc_data, dst_alloc_data;
	struct timeval tv;
	double time_elapsed_memcpy_ms = 0,
		time_elapsed_flush_ms = 0;

	src_alloc_data.len = SZ_4M;
	src_alloc_data.heap_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);
	dst_alloc_data.len = SZ_4M;
	dst_alloc_data.heap_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);

	src_alloc_data.flags = 0;
	dst_alloc_data.flags = 0;
	if (do_ion_memcpy(src_alloc_data, dst_alloc_data,
				&time_elapsed_memcpy_ms, NULL)) {
		printf("Uncached -> Uncached: FAIL\n");
	} else {
		printf("Uncached -> Uncached: %.3fms\n", time_elapsed_memcpy_ms);
	}

	src_alloc_data.flags = 0;
	dst_alloc_data.flags = ION_FLAG_CACHED;
	if (do_ion_memcpy(src_alloc_data, dst_alloc_data,
				&time_elapsed_memcpy_ms, NULL)) {
		printf("Uncached -> Cached: FAIL\n");
	} else {
		printf("Uncached -> Cached: %.3fms\n", time_elapsed_memcpy_ms);
	}

	src_alloc_data.flags = ION_FLAG_CACHED;
	dst_alloc_data.flags = 0;
	if (do_ion_memcpy(src_alloc_data, dst_alloc_data,
				&time_elapsed_memcpy_ms, NULL)) {
		printf("Cached -> Uncached: FAIL\n");
	} else {
		printf("Cached -> Uncached: %.3fms\n", time_elapsed_memcpy_ms);
	}

	src_alloc_data.flags = ION_FLAG_CACHED;
	dst_alloc_data.flags = ION_FLAG_CACHED;
	if (do_ion_memcpy(src_alloc_data, dst_alloc_data,
				&time_elapsed_memcpy_ms,
				&time_elapsed_flush_ms)) {
		printf("Cached -> Cached: FAIL\n");
	} else {
		printf("Cached -> Cached: %.3fms (cache flush took %.3fms)\n",
			time_elapsed_memcpy_ms, time_elapsed_flush_ms);
	}
}

/**
 * Memory throughput test. Print some stats.
 */
static void mmap_memcpy_test(void)
{
	char *chunk, *src, *dst;
	struct timeval tv;
	double *data_rates;
	double sum = 0, sum_of_squares = 0, average, std_dev, dmin, dmax;
	int iters = 1000, memcpy_iters = 100;
	int i, j;
	const size_t chunk_len = SZ_1M;

	MALLOC(double *, data_rates, iters * sizeof(double));

	chunk = mmap(NULL, chunk_len,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		-1, 0);
	if (chunk == MAP_FAILED) {
		perror("Couldn't allocate 1MB buffer with mmap\n");
		goto free_data_rates;
	}

	/*
	 * Some systems appear to do this trick where they mprotect
	 * the first page of a large allocation with
	 * PROT_NONE. Possibly to protect against someone else
	 * overflowing into their buffer? We do it here just to
	 * emulate the "real-world" a little closer.
	 */
	if (mprotect(chunk, SZ_4K, PROT_NONE)) {
		perror("Couldn't mprotect the first page of the 1MB buffer\n");
		goto munmap_chunk;
	}

	src = chunk + SZ_4K;
	dst = chunk + SZ_512K;
	memset(src, 0x5a, SZ_64K);
	memset(dst, 0xa5, SZ_64K);

	for (i = 0; i < iters; ++i) {
		float elapsed_ms;
		mprof_tick(&tv);
		for (j = 0; j < memcpy_iters; ++j)
			memcpy(dst, src, SZ_64K);
		elapsed_ms = mprof_tock(&tv);
		/* units: MB/s */
		data_rates[i] = ((SZ_64K * memcpy_iters) / SZ_1M)
			/ (elapsed_ms / 1000);
	}

	dmin = dmax = data_rates[0];
	for (i = 0; i < iters; ++i) {
		sum += data_rates[i];
		dmin = MIN(dmin, data_rates[i]);
		dmax = MAX(dmax, data_rates[i]);
	}
	average = sum / iters;

	for (i = 0; i < iters; ++i)
		sum_of_squares += pow(data_rates[i] - average, 2);
	std_dev = sqrt(sum_of_squares / iters);

	printf("average: %.3f MB/s, min: %.3f MB/s, max: %.3f MB/s, std_dev: %.3f MB/s\n",
		average, dmin, dmax, std_dev);

munmap_chunk:
	munmap(chunk, chunk_len);
free_data_rates:
	free(data_rates);
}

static int file_exists(const char const *fname)
{
	struct stat tmp;
	return stat(fname, &tmp) == 0;
}

#define USAGE_STRING							\
	"Usage: %s [options]\n"						\
	"\n"								\
	"Supported options:\n"						\
	"\n"								\
	"  -h         Print this message and exit\n"			\
	"  -a         Do the adversarial test (same as -l)\n"		\
	"  -b         Do basic sanity tests\n"				\
	"  -z MB      Size (in MB) of buffer for basic sanity tests (default=1)\n" \
	"  -e[REPS]   Do Ion heap profiling. Optionally specify number of reps\n" \
	"             E.g.: -e10 would do 10 reps (default=100). The number\n" \
	"             of reps is only used when the default allocation profile\n" \
	"             is used (i.e. when -i is not given)\n"		\
	"  -i file    Input `alloc profile' for heap profiling (-e)\n"	\
	"             (Runs a general default profile if omitted)\n"	\
	"  -k         Do kernel alloc profiling (requires kernel module)\n" \
	"  -l         Do leak test (leak an ion handle)\n"		\
	"  -m         Do map extra test (requires kernel module)\n"	\
	"  -n         Do the nominal test (same as -b)\n"		\
	"  -o         Do OOM test (alloc from Ion Iommu heap until OOM)\n" \
	"  -p MS      Sleep for MS milliseconds between stuff (for debugging)\n" \
	"  -r         Do the repeatability test\n"			\
	"  -s         Do the stress test (same as -e)\n"		\
	"  -t MB      Size (in MB) of temp buffer pre-allocated before Ion allocations (default 0 MB)\n" \
	"  --ion-memcpy-test\n" \
	"             Does some memcpy's between various types of Ion buffers\n" \
	"  --mmap-memcpy-test\n" \
	"             Does some memcpy's between some large buffers allocated\n" \
	"             by mmap\n"

static void usage(char *progname)
{
	printf(USAGE_STRING, progname);
}

static struct option memory_prof_options[] = {
	{"ion-memcpy-test", no_argument, 0, 0},
	{"mmap-memcpy-test", no_argument, 0, 0},
	{0, 0, 0, 0}
};

int main(int argc, char *argv[])
{
	int rc = 0, i, opt;
	unsigned long basic_sanity_size_mb = SZ_1M;
	bool do_basic_sanity_tests = false;
	bool do_heap_profiling = false;
	bool do_kernel_alloc_profiling = false;
	bool do_map_extra_test = false;
	bool do_oom_test = false;
	bool do_leak_test = false;
	bool do_ion_memcpy_test = false;
	bool do_mmap_memcpy_test = false;
	const char *alloc_profile = NULL;
	int num_reps = 1;
	int num_heap_prof_reps = -1;
	int ion_pre_alloc_size = ION_PRE_ALLOC_SIZE_DEFAULT;
	int option_index = 0;

	while (-1 != (opt = getopt_long(
				argc,
				argv,
				"abe::hi:klmnop:rs::t:z:",
				memory_prof_options,
				&option_index))) {
		switch (opt) {
		case 0:
			if (strcmp("ion-memcpy-test",
					memory_prof_options[option_index].name)
				== 0) {
				do_ion_memcpy_test = true;
				break;
			}
			if (strcmp("mmap-memcpy-test",
					memory_prof_options[option_index].name)
				== 0) {
				do_mmap_memcpy_test = true;
				break;
			}
			printf("Unhandled longopt: %s\n",
				memory_prof_options[option_index].name);
			break;
		case 't':
			ion_pre_alloc_size = atoi(optarg);
			break;
		case 'n':
		case 'b':
			do_basic_sanity_tests = true;
			break;
		case 's':
		case 'e':
			if (optarg)
				num_heap_prof_reps = atoi(optarg);
			do_heap_profiling = true;
			break;
		case 'i':
			alloc_profile = optarg;
			if (!file_exists(alloc_profile))
				err(1, "Can't read alloc profile file: %s\n",
					alloc_profile);
			break;
		case 'k':
			do_kernel_alloc_profiling = true;
			break;
		case 'a':
		case 'l':
			do_leak_test = true;
			break;
		case 'm':
			do_map_extra_test = true;
			break;
		case 'o':
			do_oom_test = true;
			break;
		case 'r':
			num_reps = NUM_REPS_FOR_REPEATABILITY;
			break;
		case 'p':
			/* ms to us */
			sleepiness = atoi(optarg) * 1000;
			break;
		case 'z':
			basic_sanity_size_mb = atoi(optarg) * SZ_1M;
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

	if (do_basic_sanity_tests)
		for (i = 0; i < num_reps; ++i)
			basic_sanity_tests(basic_sanity_size_mb);
	if (do_map_extra_test)
		for (i = 0; i < num_reps; ++i)
			map_extra_test();
	if (do_heap_profiling)
		for (i = 0; i < num_reps; ++i)
			heap_profiling(ion_pre_alloc_size,
				num_heap_prof_reps, alloc_profile);
	if (do_kernel_alloc_profiling)
		for (i = 0; i < num_reps; ++i)
			profile_kernel_alloc();
	if (do_oom_test)
		for (i = 0; i < num_reps; ++i)
			oom_test();
	if (do_leak_test)
		for (i = 0; i < num_reps; ++i)
			leak_test();

	if (do_ion_memcpy_test)
		ion_memcpy_test();

	if (do_mmap_memcpy_test)
		mmap_memcpy_test();

	return rc;
}
