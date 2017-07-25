/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

/* MSM ION content protection tests. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <sys/types.h>
#include <ctype.h>
#include <linux/input.h>
#include <linux/msm_ion.h>
#include <sys/mman.h>
#include "ion_test_plan.h"
#include "ion_test_utils.h"

#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof((arr)[0]))

static struct ion_test_data mm_heap_test = {
	.align = 0x100000,
	.size = 0x100000,
	.heap_mask = ION_HEAP(ION_CP_MM_HEAP_ID),
	.flags = ION_FLAG_SECURE,
	.heap_type_req = CP,
};
static struct ion_test_data adv_mm_heap_test = {
	.align = 0x100000,
	.size = 0x100000,
	.heap_mask = ION_HEAP(ION_CP_MM_HEAP_ID),
	.heap_type_req = CP,
	.flags = 0,
};

static struct ion_test_data *mm_heap_data_settings[] = {
	[NOMINAL_TEST] = &mm_heap_test,
	[ADV_TEST] = &adv_mm_heap_test,
};

static int test_sec_alloc(const char *ion_dev, const char *msm_ion_dev,
				struct ion_test_plan *ion_tp, int test_type,
				int *test_skipped)
{
	int ion_fd, ion_kernel_fd, map_fd, rc;
	unsigned long addr;
	struct ion_allocation_data alloc_data, sec_alloc_data;
	struct ion_fd_data fd_data;
	struct ion_test_data ktest_data;
	struct ion_test_data *test_data;
	struct ion_test_data **test_data_table =
		(struct ion_test_data **)ion_tp->test_plan_data;

	if (test_type == NOMINAL_TEST)
		test_data = test_data_table[NOMINAL_TEST];
	else
		test_data = test_data_table[ADV_TEST];

	*test_skipped = !test_data->valid;
	if (!test_data->valid) {
		rc = 0;
		debug(INFO, "%s was skipped\n",__func__);
		goto out;
	}

	ion_fd = open(ion_dev, O_RDONLY);
	if (ion_fd < 0) {
		debug(INFO, "Failed to open ion device\n");
		perror("msm ion");
		return ion_fd;
	}
	ion_kernel_fd = open(msm_ion_dev, O_RDONLY);
	if (ion_kernel_fd < 0) {
		debug(INFO, "Failed to open msm ion test device\n");
		perror("msm ion");
		close(ion_fd);
		return ion_kernel_fd;
	}
	alloc_data.len = test_data->size;
	alloc_data.align = test_data->align;
	alloc_data.heap_mask = test_data->heap_mask;
	alloc_data.flags = ION_SECURE;

	rc = ioctl(ion_fd, ION_IOC_ALLOC, &alloc_data);
	if (rc) {
		debug(ERR, "alloc buf failed\n");
		goto cp_alloc_err;
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_SEC, NULL);
	if (rc) {
		debug(INFO, "unable to secure heap\n");
		goto cp_alloc_sec_err;
	}
	sec_alloc_data.len = test_data->size;
	sec_alloc_data.heap_mask = test_data->heap_mask;
	sec_alloc_data.flags = test_data->flags;
	sec_alloc_data.align = test_data->align;
	rc = ioctl(ion_fd, ION_IOC_ALLOC, &sec_alloc_data);
	if (rc < 0 && test_type == NOMINAL_TEST) {
		debug(ERR, "Nominal cp alloc buf failed\n");
		goto cp_alloc_sec_err;
	} else if (rc < 0 && test_type == ADV_TEST) {
		rc = 0;
		goto cp_alloc_sec_err;
	} else if (rc == 0 && test_type == ADV_TEST) {
		debug(INFO, "erroneous alloc request succeeded\n");
		rc = -EIO;
		ioctl(ion_fd, ION_IOC_FREE, &sec_alloc_data.handle);
		goto cp_alloc_sec_err;
	} else {
		ioctl(ion_fd, ION_IOC_FREE, &sec_alloc_data.handle);
	}
	ioctl(ion_kernel_fd, IOC_ION_UNSEC, NULL);
	if (test_type == ADV_TEST) {
		fd_data.handle = alloc_data.handle;
		rc = ioctl(ion_fd, ION_IOC_MAP, &fd_data);
		if (rc < 0) {
			debug(INFO, "unable to ion map buffer\n");
			goto cp_alloc_sec_err;
		}
		map_fd = fd_data.fd;
		addr = (unsigned long)mmap(NULL, alloc_data.len,
					PROT_READ | PROT_WRITE,
					MAP_SHARED , map_fd, 0);
		sec_alloc_data.flags |= ION_FLAG_SECURE;
		rc = ioctl(ion_fd, ION_IOC_ALLOC, &sec_alloc_data);
		if (rc == 0) {
			rc = -EIO;
			debug(INFO, "Sec alloc succeded with umapped buf\n");
			ioctl(ion_fd, ION_IOC_FREE, &sec_alloc_data.handle);
			close(fd_data.fd);
			goto cp_alloc_sec_err;
		}
		close(fd_data.fd);
		rc = ioctl(ion_kernel_fd, IOC_ION_KCLIENT_CREATE, NULL);
		if (rc) {
			debug(INFO, "failed to create kernel client\n");
			goto cp_alloc_sec_err;
		}
		rc = ioctl(ion_fd, ION_IOC_SHARE, &fd_data);
		if (rc < 0) {
			debug(INFO, "unable to share ion buffer\n");
			goto cp_alloc_sec_err;
		}
		ktest_data.align = test_data->align;
		ktest_data.size = test_data->size;
		ktest_data.heap_mask = test_data->heap_mask;
		ktest_data.flags = test_data->flags;
		ktest_data.shared_fd = fd_data.fd;
		rc = ioctl(ion_kernel_fd, IOC_ION_UIMPORT, &ktest_data);
		if (rc) {
			debug(INFO, "unable to import ubuf to kernel space\n");
			ioctl(ion_kernel_fd, IOC_ION_KCLIENT_DESTROY, NULL);
			goto cp_alloc_sec_err;
		}
		rc = ioctl(ion_kernel_fd, IOC_ION_KMAP, NULL);
		if (rc) {
			debug(INFO, "unable to map ubuf to kernel space\n");
			ioctl(ion_kernel_fd, IOC_ION_KFREE, NULL);
			ioctl(ion_kernel_fd, IOC_ION_KCLIENT_DESTROY, NULL);
			goto cp_alloc_sec_err;
		}
		rc = ioctl(ion_fd, ION_IOC_ALLOC, &sec_alloc_data);
		if (rc == 0) {
			rc = -EIO;
			ioctl(ion_fd, ION_IOC_FREE, &sec_alloc_data.handle);
			debug(INFO, "Sec alloc succeded with kmapped buf\n");
		}
		ioctl(ion_kernel_fd, IOC_ION_KUMAP, NULL);
		ioctl(ion_kernel_fd, IOC_ION_KFREE, NULL);
		ioctl(ion_kernel_fd, IOC_ION_KCLIENT_DESTROY, NULL);
		close(fd_data.fd);
	}
cp_alloc_sec_err:
	ioctl(ion_fd, ION_IOC_FREE, &alloc_data.handle);
cp_alloc_err:
	close(ion_kernel_fd);
	close(ion_fd);
out:
	return rc;
}

static struct ion_test_plan sec_alloc_test = {
	.name = "CP ion alloc buf",
	.test_plan_data = &mm_heap_data_settings,
	.test_plan_data_len = 3,
	.test_type_flags = NOMINAL_TEST | ADV_TEST,
	.test_fn = test_sec_alloc,
};

static int test_sec_map(const char *ion_dev, const char *msm_ion_dev,
			struct ion_test_plan *ion_tp, int test_type,
			int *test_skipped)
{
	int ion_fd, rc, ion_kernel_fd, map_fd;
	unsigned long addr;
	struct ion_allocation_data alloc_data;
	struct ion_fd_data fd_data;
	struct ion_test_data *test_data, ktest_data;
	struct ion_test_data **test_data_table =
		(struct ion_test_data **)ion_tp->test_plan_data;

	if (test_type == NOMINAL_TEST)
		test_data = test_data_table[NOMINAL_TEST];
	else
		test_data = test_data_table[ADV_TEST];

	*test_skipped = !test_data->valid;
	if (!test_data->valid) {
		rc = 0;
		debug(INFO, "%s was skipped\n",__func__);
		goto out;
	}

	ion_fd = open(ion_dev, O_RDONLY);
	if (ion_fd < 0) {
		debug(INFO, "Failed to open ion device\n");
		perror("msm ion");
		return -EIO;
	}
	ion_kernel_fd = open(msm_ion_dev, O_RDWR);
	if (ion_kernel_fd < 0) {
		debug(INFO, "Failed to open msm ion test device\n");
		perror("msm ion");
		close(ion_fd);
		return -EIO;
	}
	alloc_data.len = test_data->size;
	alloc_data.align = test_data->align;
	alloc_data.heap_mask = test_data->heap_mask;
	alloc_data.flags = test_data->flags;
	rc = ioctl(ion_fd, ION_IOC_ALLOC, &alloc_data);
	if (rc) {
		debug(INFO, "Failed to allocate uspace ion buffer\n");
		goto sec_map_alloc_err;
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_SEC, NULL);
	if (rc) {
		debug(INFO, "unable to secure heap\n");
		goto sec_map_alloc_err;
	}
	debug(INFO, "passed secure heap\n");
	fd_data.handle = alloc_data.handle;
	rc = ioctl(ion_fd, ION_IOC_MAP, &fd_data);
	if (rc == 0) {
		if (test_type == ADV_TEST) {
			debug(ERR, "mapping buffer to uspace succeeded\n");
			rc = -EIO;
			close(fd_data.fd);
			goto sec_map_err;
		}
	}
	map_fd = fd_data.fd;
	addr = (unsigned long)mmap(NULL, alloc_data.len,
					PROT_READ | PROT_WRITE,
					MAP_SHARED, map_fd, 0);
	rc = ioctl(ion_fd, ION_IOC_SHARE, &fd_data);
	if (rc) {
		debug(INFO, "unable to share user buffer\n");
		goto sec_map_share_err;
	}
	debug(INFO, "shared user buffer\n");
	rc = ioctl(ion_kernel_fd, IOC_ION_KCLIENT_CREATE, NULL);
	if (rc) {
		debug(INFO, "failed to create kernel client\n");
		goto sec_map_kc_err;
	}
	ktest_data.align = test_data->align;
	ktest_data.size = test_data->size;
	ktest_data.heap_mask = test_data->heap_mask;
	ktest_data.flags = test_data->flags;
	ktest_data.shared_fd = fd_data.fd;
	rc = ioctl(ion_kernel_fd, IOC_ION_UIMPORT, &ktest_data);
	if (rc) {
		debug(INFO, "unable to import ubuf to kernel space\n");
		goto sec_map_uimp_err;
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_KMAP, NULL);
	if (rc == 0 && test_type == ADV_TEST) {
		rc = -EIO;
		debug(INFO, "able to map ubuf to kernel space\n");
		ioctl(ion_kernel_fd, IOC_ION_KUMAP, NULL);
	} else if (rc < 0) {
		rc = 0;
	} else {
		ioctl(ion_kernel_fd, IOC_ION_KUMAP, NULL);
	}
	ioctl(ion_kernel_fd, IOC_ION_KFREE, NULL);
sec_map_uimp_err:
	ioctl(ion_kernel_fd, IOC_ION_KCLIENT_DESTROY, NULL);
sec_map_kc_err:
	close(fd_data.fd);
sec_map_share_err:
	munmap((void *)addr, alloc_data.len);
sec_map_err:
	ioctl(ion_kernel_fd, IOC_ION_UNSEC, NULL);
sec_map_alloc_err:
	ioctl(ion_fd, ION_IOC_FREE, &alloc_data.handle);
	close(ion_kernel_fd);
	close(ion_fd);
out:
	return rc;
}

static struct ion_test_plan sec_map_test = {
	.name = "CP ion map test",
	.test_plan_data = mm_heap_data_settings,
	.test_plan_data_len = 3,
	.test_type_flags = NOMINAL_TEST,
	.test_fn = test_sec_map,
};
static struct ion_test_plan *cp_tests[] = {
	&sec_alloc_test,
	&sec_map_test,
};

struct ion_test_plan **get_cp_ion_tests(const char *dev, size_t *size)
{
	*size = ARRAY_SIZE(cp_tests);
	setup_heaps_for_tests(dev, cp_tests, *size);
	return cp_tests;
}
