/* Copyright (c) 2012, The Linux Foundation. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include "msm_bus_test.h"

static int msm_bus_create_client(unsigned long arg)
{
	struct msm_bus_paths *usecase;
	struct msm_bus_vectors *vectors;
	struct msm_bus_scale_pdata *pdata;
	int i, j;
	struct msm_bus_test_cldata cldata;
	uint32_t clid;

	pdata = kmalloc(sizeof(struct msm_bus_scale_pdata), GFP_KERNEL);
	pr_debug("AXI: %s(): create cl\n", __func__);
	if (__copy_from_user(&cldata, (void __user *)arg,
			sizeof(struct msm_bus_test_cldata))) {
		pr_err("AXI: %s(): Error getting user memory\n", __func__);
		return -EFAULT;
	}

	pdata->name = cldata.pdata.name;
	pdata->active_only = cldata.pdata.active_only;
	pdata->num_usecases = cldata.pdata.num_usecases;
	usecase = kmalloc(((pdata->num_usecases) *
		sizeof(struct msm_bus_paths)), GFP_KERNEL);
	if (IS_ERR(usecase)) {
		pr_err("AXI: %s(): Error creating usecase in test\n", __func__);
		return -ENOMEM;
	}

	pdata->usecase = usecase;
	for (i = 0; i < pdata->num_usecases; i++) {
		pdata->usecase[i].num_paths = cldata.pdata.usecase[i].num_paths;
		vectors = kmalloc((pdata->usecase[i].num_paths *
			sizeof(struct msm_bus_vectors)), GFP_KERNEL);
		if (IS_ERR(vectors)) {
			for (j = 0; j < i; j++)
				kfree(pdata->usecase[j].vectors);

			kfree(pdata->usecase);
			kfree(pdata);
			pr_err("AXI: %s(): Error creating usecase in test\n",
				__func__);
			return -ENOMEM;
		}

		for (j = 0; j < pdata->usecase[i].num_paths; j++) {
			vectors[j].src = cldata.pdata.usecase[i].vectors[j].src;
			vectors[j].dst = cldata.pdata.usecase[i].vectors[j].dst;
			vectors[j].ab = cldata.pdata.usecase[i].vectors[j].ab;
			vectors[j].ib = cldata.pdata.usecase[i].vectors[j].ib;
		}
		pdata->usecase[i].vectors = vectors;
	}

	clid = msm_bus_scale_register_client(pdata);
	pr_debug("AXI: %s(): Got client id: %u\n", __func__, clid);
	cldata.clid = clid;
	cldata.pdatah = (uint32_t)(pdata);
	if (__copy_to_user((void __user *)arg, &cldata,
		sizeof(struct msm_bus_test_cldata))) {
		pr_err("AXI: %s(): Error copying data to client\n", __func__);
		msm_bus_scale_unregister_client(clid);
		for (i = 0; i < pdata->num_usecases; i++)
			kfree(pdata->usecase[i].vectors);

		kfree(pdata->usecase);
		kfree(pdata);
		return -EFAULT;
	}

	return 0;
}

static int msm_bus_ioctl_unreg_cl(unsigned cmd, unsigned long arg)
{
	int retval = 0, i;
	uint32_t clid;
	struct msm_bus_test_cldata cldata;
	struct msm_bus_scale_pdata *pdata;

	if (__copy_from_user(&cldata, (void __user *)arg,
		sizeof(struct msm_bus_test_cldata)))
		retval = -EFAULT;

	clid = cldata.clid;
	pr_debug("AXI: %s(): IOCTL: Unregister cl%u\n", __func__, clid);
	msm_bus_scale_unregister_client(clid);
	pdata = (struct msm_bus_scale_pdata *)cldata.pdatah;
	for (i = 0; i < pdata->num_usecases; i++)
		kfree(pdata->usecase[i].vectors);

	kfree(pdata->usecase);
	kfree(pdata);
	return retval;
}

static int msm_bus_ioctl_update_req(unsigned cmd, unsigned long arg)
{
	int retval = 0;
	struct msm_bus_test_update_req_data rdata;

	if (__copy_from_user(&rdata, (void __user *)arg,
		sizeof(rdata))) {
		retval = -EFAULT;
		goto err;
	}

	pr_debug("AXI: %s(): IOCTL: Update req for cl: %u, index: %d\n",
		__func__, rdata.clid, rdata.index);
	retval = msm_bus_scale_client_update_request(rdata.clid,
		rdata.index);
err:
	return retval;
}

static long msm_bus_test_ioctl(struct file *file, unsigned cmd,
	unsigned long arg)
{
	long retval = 0;
	pr_debug("AXI: %s(): entering test ioctl\n", __func__);

	if (_IOC_TYPE(cmd) != MSM_BUS_TEST_IOC_MAGIC) {
		pr_err("AXI: %s(): Wrong IOC_MAGIC.Exiting\n", __func__);
		return -ENOTTY;
	}

	switch (cmd) {
	case MSM_BUS_TEST_REG_CL:
		retval = msm_bus_create_client(arg);
		break;

	case MSM_BUS_TEST_UNREG_CL:
		retval = msm_bus_ioctl_unreg_cl(cmd, arg);
		break;

	case MSM_BUS_TEST_UPDATE_REQ:
		retval = msm_bus_ioctl_update_req(cmd, arg);
		break;
	default:
		pr_err("AXI: %s(): IOCTL: Invalid case\n", __func__);
		retval = -EFAULT;
		break;
	}

	return retval;
}

static const struct file_operations msm_bus_test_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = msm_bus_test_ioctl,
};

static struct miscdevice msm_bus_test_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msmbustest",
	.fops = &msm_bus_test_fops,
};

static int msm_bus_test_init(void)
{
	int ret;

	pr_debug("AXI: %s(): Initializing ioctl module\n", __func__);
	ret = misc_register(&msm_bus_test_dev);
	if (ret < 0)
		return ret;

	return 0;
}

static void msm_bus_test_exit(void)
{
	pr_debug("AXI: %s(): Exiting ioctl module\n", __func__);
	misc_deregister(&msm_bus_test_dev);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Test for MSM Bus Scaling driver");
MODULE_VERSION("1.00");

module_init(msm_bus_test_init);
module_exit(msm_bus_test_exit);
