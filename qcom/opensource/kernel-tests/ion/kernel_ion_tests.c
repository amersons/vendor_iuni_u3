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

/* MSM ION Kernel api tests. */

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
	.align = 0x1000,
	.size = 0x1000,
	.heap_mask = ION_HEAP(ION_SYSTEM_HEAP_ID),
	.heap_type_req = SYSTEM_MEM,
	.flags = ION_FLAG_CACHED,
};
static struct ion_test_data phys_heap_test = {
	.align = 0x1000,
	.size = 0x1000,
	.heap_mask = ION_HEAP(ION_QSECOM_HEAP_ID),
	.heap_type_req = CARVEOUT,
	.flags = 0,
};
static struct ion_test_data adv_mm_heap_test = {
	.align = 0x1000,
	.size = 0xC0000000,
	.heap_mask = ION_HEAP(ION_QSECOM_HEAP_ID),
	.heap_type_req = CARVEOUT,
	.flags = 0,
};
static struct ion_test_data adv_system_heap_test = {
	.align = 0x1000,
	.size = 0x1000,
	.heap_mask = ION_HEAP(ION_SYSTEM_HEAP_ID),
	.heap_type_req = SYSTEM_MEM,
	.flags = 0,
};

static struct ion_test_data *mm_heap_data_settings[] = {
	[NOMINAL_TEST] = &mm_heap_test,
	[ADV_TEST] = &adv_mm_heap_test,
};

/*
 * Kernel ion client create test
 */
int test_kclient(const char *ion_dev, const char *msm_ion_dev,
			struct ion_test_plan *ion_tp, int test_type,
			int *test_skipped)
{
	int ion_kernel_fd, rc;
	ion_kernel_fd = open(msm_ion_dev, O_RDWR);
	if (ion_kernel_fd < 0) {
		debug(ERR, "Failed to open msm ion test device\n");
		perror("msm ion");
		return -EIO;
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_KCLIENT_CREATE, NULL);
	if (rc < 0) {
		debug(ERR, "kernel client create failed\n");
		return -EIO;
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_KCLIENT_DESTROY, NULL);
	close(ion_kernel_fd);
	*test_skipped = 0;
	return 0;
}

static struct ion_test_plan kclient_test = {
	.name = "Kernel ion client",
	.test_type_flags = NOMINAL_TEST,
	.test_plan_data_len = 0,
	.test_fn = test_kclient,
};
/*
 * Kernel ion buf alloc
 * Nominal test: Using mm heap.
 * ADV test: Invalid client, and erroneous alloc request
 */
int test_kalloc(const char *ion_dev, const char *msm_ion_dev,
		struct ion_test_plan *ion_tp, int test_type,
		int *test_skipped)
{
	int ion_kernel_fd, rc;
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
	ion_kernel_fd = open(msm_ion_dev, O_RDWR);
	if (ion_kernel_fd < 0) {
		debug(ERR, "Failed to open msm ion test device\n");
		perror("msm ion");
		return -EIO;
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_KCLIENT_CREATE, NULL);
	if (rc < 0) {
		debug(ERR, "kernel client create failed\n");
		close(ion_kernel_fd);
		return -EIO;
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_KALLOC, test_data);
	if (rc < 0 && test_type == NOMINAL_TEST) {
		debug(ERR, "Nominal kernel alloc buf failed\n");
		goto alloc_err;
	} else if (rc < 0 && test_type == ADV_TEST) {
		rc = 0;
		goto alloc_err;
	} else if (rc == 0 && test_type == ADV_TEST) {
		debug(INFO, "erroneous alloc request succeeded\n");
		rc = EIO;
	}
	ioctl(ion_kernel_fd, IOC_ION_KFREE, NULL);
alloc_err:
	ioctl(ion_kernel_fd, IOC_ION_KCLIENT_DESTROY, NULL);
	close(ion_kernel_fd);
out:
	return rc;

}

static struct ion_test_plan kalloc_test = {
	.name = "Kernel ion alloc buf",
	.test_plan_data = mm_heap_data_settings,
	.test_plan_data_len = 3,
	.test_type_flags = NOMINAL_TEST | ADV_TEST,
	.test_fn = test_kalloc,
};
/*
 * Kernel ion phys attr
 * Nominal test: use mm heap settings
 * Adeveserial test: Invalid handle and client
 */
int test_kphys(const char *ion_dev, const char *msm_ion_dev,
			struct ion_test_plan *ion_tp, int test_type,
			int *test_skipped)
{
	int ion_kernel_fd, rc;
	struct ion_test_data *test_data =
		(struct ion_test_data *)ion_tp->test_plan_data;

	*test_skipped = !test_data->valid;

	if (!test_data->valid) {
		rc = 0;
		debug(INFO, "%s was skipped\n",__func__);
		goto out;
	}

	ion_kernel_fd = open(msm_ion_dev, O_RDWR);
	if (ion_kernel_fd < 0) {
		debug(ERR, "Failed to open msm ion test device\n");
		perror("msm ion");
		return -EIO;
	}
	/*
	 * ADV test 1: invalid client
	 * TODO: Requires fix in ion_phys
	 * Currently causes a creash
	 */
	if (test_type == ADV_TEST) {
		rc = ioctl(ion_kernel_fd, IOC_ION_KPHYS, NULL);
		if (!rc) {
			debug(INFO, "phys attr obtained with invalid handle\n");
			rc = -EIO;
			close(ion_kernel_fd);
			return rc;
		}
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_KCLIENT_CREATE, NULL);
	if (rc < 0) {
		debug(ERR, "kernel client create failed\n");
		goto n_kphys_kc_err;
	}
	/*
	 * ADV test 2: invalid handle
	 * TODO: Requires fix in ion_phys
	 * Currently causes a creash
	 */
	if (test_type == ADV_TEST) {
		rc = ioctl(ion_kernel_fd, IOC_ION_KPHYS, NULL);
		if (!rc) {
			debug(INFO, "phys attr obtained with invalid handle\n");
			rc = -EIO;
			goto n_kphys_alloc_err;
		}
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_KALLOC, test_data);
	if (rc < 0) {
		debug(ERR, "kernel alloc buf failed\n");
		goto n_kphys_alloc_err;
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_KPHYS, NULL);
	ioctl(ion_kernel_fd, IOC_ION_KFREE, NULL);
n_kphys_alloc_err:
	ioctl(ion_kernel_fd, IOC_ION_KCLIENT_DESTROY, NULL);
n_kphys_kc_err:
	close(ion_kernel_fd);
out:
	return rc;
}

static struct ion_test_plan kphys_test = {
	.name = "Kernel ion buf phys attr test",
	.test_plan_data = &phys_heap_test,
	.test_plan_data_len = 1,
	/*
	 * TODO: Enable ADV test after ion phys api
	 * can handle erroneous input
	 */
	.test_type_flags = NOMINAL_TEST,
	.test_fn = test_kphys,
};

/*
 * Kernel ion system phys attr
 * Adeveserial test: get phys attr from system heap
 */
int test_ksystem_phys(const char *ion_dev, const char *msm_ion_dev,
			struct ion_test_plan *ion_tp, int test_type,
			int *test_skipped)
{
	int ion_kernel_fd, rc;
	struct ion_test_data *test_data =
		(struct ion_test_data *)ion_tp->test_plan_data;

	*test_skipped = !test_data->valid;
	if (!test_data->valid) {
		rc = 0;
		debug(INFO, "%s was skipped\n",__func__);
		goto out;
	}

	ion_kernel_fd = open(msm_ion_dev, O_RDWR);
	if (ion_kernel_fd < 0) {
		debug(ERR, "Failed to open msm ion test device\n");
		perror("msm ion");
		return -EIO;
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_KCLIENT_CREATE, NULL);
	if (rc < 0) {
		debug(ERR, "kernel client create failed\n");
		goto n_kphys_kc_err;
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_KALLOC, test_data);
	if (rc < 0) {
		debug(ERR, "kernel system alloc buf failed\n");
		goto n_kphys_alloc_err;
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_KPHYS, NULL);
	if (rc < 0 && test_type == ADV_TEST)
		rc = 0;
	ioctl(ion_kernel_fd, IOC_ION_KFREE, NULL);
n_kphys_alloc_err:
	ioctl(ion_kernel_fd, IOC_ION_KCLIENT_DESTROY, NULL);
n_kphys_kc_err:
	close(ion_kernel_fd);
out:
	return rc;
}

static struct ion_test_plan ksystem_test = {
	.name = "Kernel system buf phys attr test",
	.test_plan_data = &adv_system_heap_test,
	.test_plan_data_len = 1,
	.test_type_flags = ADV_TEST,
	.test_fn = test_ksystem_phys,
};
/*
 * Kernel ion map
 */
int test_kmap(const char *ion_dev, const char *msm_ion_dev,
			struct ion_test_plan *ion_tp, int test_type,
			int *test_skipped)
{
	*test_skipped = 0;
	if (test_type == NOMINAL_TEST) {
		int ion_kernel_fd, rc;
		struct ion_test_data *test_data =
			(struct ion_test_data *)ion_tp->test_plan_data;

		*test_skipped = !test_data->valid;

		if (!test_data->valid) {
			rc = 0;
			debug(INFO, "%s was skipped\n",__func__);
			goto out;
		}

		ion_kernel_fd = open(msm_ion_dev, O_RDWR);
		if (ion_kernel_fd < 0) {
			debug(ERR, "Failed to open msm ion test device\n");
			perror("msm ion");
			return -EIO;
		}
		rc = ioctl(ion_kernel_fd, IOC_ION_KCLIENT_CREATE, NULL);
		if (rc < 0) {
			debug(ERR, "kernel client create failed\n");
			goto n_kmap_kc_err;
		}
		rc = ioctl(ion_kernel_fd, IOC_ION_KALLOC, test_data);
		if (rc < 0) {
			debug(ERR, "kernel alloc buf failed\n");
			goto n_kmap_alloc_err;
		}
		rc = ioctl(ion_kernel_fd, IOC_ION_KMAP, NULL);
		if (!rc) {
			rc = ioctl(ion_kernel_fd, IOC_ION_WRITE_VERIFY, NULL);
			if (rc < 0)
				debug(ERR, "kernel map verification fails\n");
			ioctl(ion_kernel_fd, IOC_ION_KUMAP, NULL);
		}
		ioctl(ion_kernel_fd, IOC_ION_KFREE, NULL);
n_kmap_alloc_err:
		ioctl(ion_kernel_fd, IOC_ION_KCLIENT_DESTROY, NULL);
n_kmap_kc_err:
		close(ion_kernel_fd);
		return rc;
	}
out:
	return 0;
}
static struct ion_test_plan kmap_test = {
	.name = "Kernel ion map test",
	.test_plan_data = &mm_heap_test,
	.test_plan_data_len = 1,
	.test_type_flags = NOMINAL_TEST,
	.test_fn = test_kmap,
};
/*
 * Kernel ion user import test
 */
int test_kuimport(const char *ion_dev, const char *msm_ion_dev,
			struct ion_test_plan *ion_tp, int test_type,
			int *test_skipped)
{
	int ion_kernel_fd, rc, ion_fd, map_fd;
	struct ion_allocation_data alloc_data;
	struct ion_fd_data fd_data;
	unsigned long addr;
	struct ion_test_data *test_data =
		(struct ion_test_data *)ion_tp->test_plan_data;

	*test_skipped = !test_data->valid;

	if (!test_data->valid) {
		rc = 0;
		debug(INFO, "%s was skipped\n",__func__);
		goto out;
	}

	ion_kernel_fd = open(msm_ion_dev, O_RDWR);
	if (ion_kernel_fd < 0) {
		debug(ERR, "Failed to open msm ion test device\n");
		perror("msm ion");
		return -EIO;
	}
	rc = ioctl(ion_kernel_fd, IOC_ION_KCLIENT_CREATE, NULL);
	if (rc < 0) {
		debug(ERR, "kernel client create failed\n");
		close(ion_kernel_fd);
		return -EIO;
	}
	ion_fd = open(ion_dev, O_RDONLY);
	if (ion_fd < 0) {
		debug(ERR, "Failed to open ion device\n");
		perror("msm ion");
		rc = -EIO;
		goto kuimp_id_err;
	}
	if (test_type == ADV_TEST) {
		rc = ioctl(ion_kernel_fd, IOC_ION_UIMPORT, &ion_fd);
		if (rc < 0)
			rc = 0;
		else
			rc = -EIO;
		goto kuimp_adv_test_close;
	}
	if (test_type == NOMINAL_TEST) {
		alloc_data.len = test_data->size;
		alloc_data.align = test_data->align;
		alloc_data.heap_mask = test_data->heap_mask;
		alloc_data.flags = test_data->flags;
		rc = ioctl(ion_fd, ION_IOC_ALLOC, &alloc_data);
		if (rc) {
			debug(ERR, "Failed to allocate uspace ion buffer\n");
			goto kuimp_adv_test_close;
		}
		fd_data.handle = alloc_data.handle;
		rc = ioctl(ion_fd, ION_IOC_MAP, &fd_data);
		if (rc) {
			debug(ERR, "Failed to map uspace ion buffer\n");
			goto kuimp_map_err;
		}
		map_fd = fd_data.fd;
		addr = (unsigned long)mmap(NULL, alloc_data.len,
						PROT_READ | PROT_WRITE,
						MAP_SHARED , map_fd, 0);
		write_pattern(addr, alloc_data.len);
		rc = ioctl(ion_fd, ION_IOC_SHARE, &fd_data);
		if (rc) {
			debug(ERR, "Failed to share uspace ion buffer fd\n");
			goto kuimp_share_err;
		}
		test_data->shared_fd = fd_data.fd;
		rc = ioctl(ion_kernel_fd, IOC_ION_UIMPORT, test_data);
		if (!rc) {
			rc = ioctl(ion_kernel_fd, IOC_ION_KMAP, NULL);
			if (!rc)
				rc = ioctl(ion_kernel_fd, IOC_ION_VERIFY, NULL);
			else
				debug(ERR, "failed to map uimp buffer\n");
			ioctl(ion_kernel_fd, IOC_ION_KUMAP, NULL);
			ioctl(ion_kernel_fd, IOC_ION_KFREE, NULL);
		}
		if (rc)
			debug(ERR, "Failed to import uspace buffer\n");
kuimp_share_err:
		munmap((void *)addr, alloc_data.len);
		close(map_fd);
kuimp_map_err:
		ioctl(ion_fd, ION_IOC_FREE, &alloc_data.handle);
	}
kuimp_adv_test_close:
	close(ion_fd);
kuimp_id_err:
	ioctl(ion_kernel_fd, IOC_ION_KCLIENT_DESTROY, NULL);
	close(ion_kernel_fd);
out:
	return rc;
}

static struct ion_test_plan kuimp_test = {
	.name = "Kernel ion uspace buffer import",
	.test_plan_data = &mm_heap_test,
	.test_plan_data_len = 1,
	.test_type_flags = ADV_TEST,
	.test_fn = test_kuimport,
};

/*
 * Kernel ion imported user buf attributes test
 */
int test_ubuf_attr(const char *ion_dev, const char *msm_ion_dev,
			struct ion_test_plan *ion_tp, int test_type,
			int *test_skipped)
{
	*test_skipped = 0;
	if (test_type == NOMINAL_TEST)	{
		int ion_kernel_fd, rc, ion_fd, map_fd;
		struct ion_allocation_data alloc_data;
		struct ion_fd_data fd_data;
		unsigned long addr, flags, size;
		struct ion_test_data *test_data =
				(struct ion_test_data *)ion_tp->test_plan_data;

		*test_skipped = !test_data->valid;
		if (!test_data->valid) {
			rc = 0;
			debug(INFO, "%s was skipped (nom)\n",__func__);
			goto out;
		}

		ion_kernel_fd = open(msm_ion_dev, O_RDWR);
		if (ion_kernel_fd < 0) {
			debug(ERR, "Failed to open msm ion test device\n");
			perror("msm ion");
			return -EIO;
		}
		rc = ioctl(ion_kernel_fd, IOC_ION_KCLIENT_CREATE, NULL);
		if (rc < 0) {
			debug(ERR, "kernel client create failed\n");
			close(ion_kernel_fd);
			return -EIO;
		}
		ion_fd = open(ion_dev, O_RDONLY);
		if (ion_fd < 0) {
			debug(ERR, "Failed to open ion device\n");
			perror("msm ion");
			rc = -EIO;
			goto kubuf_id_err;
		}
		alloc_data.len = test_data->size;
		alloc_data.align = test_data->align;
		alloc_data.heap_mask = test_data->heap_mask;
		alloc_data.flags = test_data->flags;
		rc = ioctl(ion_fd, ION_IOC_ALLOC, &alloc_data);
		if (rc) {
			debug(ERR, "Failed to allocate uspace ion buffer\n");
			goto n_kubuf_alloc_err;
		}
		fd_data.handle = alloc_data.handle;
		rc = ioctl(ion_fd, ION_IOC_SHARE, &fd_data);
		if (rc) {
			debug(ERR, "Failed to share uspace ion buffer fd\n");
			goto n_kubuf_share_err;
		}
		test_data->shared_fd = fd_data.fd;
		rc = ioctl(ion_kernel_fd, IOC_ION_UIMPORT, test_data);
		if (!rc) {
			rc = ioctl(ion_kernel_fd, IOC_ION_UBUF_FLAGS, &flags);
			if (!rc)
				rc = ioctl(ion_kernel_fd, IOC_ION_UBUF_SIZE,
									&size);
			else if (rc < 0) {
				debug(ERR, "failed to get ubuf attributes\n");
			} else {
				rc =  ((size == alloc_data.len) &&
					(flags == alloc_data.flags)) ?
					0 : -EIO;
			}
			ioctl(ion_kernel_fd, IOC_ION_KFREE, NULL);
		} else
			debug(ERR, "failed to import uspace buf\n");
		close(fd_data.fd);
n_kubuf_share_err:
		ioctl(ion_fd, ION_IOC_FREE, &alloc_data.handle);
n_kubuf_alloc_err:
		close(ion_fd);
kubuf_id_err:
		ioctl(ion_kernel_fd, IOC_ION_KCLIENT_DESTROY, NULL);
		close(ion_kernel_fd);
		return rc;
	} else if (test_type == ADV_TEST) {
		int ion_kernel_fd, rc, ion_fd, map_fd;
		struct ion_allocation_data alloc_data;
		struct ion_fd_data fd_data;
		unsigned long addr, flags, size;
		struct ion_test_data *test_data =
				(struct ion_test_data *)ion_tp->test_plan_data;

		*test_skipped = !test_data->valid;

		if (!test_data->valid) {
			rc = 0;
			debug(INFO, "%s was skipped (adv)\n",__func__);
			goto out;
		}

		ion_kernel_fd = open(msm_ion_dev, O_RDWR);
		if (ion_kernel_fd < 0) {
			debug(ERR, "Failed to open msm ion test device\n");
			perror("msm ion");
			return -EIO;
		}
		ion_fd = open(ion_dev, O_RDONLY);
		if (ion_fd < 0) {
			debug(ERR, "Failed to open ion device\n");
			perror("msm ion");
			close(ion_kernel_fd);
			return -EIO;
		}
		alloc_data.len = test_data->size;
		alloc_data.align = test_data->align;
		alloc_data.heap_mask = test_data->heap_mask;
		alloc_data.flags = test_data->flags;
		rc = ioctl(ion_fd, ION_IOC_ALLOC, &alloc_data);
		if (rc) {
			debug(ERR, "Failed to allocate uspace ion buffer\n");
			goto a_kubuf_alloc_err;
		}
		fd_data.handle = alloc_data.handle;
		rc = ioctl(ion_fd, ION_IOC_SHARE, &fd_data);
		if (rc) {
			debug(ERR, "Failed to share uspace ion buffer fd\n");
			goto a_kubuf_share_err;
		}
		test_data->shared_fd = fd_data.fd;
		rc = ioctl(ion_kernel_fd, IOC_ION_UBUF_FLAGS, &flags);
		if (rc)
			rc = ioctl(ion_kernel_fd, IOC_ION_UBUF_SIZE, &size);
		if (rc == 0) {
			debug(INFO, "can read ubuf attr with invalid client\n");
			goto a_kubuf_attr_client_err;
		}
		rc = ioctl(ion_kernel_fd, IOC_ION_KCLIENT_CREATE, NULL);
		if (rc < 0) {
			debug(INFO, "kernel client create failed\n");
			goto a_kubuf_attr_client_err;
		}
		rc = ioctl(ion_kernel_fd, IOC_ION_UBUF_FLAGS, &flags);
		if (rc)
			rc = ioctl(ion_kernel_fd, IOC_ION_UBUF_SIZE, &size);
		if (rc == 0) {
			debug(INFO, "can read ubuf attr with invalid handle\n");
			goto a_kubuf_attr_handle_err;
		}
		rc = 0;
a_kubuf_attr_handle_err:
		ioctl(ion_kernel_fd, IOC_ION_KCLIENT_DESTROY, NULL);
a_kubuf_attr_client_err:
a_kubuf_share_err:
		ioctl(ion_fd, ION_IOC_FREE, &alloc_data.handle);
a_kubuf_alloc_err:
		close(ion_fd);
		close(ion_kernel_fd);
	}
out:
	return 0;
}
static struct ion_test_plan kubuf_attr_test = {
	.name = "Kernel ion uspace buffer size and flags",
	.test_plan_data = &mm_heap_test,
	.test_plan_data_len = 1,
	/* TODO: ADV test causes crash in ion.
	 * Needs to be fixed till then ADV test
	 * diabled
	 */
	.test_type_flags = NOMINAL_TEST,
	.test_fn = test_ubuf_attr,
};
static struct ion_test_plan *kernel_tests[] = {
	&kclient_test,
	&kalloc_test,
	&kphys_test,
	&ksystem_test,
	&kmap_test,
	&kuimp_test,
	&kubuf_attr_test,
};

struct ion_test_plan **get_kernel_ion_tests(const char *dev, size_t *size)
{
	*size = ARRAY_SIZE(kernel_tests);
	setup_heaps_for_tests(dev, kernel_tests, *size);
	return kernel_tests;
}
