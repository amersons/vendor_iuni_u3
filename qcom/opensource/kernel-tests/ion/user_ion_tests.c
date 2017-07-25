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

/* MSM ION User space tests. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <malloc.h>
#include <linux/input.h>
#include <linux/msm_ion.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include "ion_test_plan.h"
#include "ion_test_utils.h"

#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof((arr)[0]))

static struct ion_test_data mm_heap_test = {
	.align = 0x1000,
	.size = 0x1000,
	.heap_type_req = SYSTEM_MEM2,
	.heap_mask = ION_HEAP(ION_IOMMU_HEAP_ID),
};

static struct ion_test_data adv_mm_heap_test = {
	.align = 0x1000,
	.size = 0xC0000000,
	.heap_type_req = CARVEOUT,
	.heap_mask = ION_HEAP(ION_QSECOM_HEAP_ID),
};

static struct ion_test_data *mm_heap_data_settings[] = {
	[NOMINAL_TEST] = &mm_heap_test,
	[ADV_TEST] = &adv_mm_heap_test,
};

static int test_ualloc(const char *ion_dev, const char *msm_ion_dev,
			struct ion_test_plan *ion_tp, int test_type,
			int *test_skipped)
{
	int ion_fd, rc;
	struct ion_allocation_data alloc_data;
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
		debug(ERR, "Failed to open msm ion test device\n");
		perror("msm ion");
		return -EIO;
	}
	alloc_data.len = test_data->size;
	alloc_data.align = test_data->align;
	alloc_data.heap_mask = test_data->heap_mask;
	alloc_data.flags = test_data->flags;
	rc = ioctl(ion_fd, ION_IOC_ALLOC, &alloc_data);
	if (rc < 0 && test_type == NOMINAL_TEST) {
		debug(ERR, "Nominal user alloc buf failed\n");
		perror("msm ion");
		goto ualloc_err;
	} else if (rc < 0 && test_type == ADV_TEST) {
		rc = 0;
		goto ualloc_err;
	} else if (rc == 0 && test_type == ADV_TEST) {
		debug(INFO, "erroneous alloc request succeeded\n");
		rc = -EIO;
	}
	ioctl(ion_fd, ION_IOC_FREE, &alloc_data.handle);
ualloc_err:
	close(ion_fd);
out:
	return rc;

}

static struct ion_test_plan ualloc_test = {
	.name = "User ion alloc buf",
	.test_plan_data = mm_heap_data_settings,
	.test_plan_data_len = 3,
	.test_type_flags = NOMINAL_TEST | ADV_TEST,
	.test_fn = test_ualloc,
};

static int test_umap(const char *ion_dev, const char *msm_ion_dev,
			struct ion_test_plan *ion_tp, int test_type,
			int *test_skipped)
{
	int ion_fd, map_fd, rc;
	void *addr;
	struct ion_allocation_data alloc_data;
	struct ion_fd_data fd_data;
	struct ion_test_data *test_data =
		(struct ion_test_data *)ion_tp->test_plan_data;

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
	alloc_data.len = test_data->size;
	alloc_data.align = test_data->align;
	alloc_data.heap_mask = test_data->heap_mask;
	alloc_data.flags = test_data->flags;
	rc = ioctl(ion_fd, ION_IOC_ALLOC, &alloc_data);
	if (rc) {
		debug(ERR, "Failed to allocate uspace ion buffer\n");
		goto umap_alloc_err;
	}
	/*
	 * ADV TEST1: Use invalid handle
	 */
	if (test_type == ADV_TEST) {
		rc = ioctl(ion_fd, ION_IOC_MAP, &fd_data);
		if (!rc) {
			debug(INFO, "mapped using invalid handle\n");
			rc = -EIO;
			goto umap_map_err;
		}
	}
	fd_data.handle = alloc_data.handle;
	rc = ioctl(ion_fd, ION_IOC_MAP, &fd_data);
	if (rc < 0) {
		debug(INFO, "unable to ion map buffer\n");
		goto umap_map_err;
	}
	map_fd = fd_data.fd;
	/*
	 * ADV test2: map twice the allocted length
	 */
	if (test_type == ADV_TEST) {
		addr = mmap(NULL, alloc_data.len * 2,
					PROT_READ | PROT_WRITE,
					MAP_SHARED, map_fd, 0);
		if (!addr) {
			debug(INFO, "mmap succeded using larger map length\n");
			munmap(addr, alloc_data.len * 2);
			rc = -EIO;
			goto umap_map_err;
		}
	}
	addr = mmap(NULL, alloc_data.len,
					PROT_READ | PROT_WRITE,
					MAP_SHARED, map_fd, 0);
	if (!addr) {
		debug(INFO, "mmap failed\n");
		goto umap_map_err;
	}
	write_pattern((unsigned long)addr, alloc_data.len);
	if (verify_pattern((unsigned long)addr, alloc_data.len)) {
		debug(INFO, "mapped buffer not writeable\n");
		rc = -EIO;
	} else
		rc = 0;
	munmap(addr, alloc_data.len);
	close(map_fd);
	ioctl(ion_fd, ION_IOC_FREE, &alloc_data.handle);
	/*
	 * Nominal Test with alloc length / 2
	 */
	alloc_data.len = test_data->size * 2;
	rc = ioctl(ion_fd, ION_IOC_ALLOC, &alloc_data);
	if (rc) {
		debug(ERR, "Failed to allocate uspace ion buffer\n");
		goto umap_alloc_err;
	}
	fd_data.handle = alloc_data.handle;
	rc = ioctl(ion_fd, ION_IOC_MAP, &fd_data);
	if (rc < 0) {
		debug(INFO, "unable to ion map buffer\n");
		goto umap_map_err;
	}
	map_fd = fd_data.fd;
	addr = mmap(NULL, alloc_data.len/2,
					PROT_READ | PROT_WRITE,
					MAP_SHARED, map_fd, 0);
	if (!addr) {
		debug(INFO, "mmap failed\n");
		close(map_fd);
		goto umap_map_err;
	}
	write_pattern((unsigned long)addr, alloc_data.len/2);
	if (verify_pattern((unsigned long)addr, alloc_data.len/2)) {
		debug(INFO, "mapped buffer not writeable\n");
		rc = -EIO;
	} else
		rc = 0;
	munmap(addr, alloc_data.len/2);
	close(map_fd);
umap_map_err:
	ioctl(ion_fd, ION_IOC_FREE, &alloc_data.handle);
umap_alloc_err:
	close(ion_fd);
out:
	return rc;
}

static struct ion_test_plan umap_test = {
	.name = "User ion buf map",
	.test_plan_data = &mm_heap_test,
	.test_plan_data_len = 1,
	.test_type_flags = NOMINAL_TEST | ADV_TEST,
	.test_fn = test_umap,
};

static int uimport_ion_buf(int fd)
{
	static char cbuf[100];
	int ion_fd, size, rc;
	void *addr;
	struct ion_fd_data fd_data;
	debug(INFO, "importing data in child done\n");
	/* receive buf fd */
	if (-EIO == sock_read(fd, cbuf, sizeof(int)*8))
		return -EIO;
	fd_data.fd = atoi(cbuf);
	/* receive buf length */
	if (-EIO == sock_read(fd, cbuf, sizeof(int)*8))
		return -EIO;
	size = atoi(cbuf);
	/* receive ion dev name */
	if (-EIO == sock_read(fd, cbuf, sizeof(cbuf)))
		return -EIO;
	ion_fd = open(cbuf, O_RDONLY);
	if (ion_fd < 1) {
		debug(INFO, "unable to open ion device\n");
		rc = -EINVAL;
		goto child_args_err;
	}
	rc = ioctl(ion_fd, ION_IOC_IMPORT, &fd_data);
	if (rc) {
		debug(INFO, "ION_IOC_IMPORT failed\n");
		rc = -EIO;
		goto child_imp_err;
	}
	addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
					MAP_SHARED, fd_data.fd, 0);
	if (!addr) {
		debug(INFO, "failed to map buffer\n");
		rc = -EIO;
		goto child_mmap_err;
	}
	if (verify_pattern((unsigned long)addr, (size_t)size)) {
		debug(INFO, "unable to verify imported buffer\n");
		rc = -EIO;
	} else
		debug(INFO, "import test passed\n");
	munmap(addr, size);
child_mmap_err:
	close(fd_data.fd);
	ioctl(ion_fd, ION_IOC_FREE, &fd_data.handle);
child_imp_err:
	close(ion_fd);
child_args_err:
	itoa(rc, cbuf, sizeof(int)*8);
child_sock_err:
	close(fd);
	return rc;
}

static int uimp_spawn_process(pid_t *pid, int *wr_fd)
{
	int socket[2], child;
	if (pipe(socket) < 0) {
		debug(INFO, "unable to create pipe\n");
		return -EIO;
	}
	child = fork();
	if (child == -1) {
		debug(INFO, "unable to fork process\n");
		return -EIO;
	} else if (child == 0) {
		close(socket[1]);
		if (uimport_ion_buf(socket[0]))
			exit(1) ;
		exit(0);
	} else {
		*pid = child;
		close(socket[0]);
		*wr_fd = socket[1];
		return 0;
	}
}

static int test_uimp(const char *ion_dev, const char *msm_ion_dev,
			struct ion_test_plan *ion_tp, int test_type,
			int *test_skipped)
{
	int ion_fd, map_fd, rc, wr_fd;
	int sock_fd;
	char buf[32];
	void *addr;
	struct ion_allocation_data alloc_data;
	struct ion_fd_data fd_data;
	int child_status;
	static char ubuf[100];
	pid_t tpid, child_pid;
	struct ion_test_data *test_data =
		(struct ion_test_data *)ion_tp->test_plan_data;

	*test_skipped = !test_data->valid;
	if (!test_data->valid) {
		rc = 0;
		debug(INFO, "%s was skipped\n",__func__);
		goto out;
	}

	rc = uimp_spawn_process(&child_pid, &wr_fd);
	if (rc)
		return -EIO;

	ion_fd = open(ion_dev, O_RDONLY);
	if (ion_fd < 0) {
		debug(INFO, "Failed to open ion device\n");
		perror("msm ion");
		return -EIO;
	}
	alloc_data.len = test_data->size;
	alloc_data.align = test_data->align;
	alloc_data.heap_mask = test_data->heap_mask;
	alloc_data.flags = test_data->flags;
	rc = ioctl(ion_fd, ION_IOC_ALLOC, &alloc_data);
	if (rc) {
		debug(INFO, "Failed to allocate uspace ion buffer\n");
		goto uimp_alloc_err;
	}
	fd_data.handle = alloc_data.handle;
	rc = ioctl(ion_fd, ION_IOC_MAP, &fd_data);
	if (rc < 0) {
		debug(INFO, "unable to ion map buffer\n");
		goto uimp_map_err;
	}
	map_fd = fd_data.fd;
	addr = mmap(NULL, alloc_data.len,
					PROT_READ | PROT_WRITE,
					MAP_SHARED , map_fd, 0);
	if (!addr) {
		debug(INFO, "mmap failed\n");
		rc = -EIO;
		goto uimp_mmap_err;
	}
	write_pattern((unsigned long)addr, alloc_data.len);
	fd_data.handle = alloc_data.handle;
	rc = ioctl(ion_fd, ION_IOC_SHARE, &fd_data);
	if (rc < 0) {
		debug(INFO, "unable to share ion buffer\n");
		goto uimp_share_err;
	}
	if (test_type == NOMINAL_TEST)
		itoa(fd_data.fd, ubuf, sizeof(int) * 8);
	else
		itoa(ion_fd, ubuf, sizeof(int) * 8);
	if (-EIO == sock_write(wr_fd, ubuf, sizeof(int) * 8))
		goto uimp_sock_err;
	itoa(alloc_data.len, ubuf, sizeof(int) * 8);
	if (-EIO == sock_write(wr_fd, ubuf, sizeof(int) * 8))
		goto uimp_sock_err;
	strncpy(ubuf, ion_dev, sizeof(ubuf));
	if (-EIO == sock_write(wr_fd, ubuf, sizeof(ubuf)))
		goto uimp_sock_err;
	do {
		tpid = wait(&child_status);
	} while (tpid != child_pid);
	if (test_type == NOMINAL_TEST && child_status != 0)
		rc = child_status;
	else if (test_type == ADV_TEST && child_status == 0)
		rc = -EIO;
	else
		rc = 0;
uimp_sock_err:
	close(fd_data.fd);
uimp_share_err:
	munmap(addr, alloc_data.len);
uimp_mmap_err:
	close(map_fd);
uimp_map_err:
	ioctl(ion_fd, ION_IOC_FREE, &alloc_data.handle);
uimp_alloc_err:
	close(ion_fd);
	close(wr_fd);
out:
	return rc;
}

static struct ion_test_plan uimp_test = {
	.name = "User ion import test ",
	.test_plan_data = &mm_heap_test,
	.test_plan_data_len = 1,
	.test_type_flags = ADV_TEST,
	.test_fn = test_uimp,
};

static struct ion_test_plan *user_tests[] = {
	&ualloc_test,
	&umap_test,
	&uimp_test,
};

struct ion_test_plan **get_user_ion_tests(const char *dev, size_t *size)
{
	*size = ARRAY_SIZE(user_tests);
	setup_heaps_for_tests(dev, user_tests, *size);
	return user_tests;
}
