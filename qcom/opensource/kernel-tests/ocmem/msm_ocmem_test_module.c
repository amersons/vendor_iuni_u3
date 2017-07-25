/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/memory.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <mach/irqs.h>
#include <mach/ocmem.h>


#define OCMEM_KERNEL_TEST_MAGIC 0xc1

#define OCMEM_TEST_TYPE_NOMINAL \
	_IO(OCMEM_KERNEL_TEST_MAGIC, 1)
#define OCMEM_TEST_TYPE_ADVERSARIAL \
	_IO(OCMEM_KERNEL_TEST_MAGIC, 2)
#define OCMEM_TEST_TYPE_STRESS \
	_IO(OCMEM_KERNEL_TEST_MAGIC, 3)
#define OCMEM_TEST_VERBOSE_MODE \
	_IO(OCMEM_KERNEL_TEST_MAGIC, 4)
#define OCMEM_TEST_DEBUG_MODE \
	_IO(OCMEM_KERNEL_TEST_MAGIC, 5)

#define TEST_OCMEM_CLIENT OCMEM_GRAPHICS

/* "OCMM" */
#define OCMEM_READ_WRITE_MAGIC 0xA110CA7E
#define OCMEM_MAX_SIZE 0x180000
#define OCMEM_BASE_ADDR 0xFEC00000

static int debug_mode;
static int verbose_mode;

static int num_test_cases;
static int num_success;
static unsigned long test_ocmem_client_quota;

#define OCMEM_LOG(msg, args...) do {					\
			if (verbose_mode)	\
				pr_info(msg, ##args);	\
			else	\
				pr_debug(msg, ##args);	\
		} while (0)

static inline int ocmem_phys_address(unsigned long addr)
{
	if (addr <= OCMEM_MAX_SIZE)
		addr += OCMEM_BASE_ADDR;
	return addr;
}

static int test_client_notif_handler(struct notifier_block *this,
		unsigned long event, void *data)
{
	struct ocmem_buf *buff = data;

	OCMEM_LOG("ocmem_test: test_client notitification event %lu\n", event);
	OCMEM_LOG("Notification buffer buff %p(addr %lx:len %lx)\n",
			buff, buff->addr, buff->len);
	return NOTIFY_DONE;
}

static struct notifier_block test_client_nb = {
	.notifier_call = test_client_notif_handler,
};

#ifdef CONFIG_MSM_OCMEM_NONSECURE
static int ocmem_verify_access(int id, struct ocmem_buf *buffer)
{
	unsigned long start;
	unsigned long sz = buffer->len;
	unsigned i = 0;
	unsigned failures = 0;
	void *vstart = NULL;
	unsigned *ptr = NULL;

	start = ocmem_phys_address(buffer->addr);

	OCMEM_LOG("io-remapping addr %lx and len %lx\n", start, sz);

	vstart = ioremap_nocache(start, sz);

	if (!vstart) {
		pr_err("ocmem test: ioremap failed\n");
		return -ENOMEM;
	}

	ptr = (unsigned *)vstart;

	for (i = 0; i < sz/(sizeof(int)); i++) {
		ptr[i] = OCMEM_READ_WRITE_MAGIC;
		/* Ensure that the value was written out to OCMEM memory */
		mb();
		if (ptr[i] != OCMEM_READ_WRITE_MAGIC) {
			pr_err("ocmem test: verify access failed for client %d\n",
					id);
			failures++;
		}
	}

	OCMEM_LOG("read write test for memory range %lx to %lx completed\n",
			start, start + sz - 1);

	iounmap(vstart);
	return failures;
}
#else
static int ocmem_verify_access(int id, struct ocmem_buf *buffer)
{
	OCMEM_LOG("Secure mode, skipping read/write verification\n");
	return 0;
}
#endif


/*
 * Test Case: Single Allocation Test
 * Invokes: Synchronous allocation call
 * Verifies: A buffer of "size" is allocated when there is no contention
 */

static int ocmem_test_single_alloc(void)
{
	struct ocmem_buf *buff = NULL;
	unsigned long size = test_ocmem_client_quota;
	num_test_cases++;

	buff = ocmem_allocate(TEST_OCMEM_CLIENT, test_ocmem_client_quota);

	if (!buff || !buff->len)
		return -EINVAL;

	OCMEM_LOG("ocmem test: single_alloc: buff %p(addr:%lx len:%lx)\n",
					buff, buff->addr, buff->len);

	if (buff->len != size)
		return -EINVAL;

	ocmem_verify_access(TEST_OCMEM_CLIENT, buff);

	if (ocmem_free(TEST_OCMEM_CLIENT, buff))
		return -EINVAL;

	OCMEM_LOG("ocmem test: single_alloc succeeded\n");
	return 0;
}

/*
 * Test Case: Single Allocation No Wait Test
 * Invokes: Synchronous no wait allocation call
 * Verifies: A buffer of "size" is allocated when there is no contention
 */
static int ocmem_test_single_alloc_nowait(void)
{
	struct ocmem_buf *buff = NULL;
	unsigned long size = test_ocmem_client_quota;

	num_test_cases++;

	buff = ocmem_allocate_nowait(TEST_OCMEM_CLIENT, size);

	if (!buff || !buff->len)
		return -EINVAL;

	OCMEM_LOG("ocmem test: single_alloc_nw: buff %p(addr:%lx len:%lx)\n",
					buff, buff->addr, buff->len);

	if (buff->len != size)
		return -EINVAL;

	ocmem_verify_access(TEST_OCMEM_CLIENT, buff);

	if (ocmem_free(TEST_OCMEM_CLIENT, buff))
		return -EINVAL;

	OCMEM_LOG("ocmem test: single_alloc_nw succeeded\n");
	num_success++;
	return 0;
}

/*
 * Test Case: Single Allocation Range Test
 * Invokes: Range allocation call
 * Verifies: A buffer of size "max" is allocated when there is no contention
 */
static int ocmem_test_single_alloc_range(void)
{
	struct ocmem_buf *buff = NULL;
	void *test_client_hndl = NULL;
	unsigned long min = test_ocmem_client_quota / 2;
	unsigned long max = test_ocmem_client_quota;
	unsigned long step = min;

	num_test_cases++;

	test_client_hndl = ocmem_notifier_register(TEST_OCMEM_CLIENT, &test_client_nb);

	if (!test_client_hndl)
		goto notifier_fail;

	buff = ocmem_allocate_range(TEST_OCMEM_CLIENT, min, max, step);

	if (!buff || !buff->len)
		goto test_case_fail;

	OCMEM_LOG("ocmem test: single_alloc_range: buff %p(addr:%lx len:%lx)\n",
					buff, buff->addr, buff->len);

	if (buff->len != max)
		goto test_case_fail;

	if (ocmem_free(TEST_OCMEM_CLIENT, buff))
		goto test_case_fail;

	if (ocmem_notifier_unregister(test_client_hndl, &test_client_nb))
		return -EINVAL;

	num_success++;
	OCMEM_LOG("ocmem test: single_alloc_range succeeded\n");
	return 0;

test_case_fail:
	if (ocmem_notifier_unregister(test_client_hndl, &test_client_nb))
		return -EINVAL;
notifier_fail:
	return -EINVAL;
}

/*
 * Test Case: Single Allocation Non blocking Test
 * Invokes: Non blocking allocation call
 * Verifies: A buffer of "size" is immediately allocated during no contention
 */
static int ocmem_test_single_alloc_nb(void)
{
	struct ocmem_buf *buff = NULL;
	void *test_client_hndl;
	unsigned long size = test_ocmem_client_quota;

	num_test_cases++;

	test_client_hndl = ocmem_notifier_register(TEST_OCMEM_CLIENT, &test_client_nb);

	if (!test_client_hndl)
		goto notifier_fail;

	buff = ocmem_allocate_nb(TEST_OCMEM_CLIENT, size);

	if (!buff || !buff->len)
		goto test_case_fail;

	OCMEM_LOG("ocmem test: single_alloc_nb: buff %p(addr: %lx len:%lx)\n",
					buff, buff->addr, buff->len);

	if (buff->len != size)
		goto test_case_fail;

	if (ocmem_free(TEST_OCMEM_CLIENT, buff))
		goto test_case_fail;

	if (ocmem_notifier_unregister(test_client_hndl, &test_client_nb))
		return -EINVAL;

	num_success++;
	OCMEM_LOG("ocmem test: single_alloc_nb succeeded\n");
	return 0;

test_case_fail:
	if (ocmem_notifier_unregister(test_client_hndl, &test_client_nb))
		return -EINVAL;
notifier_fail:
	return -EINVAL;
}

static int (*nominal_test_cases[]) (void) = {
	ocmem_test_single_alloc,
	ocmem_test_single_alloc_nowait,
	ocmem_test_single_alloc_nb,
	ocmem_test_single_alloc_range,
};

static int msm_ocmem_nominal_test(void)
{
	unsigned errors = 0;
	int rc = 0;
	unsigned i = 0;

	OCMEM_LOG("OCMEM test: Nominal test suite invoked\n");

	for (i = 0; i < ARRAY_SIZE(nominal_test_cases); i++) {
		OCMEM_LOG("Nominal test case %d\n", i);
		rc = nominal_test_cases[i]();
		if (rc < 0) {
			pr_err("ocmem test: nominal: test case %d failed\n", i);
			errors++;
			/* Bail out on first failure if debug mode is not set */
			if (!debug_mode)
				return -EINVAL;
		}

	}
	OCMEM_LOG("OCMEM test: Nominal test completed with %d errors\n",
				errors);
	return errors;
}


/*
 * Test Case: Allocation Denied Test
 * Invokes: Synchronous allocation call
 * Verifies: A subystem that is not allowed OCMEM memory must be denied alloc
 */
static int ocmem_test_alloc_denied(void)
{
	struct ocmem_buf *buff = NULL;
	unsigned long size = test_ocmem_client_quota;
	num_test_cases++;

	buff = ocmem_allocate(OCMEM_VOICE, size);

	if (buff != NULL)
		return -EINVAL;

	OCMEM_LOG("ocmem test: alloc_denied: NULL buffer returned\n");
	num_success++;
	return 0;
}

static int (*adversarial_test_cases[]) (void) = {
	ocmem_test_alloc_denied,
};

static int msm_ocmem_adversarial_test(void)
{
	unsigned errors = 0;
	unsigned i = 0;
	int rc = 0;

	OCMEM_LOG("OCMEM test: Adversarial test suite invoked\n");

	for (i = 0; i < ARRAY_SIZE(adversarial_test_cases); i++) {
		OCMEM_LOG("Adversarial test case %d\n", i);
		rc = adversarial_test_cases[i]();
		if (rc < 0) {
			pr_err("ocmem test: adversarial: test case %d failed\n",
					i);
			errors++;
			/* Bail out on first failure if debug mode is not set */
			if (!debug_mode)
				return -EINVAL;
		}

	}
	OCMEM_LOG("OCMEM test: Adversarial test completed with %d errors\n",
				errors);
	return errors;
}

static int msm_ocmem_stress_test(void)
{
	OCMEM_LOG("OCMEM test: Stress test suite invoked\n");
	return 0;
}

static long msm_ocmem_test_ioctl(struct file *file, unsigned cmd,
					unsigned long arg)
{
	long retval = 0;
	int err = 0;

	OCMEM_LOG("OCMEM test: ioctl invoked");

	/* Verify user space ioctl arguments */
	if (_IOC_TYPE(cmd) != OCMEM_KERNEL_TEST_MAGIC) {
		pr_err("OCMEM test: Unknown IOCTL type, ignoring\n");
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg,
					_IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg,
					_IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	switch (cmd) {

	case OCMEM_TEST_DEBUG_MODE:
		if (__get_user(debug_mode, (int __user *)arg))
			return -EINVAL;
		if (debug_mode) {
			pr_info("OCMEM test: Enabling debug mode\n");
			pr_info("Continue testing all testcases in case of failure\n");
		}
		break;

	case OCMEM_TEST_VERBOSE_MODE:
		if (__get_user(verbose_mode, (int __user *)arg))
			return -EINVAL;
		if (verbose_mode)
			pr_info("OCMEM test: Verbose logging enabled\n");
		break;

	case OCMEM_TEST_TYPE_NOMINAL:
		retval = msm_ocmem_nominal_test();
		break;

	case OCMEM_TEST_TYPE_ADVERSARIAL:
		retval = msm_ocmem_adversarial_test();
		break;

	case OCMEM_TEST_TYPE_STRESS:
		retval = msm_ocmem_stress_test();
		break;
	default:
		pr_err("OCMEM test: Invalid ioctl\n");
		retval = -EFAULT;
		break;
	}

	return retval;
}


/*
 * Register ourselves as a misc device to be able to test the OCMEM code
 * from userspace.
 */

static const struct file_operations msm_ocmem_test_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = msm_ocmem_test_ioctl,
};

static struct miscdevice msm_ocmem_test_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ocmemtest",
	.fops = &msm_ocmem_test_fops,
};

/*
 * Module Init.
 */
static int __init ocmem_test_init(void)
{
	int ret;

	pr_info("OCMEM test: Init\n");

	ret = misc_register(&msm_ocmem_test_dev);

	if (ret < 0) {
		pr_err("OCMEM test: Failed to register device");
		return -ENODEV;
	}

	test_ocmem_client_quota = get_max_quota(TEST_OCMEM_CLIENT);
	if (!test_ocmem_client_quota) {
		pr_err("Couldn't do get_max_quota for TEST_OCMEM_CLIENT (id: %d)\n",
			TEST_OCMEM_CLIENT);
		ret = -EIO;
		goto fail;
	}

	pr_info("OCMEM test: Module loaded\n");

	ret = 0;

	return ret;
fail:
	misc_deregister(&msm_ocmem_test_dev);
	return ret;
}

/*
 * Module Exit.
 */
static void __exit ocmem_test_exit(void)
{
	pr_info("OCMEM test: Exit\n");
	misc_deregister(&msm_ocmem_test_dev);
	pr_info("OCMEM test: Module unloaded\n");
}

module_init(ocmem_test_init);
module_exit(ocmem_test_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Test for MSM On-Chip Memory");

