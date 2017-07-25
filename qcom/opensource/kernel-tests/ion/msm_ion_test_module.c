/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/msm_ion.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include "iontest.h"

#define ION_TEST_DEV_NAME "msm_ion_test"
#define CLIENT_NAME "ion_test_client"

struct msm_ion_test {
	struct ion_client *ion_client;
	struct ion_handle *ion_handle;
	struct ion_test_data test_data;
};

struct heap {
	unsigned int id;
	unsigned int size;
	enum ion_test_heap_type type;
};

static struct class *ion_test_class;
static int ion_test_major;
static struct device *ion_test_dev;
static struct heap *heap_list;
static unsigned int heap_list_len;

/*Utility apis*/
static inline int create_ion_client(struct msm_ion_test *ion_test)
{
	ion_test->ion_client = msm_ion_client_create(UINT_MAX, CLIENT_NAME);
	if (IS_ERR_OR_NULL(ion_test->ion_client))
		return -EIO;
	return 0;
}

static inline void free_ion_client(struct msm_ion_test *ion_test)
{
	ion_client_destroy(ion_test->ion_client);
}

static int alloc_ion_buf(struct msm_ion_test *ion_test,
					struct ion_test_data *test_data)
{
	ion_test->ion_handle = ion_alloc(ion_test->ion_client, test_data->size,
					test_data->align, test_data->heap_mask,
					test_data->flags);
	if (IS_ERR_OR_NULL(ion_test->ion_handle))
		return -EIO;
	return 0;
}

static inline void free_ion_buf(struct msm_ion_test *ion_test)
{
	ion_free(ion_test->ion_client, ion_test->ion_handle);
}

static int heap_detected;

#ifdef CONFIG_OF_DEVICE
static int ion_find_heaps_available(void)
{
	struct device_node *ion_node;
	struct device_node *ion_child;
	unsigned int i = 0;
	int ret = 0;
	unsigned int val = 0;

	ion_node = of_find_compatible_node(NULL, NULL, "qcom,msm-ion");

	for_each_child_of_node(ion_node, ion_child)
		heap_list_len++;

	heap_list = kzalloc(sizeof(struct heap) * heap_list_len, GFP_KERNEL);
	if (!heap_list) {
		ret = -ENOMEM;
		goto out;
	}

	for_each_child_of_node(ion_node, ion_child) {
		ret = of_property_read_u32(ion_child, "reg", &val);
		if (ret) {
			pr_err("%s: Unable to find reg key", __func__);
			goto free_heaps;
		}
		heap_list[i].id = val;

		switch (val) {
			case ION_SYSTEM_HEAP_ID:
			{
				heap_list[i].type = SYSTEM_MEM;
				break;
			}
			case ION_SYSTEM_CONTIG_HEAP_ID:
			{
				heap_list[i].type = SYSTEM_CONTIG;
				break;
			}
			case ION_CP_MM_HEAP_ID:
			{
				ret = of_property_read_u32(ion_child,
						"qcom,memory-reservation-size", &val);
				if (ret) {
					heap_list[i].type = CP;
				} else {
					heap_list[i].size = val;
					heap_list[i].type = CP_CARVEOUT;
				}
				break;
			}
			case ION_QSECOM_HEAP_ID:
			case ION_AUDIO_HEAP_ID:
			{
				ret = of_property_read_u32(ion_child,
						"qcom,memory-reservation-size", &val);
				if (ret) {
					heap_list[i].type = 0;
				} else {
					heap_list[i].size = val;
					heap_list[i].type = CARVEOUT;
				}
				break;
			}
			default:
				break;
		}
		++i;
	}
	heap_detected = 1;
	goto out;

free_heaps:
	kfree(heap_list);
	heap_list = 0;
out:
	return ret;
}
#else
static int ion_find_heaps_available(void)
{
	return 0;
}
#endif

static int ion_test_open(struct inode *inode, struct file *file)
{
	struct msm_ion_test *ion_test = kzalloc(sizeof(struct msm_ion_test),
								GFP_KERNEL);
	if (!ion_test)
		return -ENOMEM;
	pr_debug("ion test device opened\n");
	file->private_data = ion_test;

	return ion_find_heaps_available();
}

static int get_proper_heap(struct ion_heap_data *data)
{
	int ret = -EINVAL;
	unsigned int i;

	if (!heap_detected) {
		data->valid = 1;
		ret = 0;
	} else {
		data->heap_mask  = 0;
		data->size = 0;
		data->valid = 0;

		for (i = 0; i < heap_list_len; ++i) {
			if (data->type == heap_list[i].type) {
				data->size = heap_list[i].size;
				data->heap_mask = ION_HEAP(heap_list[i].id);
				data->valid = 1;
				ret = 0;
				break;
			}
		}
	}
	return ret;
}

static long ion_test_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int ret;
	ion_phys_addr_t phys_addr;
	void *addr;
	size_t len;
	unsigned long flags, size;
	struct msm_ion_test *ion_test = file->private_data;
	struct ion_test_data *test_data = &ion_test->test_data;

	switch (cmd) {
	case IOC_ION_KCLIENT_CREATE:
	{
		ret = create_ion_client(ion_test);
		break;
	}
	case IOC_ION_KCLIENT_DESTROY:
	{
		free_ion_client(ion_test);
		ret = 0;
		break;
	}
	case IOC_ION_KALLOC:
	{
		if (copy_from_user(test_data, (void __user *)arg,
						sizeof(struct ion_test_data)))
			return -EFAULT;
		ret = alloc_ion_buf(ion_test, test_data);
		if (ret)
			pr_info("allocating ion buffer failed\n");
		break;
	}
	case IOC_ION_KFREE:
	{
		free_ion_buf(ion_test);
		ret = 0;
		break;
	}
	case IOC_ION_KPHYS:
	{
		ret = ion_phys(ion_test->ion_client, ion_test->ion_handle,
							&phys_addr, &len);
		if (!ret)
			pr_info("size is 0x%x\n phys addr 0x%x", len,
						(unsigned int)phys_addr);
		break;
	}
	case IOC_ION_KMAP:
	{
		addr = ion_map_kernel(ion_test->ion_client,
					ion_test->ion_handle);
		if (IS_ERR_OR_NULL(addr)) {
			ret = -EIO;
			pr_info("mapping kernel buffer failed\n");
		} else {
			ret = 0;
			test_data->vaddr = (unsigned long)addr;
		}
		break;
	}
	case IOC_ION_KUMAP:
	{
		ion_unmap_kernel(ion_test->ion_client, ion_test->ion_handle);
		ret = 0;
		break;
	}
	case IOC_ION_UIMPORT:
	{
		if (copy_from_user(test_data, (void __user *)arg,
						sizeof(struct ion_test_data)))
			return -EFAULT;
		ion_test->ion_handle = ion_import_dma_buf(ion_test->ion_client,
							test_data->shared_fd);
		if (IS_ERR_OR_NULL(ion_test->ion_handle)) {
			ret = -EIO;
			pr_info("import of user buf failed\n");
		} else
			ret = 0;
		break;
	}
	case IOC_ION_UBUF_FLAGS:
	{
		ret = ion_handle_get_flags(ion_test->ion_client,
						ion_test->ion_handle, &flags);
		if (ret)
			pr_info("user flags cannot be retrieved\n");
		else
			if (copy_to_user((void __user *)arg, &flags,
						sizeof(unsigned long)))
				ret = -EFAULT;
		break;
	}
	case IOC_ION_UBUF_SIZE:
	{
		ret = ion_handle_get_size(ion_test->ion_client,
						ion_test->ion_handle, &size);
		if (ret)
			pr_info("buffer size cannot be retrieved\n");
		else
			if (copy_to_user((void __user *)arg, &size,
							sizeof(unsigned long)))
				ret = -EFAULT;
		break;
	}
	case IOC_ION_WRITE_VERIFY:
	{
		write_pattern(test_data->vaddr, test_data->size);
		if (verify_pattern(test_data->vaddr, test_data->size)) {
			pr_info("verify of mapped buf failed\n");
			ret = -EIO;
		} else
			ret = 0;
		break;
	}
	case IOC_ION_VERIFY:
	{
		if (verify_pattern(test_data->vaddr, test_data->size)) {
			pr_info("fail in verifying imported buffer\n");
			ret = -EIO;
		} else
			ret = 0;
		break;
	}
	case IOC_ION_SEC:
	{
		ret = msm_ion_secure_heap(ION_CP_MM_HEAP_ID);
		if (ret)
			pr_info("unable to secure heap\n");
		else
			pr_info("able to secure heap\n");
		break;
	}
	case IOC_ION_UNSEC:
	{
		ret = msm_ion_unsecure_heap(ION_CP_MM_HEAP_ID);
		if (ret)
			pr_info("unable to unsecure heap\n");
		else
			pr_info("able to unsecure heap\n");
		break;
	}
	case IOC_ION_FIND_PROPER_HEAP:
	{
		struct ion_heap_data data;

		if (copy_from_user(&data, (void __user *)arg,
						sizeof(struct ion_heap_data)))
			return -EFAULT;
		ret = get_proper_heap(&data);
		if (!ret) {
			if (copy_to_user((void __user *)arg, &data,
						sizeof(struct ion_heap_data)))
				ret = -EFAULT;
		}
		break;
	}
	default:
	{
		pr_info("command not supported\n");
		ret = -EINVAL;
	}
	};
	return ret;
}

static int ion_test_release(struct inode *inode, struct file *file)
{
	struct msm_ion_test *ion_test = file->private_data;
	pr_debug("ion test device closed\n");
	kfree(ion_test);
	return 0;
}

/*
 * Register ourselves as a device to be able to test the ion code
 * from userspace.
 */

static const struct file_operations ion_test_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ion_test_ioctl,
	.open = ion_test_open,
	.release = ion_test_release,
};

static int ion_test_device_create(void)
{
	int ret_val = 0;

	ion_test_major = register_chrdev(0, ION_TEST_DEV_NAME, &ion_test_fops);
	if (ion_test_major < 0) {
		pr_err("Unable to register chrdev: %d\n", ion_test_major);
		ret_val = ion_test_major;
		goto out;
	}

	ion_test_class = class_create(THIS_MODULE, ION_TEST_DEV_NAME);
	if (IS_ERR(ion_test_class)) {
		ret_val = PTR_ERR(ion_test_class);
		pr_err("Unable to create class: %d\n", ret_val);
		goto err_create_class;
	}

	ion_test_dev = device_create(ion_test_class, NULL, MKDEV(ion_test_major, 0), NULL, ION_TEST_DEV_NAME);
	if (IS_ERR(ion_test_dev)) {
		ret_val = PTR_ERR(ion_test_dev);
		pr_err("Unable to create device: %d\n", ret_val);
		goto err_create_device;
	}
	goto out;

err_create_device:
	class_destroy(ion_test_class);
err_create_class:
	unregister_chrdev(ion_test_major, ION_TEST_DEV_NAME);
out:
	return ret_val;
}

static void ion_test_device_destroy(void)
{
	device_destroy(ion_test_class, MKDEV(ion_test_major, 0));
	class_destroy(ion_test_class);
	unregister_chrdev(ion_test_major, ION_TEST_DEV_NAME);
	kfree(heap_list);
}

static int ion_test_init(void)
{
	return ion_test_device_create();
}

static void ion_test_exit(void)
{
	return ion_test_device_destroy();
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Test for MSM ION implementation");
module_init(ion_test_init);
module_exit(ion_test_exit);
