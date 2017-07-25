/* Copyright (c) 2013, Linux Foundation. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/coresight-stm.h>
#include <linux/time.h>
#include <linux/spinlock.h>

static DEFINE_SPINLOCK(spinlock);
static int stm_profile;

static int stm_profile_set(const char *val, struct kernel_param *kp);
module_param_call(stm_profile, stm_profile_set, param_get_int,
		  &stm_profile, 0644);

static int stm_profile_set(const char *val, struct kernel_param *kp)
{
	int i, ret;
	char data[8] = "12345678";
	struct timespec before, after, ts;
	unsigned long flags;

	ret = param_set_int(val, kp);
	if (ret)
		return ret;
	if (stm_profile != 1)
		return -EINVAL;

	spin_lock_irqsave(&spinlock, flags);
	getnstimeofday(&before);

	for(i = 0; i < 512; i++)
		stm_log(0, data, sizeof(data));

	getnstimeofday(&after);
	spin_unlock_irqrestore(&spinlock, flags);

	ts = timespec_sub(after, before);
	printk(KERN_ALERT "\nTime taken to log 4KB ofdata over STM: %lu.%09lu"
	       "sec\n",(unsigned long)ts.tv_sec, (unsigned long)ts.tv_nsec);
	return 0;

}

static int cs_profile_init(void)
{
	return 0;
}
module_init(cs_profile_init);

static void cs_profile_exit(void)
{
	return;
}
module_exit(cs_profile_exit);

MODULE_LICENSE("GPL v2");
