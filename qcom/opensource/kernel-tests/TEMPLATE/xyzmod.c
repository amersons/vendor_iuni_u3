/* Copyright (c) 2012, The Linux Foundation. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * A trivial example kernel module.
 */
#include <linux/init.h>
#include <linux/module.h>

static int template_init(void)
{
	printk(KERN_ALERT "Hello, world!\n");
	return 0;
}

static void template_exit(void)
{
	printk(KERN_ALERT "Goodbye, cruel world.\n");
}

module_init(template_init);
module_exit(template_exit);
MODULE_LICENSE("GPL v2");
