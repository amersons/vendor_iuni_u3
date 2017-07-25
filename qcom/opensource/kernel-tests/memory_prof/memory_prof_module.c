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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>
#include <asm-generic/sizes.h>
#include <mach/iommu_domains.h>
#include <linux/msm_ion.h>
#include <asm/uaccess.h>

#include "memory_prof_module.h"
#include "timing_debug.h"

#define MEMORY_PROF_DEV_NAME "memory_prof"

struct memory_prof_module_data {
	struct ion_client *client;
};

static struct class *memory_prof_class;
static int memory_prof_major;
static struct device *memory_prof_dev;

static void zero_the_pages_plz(struct page **pages, int npages,
			unsigned int page_size)
{
	void *ptr;
	int len = npages * page_size;
	ptr = vmap(pages, npages, VM_IOREMAP, pgprot_writecombine(PAGE_KERNEL));
	if (ptr == NULL) {
		WARN(1, "BOGUS vmap ERROR!!!\n");
	} else {
		memset(ptr, 0, len);
		dmac_flush_range(ptr, ptr + len);
		vunmap(ptr);
	}
}

static void test_kernel_alloc(const char *tag, gfp_t thegfp,
			unsigned int page_size,
			void (*transform_page)(struct page *),
			void (*transform_pages)(struct page **, int, unsigned int))
{
	char st1[200];
	char st2[200];
	int i, j;
	int order = get_order(page_size);
	int size = (SZ_1M * 20);
	int npages = PAGE_ALIGN(size) / page_size;
	struct page **pages;

	pages = kmalloc(npages * sizeof(struct page *),
				GFP_KERNEL);

	snprintf(st1, 200, "before %s%s", tag,
		transform_page ? " (flush each)" : "");
	snprintf(st2, 200, "after  %s%s", tag,
		transform_page ? " (flush each)" : "");

	timing_debug_tick(st1);
	for (i = 0; i < npages; ++i) {
		pages[i] = alloc_pages(thegfp, order);
		if (!pages[i]) {
			WARN(1, "BOGUS alloc_pages ERROR!!!\n");
			npages = i;
			goto out;
		}
		if (transform_page)
			for (j = 0; j < order; j++)
				transform_page(nth_page(pages[i], j));
	}
	timing_debug_tock(st2);

	if (transform_pages) {
		timing_debug_tick("before transform_pages");
		transform_pages(pages, npages, page_size);
		timing_debug_tock("after  transform_pages");
	}

out:
	for (i = 0; i < npages; ++i)
		__free_pages(pages[i], order);

	kfree(pages);
}

#define DO_TEST_KERNEL_ALLOC(thegfp, sz, transform_page, transform_pages) \
	test_kernel_alloc(#thegfp " " #sz, thegfp, sz,	\
			transform_page, transform_pages)

static void test_kernel_allocs(void)
{
	int i;
	timing_debug_init();
	timing_debug_start_new_timings();

	pr_err("Testing small pages without flushing individual pages\n");
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO,
				PAGE_SIZE, NULL, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(GFP_KERNEL | __GFP_HIGHMEM,
				PAGE_SIZE, NULL, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(GFP_KERNEL | __GFP_HIGHMEM,
				PAGE_SIZE, NULL, zero_the_pages_plz);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(GFP_KERNEL | __GFP_ZERO,
				PAGE_SIZE, NULL, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(GFP_KERNEL,
				PAGE_SIZE, NULL, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(GFP_KERNEL,
				PAGE_SIZE, NULL, zero_the_pages_plz);

	pr_err("Testing small pages with flushing individual pages\n");
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO,
				PAGE_SIZE, flush_dcache_page, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(GFP_KERNEL | __GFP_HIGHMEM,
				PAGE_SIZE, flush_dcache_page, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(GFP_KERNEL | __GFP_HIGHMEM,
				PAGE_SIZE, flush_dcache_page,
				zero_the_pages_plz);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(GFP_KERNEL | __GFP_ZERO,
				PAGE_SIZE, flush_dcache_page, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(GFP_KERNEL,
				PAGE_SIZE, flush_dcache_page, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(GFP_KERNEL,
				PAGE_SIZE, flush_dcache_page,
				zero_the_pages_plz);

	pr_err("Testing with large page sizes without flushing individual pages\n");
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(
			GFP_KERNEL | __GFP_HIGHMEM | __GFP_COMP | __GFP_ZERO,
			SZ_64K, NULL, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(
			GFP_KERNEL | __GFP_HIGHMEM | __GFP_COMP,
			SZ_64K, NULL, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(
			GFP_KERNEL | __GFP_COMP | __GFP_ZERO,
			SZ_64K, NULL, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(
			GFP_KERNEL | __GFP_COMP,
			SZ_64K, NULL, NULL);

	pr_err("Testing with large page sizes with flushing individual pages\n");
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(
			GFP_KERNEL | __GFP_HIGHMEM | __GFP_COMP | __GFP_ZERO,
			SZ_64K, flush_dcache_page, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(
			GFP_KERNEL | __GFP_HIGHMEM | __GFP_COMP,
			SZ_64K, flush_dcache_page, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(
			GFP_KERNEL | __GFP_COMP | __GFP_ZERO,
			SZ_64K, flush_dcache_page, NULL);
	for (i = 0; i < 7; ++i)
		DO_TEST_KERNEL_ALLOC(
			GFP_KERNEL | __GFP_COMP,
			SZ_64K, flush_dcache_page, NULL);

	timing_debug_dump_results();
}

static long memory_prof_test_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int ret = 0;
	struct memory_prof_module_data *module_data = file->private_data;
	switch (cmd) {
	case MEMORY_PROF_IOC_CLIENT_CREATE:
	{
		module_data->client = msm_ion_client_create(~0, "memory_prof_module");
		if (IS_ERR_OR_NULL(module_data->client))
			return -EIO;
		break;
	}
	case MEMORY_PROF_IOC_CLIENT_DESTROY:
	{
		ion_client_destroy(module_data->client);
		break;
	}
	case MEMORY_PROF_IOC_TEST_MAP_EXTRA:
	{
		struct memory_prof_map_extra_args args;
		struct ion_handle *handle;
		struct ion_client *client = module_data->client;
		unsigned long iova, buffer_size;

		if (copy_from_user(&args, (void __user *)arg,
					sizeof(struct memory_prof_map_extra_args)))
			return -EFAULT;

		handle = ion_import_dma_buf(client, args.ionfd);
		if (IS_ERR_OR_NULL(handle)) {
			pr_err("Couldn't do ion_import_dma_buf in "
				"MEMORY_PROF_IOC_TEST_MAP_EXTRA\n");
			return -EINVAL;
		}

		ret = ion_map_iommu(client, handle, VIDEO_DOMAIN,
				VIDEO_FIRMWARE_POOL, SZ_8K,
				args.iommu_map_len, &iova, &buffer_size,
				0, 0);

		if (ret) {
			pr_err("Couldn't ion_map_iommu in "
				"MEMORY_PROF_IOC_TEST_MAP_EXTRA\n");
			return ret;
		}
		break;
	}
	case MEMORY_PROF_IOC_TEST_KERNEL_ALLOCS:
	{
		test_kernel_allocs();
		break;
	}
	default:
		pr_info("command not supproted\n");
		ret = -EINVAL;
	}
	return ret;
}

static int memory_prof_test_open(struct inode *inode, struct file *file)
{
	struct memory_prof_module_data *module_data = kzalloc(
		sizeof(struct memory_prof_module_data), GFP_KERNEL);

	if (!module_data)
		return -ENOMEM;

	file->private_data = module_data;

	pr_info("memory_prof test device opened\n");
	return 0;
}

static int memory_prof_test_release(struct inode *inode, struct file *file)
{
	struct memory_prof_module_data *module_data = file->private_data;
	kfree(module_data);
	pr_info("memory_prof test device closed\n");
	return 0;
}

static const struct file_operations memory_prof_test_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = memory_prof_test_ioctl,
	.open = memory_prof_test_open,
	.release = memory_prof_test_release,
};

static int memory_prof_device_create(void)
{
	int rc = 0;

	memory_prof_major = register_chrdev(0, MEMORY_PROF_DEV_NAME,
					&memory_prof_test_fops);
	if (memory_prof_major < 0) {
		rc = memory_prof_major;
		pr_err("Unable to register chrdev: %d\n", memory_prof_major);
		goto out;
	}

	memory_prof_class = class_create(THIS_MODULE, MEMORY_PROF_DEV_NAME);
	if (IS_ERR(memory_prof_class)) {
		rc = PTR_ERR(memory_prof_class);
		pr_err("Unable to create class: %d\n", rc);
		goto err_create_class;
	}

	memory_prof_dev = device_create(memory_prof_class, NULL,
					MKDEV(memory_prof_major, 0),
					NULL, MEMORY_PROF_DEV_NAME);
	if (IS_ERR(memory_prof_dev)) {
		rc = PTR_ERR(memory_prof_dev);
		pr_err("Unable to create device: %d\n", rc);
		goto err_create_device;
	}
	return rc;

err_create_device:
	class_destroy(memory_prof_class);
err_create_class:
	unregister_chrdev(memory_prof_major, MEMORY_PROF_DEV_NAME);
out:
	return rc;
}

static void memory_prof_device_destroy(void)
{
	device_destroy(memory_prof_class, MKDEV(memory_prof_major, 0));
	class_destroy(memory_prof_class);
	unregister_chrdev(memory_prof_major, MEMORY_PROF_DEV_NAME);
}

static int memory_prof_test_init(void)
{
	return memory_prof_device_create();
}

static void memory_prof_test_exit(void)
{
	return memory_prof_device_destroy();
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Profile memory stuff");
module_init(memory_prof_test_init);
module_exit(memory_prof_test_exit);
