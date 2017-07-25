/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MEMORY_PROF_MODULE_H

#include <linux/ioctl.h>

struct memory_prof_map_extra_args {
	int ionfd;
	unsigned long iommu_map_len;
};

#define MEMORY_PROF_MAGIC     'H'

#define MEMORY_PROF_IOC_CLIENT_CREATE _IO(MEMORY_PROF_MAGIC, 0)
#define MEMORY_PROF_IOC_CLIENT_DESTROY _IO(MEMORY_PROF_MAGIC, 1)

#define MEMORY_PROF_IOC_TEST_MAP_EXTRA \
	_IOR(MEMORY_PROF_MAGIC, 2, struct memory_prof_map_extra_args)

#define MEMORY_PROF_IOC_TEST_KERNEL_ALLOCS _IO(MEMORY_PROF_MAGIC, 3)

#endif /* #ifndef _MEMORY_PROF_MODULE_H */
