/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#define AHB_BUS_TIMEOUT	0xFD4A0800
#define SEC_WDOG	0xFC4AA000
#define WDT0_RST	0x04
#define WDT0_EN		0x08

struct clk_pair {
	const char *dev;
	const char *clk;
};

static int bus_timeout_camera;
static int bus_timeout_usb;
static int pc_save;

static int bus_timeout_camera_set(const char *val, struct kernel_param *kp);
module_param_call(bus_timeout_camera, bus_timeout_camera_set, param_get_int,
						&bus_timeout_camera, 0644);

static int bus_timeout_usb_set(const char *val, struct kernel_param *kp);
module_param_call(bus_timeout_usb, bus_timeout_usb_set, param_get_int,
						&bus_timeout_usb, 0644);

static int pc_save_set(const char *val, struct kernel_param *kp);
module_param_call(pc_save, pc_save_set, param_get_int, &pc_save, 0644);

static struct clk_pair bus_timeout_camera_clocks_on[] = {
	/*
	 * gcc_mmss_noc_cfg_ahb_clk should be on but right
	 * now this clock is on by default and not accessable.
	 * Update this table if gcc_mmss_noc_cfg_ahb_clk is
	 * ever not enabled by default!
	 */
	{
		.dev = "fda0c000.qcom,cci",
		.clk = "camss_top_ahb_clk",
	},
	{
		.dev = "fda10000.qcom,vfe",
		.clk = "iface_clk",
	},
};

static struct clk_pair bus_timeout_camera_clocks_off[] = {
	{
		.dev = "fda10000.qcom,vfe",
		.clk = "camss_vfe_vfe_clk",
	}
};

static struct clk_pair bus_timeout_usb_clocks_on[] = {
	{
		.dev = "f9a55000.usb",
		.clk = "iface_clk",
	},
	{
		.dev = "f9a55000.usb",
		.clk = "core_clk",
	},
	{
		.dev = "msm_otg",
		.clk = "iface_clk",
	},
	{
		.dev = "msm_otg",
		.clk = "core_clk",
	},
};

static struct clk_pair bus_timeout_usb_clocks_off[] = {
	{
		.dev = "f9a55000.usb",
		.clk = "core_clk",
	},
	{
		.dev = "msm_otg",
		.clk = "core_clk",
	},
};

static void bus_timeout_clk_access(struct clk_pair bus_timeout_clocks_off[],
				struct clk_pair bus_timeout_clocks_on[],
				int off_size, int on_size)
{
	int i;

	/*
	 * Yes, none of this cleans up properly but the goal here
	 * is to trigger a hang which is going to kill the rest of
	 * the system anyway
	 */

	for (i = 0; i < on_size; i++) {
		struct clk *this_clock;

		this_clock = clk_get_sys(bus_timeout_clocks_on[i].dev,
					bus_timeout_clocks_on[i].clk);
		if (!IS_ERR(this_clock))
			if (clk_prepare_enable(this_clock))
				pr_warn("Device %s: Clock %s not enabled",
					bus_timeout_clocks_on[i].clk,
					bus_timeout_clocks_on[i].dev);
	}

	for (i = 0; i < off_size; i++) {
		struct clk *this_clock;

		this_clock = clk_get_sys(bus_timeout_clocks_off[i].dev,
					 bus_timeout_clocks_off[i].clk);
		if (!IS_ERR(this_clock))
			clk_disable_unprepare(this_clock);
	}
}

static int bus_timeout_camera_set(const char *val, struct kernel_param *kp)
{
	int ret;
	uint32_t dummy;
	uint32_t address = 0xfda10000;
	void *hang_addr;
	struct regulator *r;

	ret = param_set_int(val, kp);
	if (ret)
		return ret;
	if (bus_timeout_camera != 1)
		return -EPERM;

	hang_addr = ioremap(address, SZ_4K);
	r = regulator_get(NULL, "gdsc_vfe");
	ret = IS_ERR(r);
	if (!ret) {
		ret = regulator_enable(r);
		if (ret) {
			pr_err("Bus timeout test: regulator failed to enable\n");
			return ret;
		}
	} else {
		pr_err("Bus timeout test: Unable to get regulator reference\n");
		return ret;
	}
	bus_timeout_clk_access(bus_timeout_camera_clocks_off,
				bus_timeout_camera_clocks_on,
				ARRAY_SIZE(bus_timeout_camera_clocks_off),
				ARRAY_SIZE(bus_timeout_camera_clocks_on));
	dummy = readl_relaxed(hang_addr);
	mdelay(15000);
	pr_err("Bus timeout test failed\n");
	iounmap(hang_addr);
	return -EIO;
}

static int bus_timeout_usb_set(const char *val, struct kernel_param *kp)
{
	int ret;
	uint32_t dummy;
	uint32_t address = 0xf9a55000;
	void *hang_addr;

	ret = param_set_int(val, kp);
	if (ret)
		return ret;
	if (bus_timeout_usb != 1)
		return -EPERM;

	hang_addr = ioremap(address, SZ_4K);
	bus_timeout_clk_access(bus_timeout_usb_clocks_off,
				bus_timeout_usb_clocks_on,
				ARRAY_SIZE(bus_timeout_usb_clocks_off),
				ARRAY_SIZE(bus_timeout_usb_clocks_on));
	dummy = readl_relaxed(hang_addr);
	mdelay(15000);
	pr_err("Bus timeout test failed: 0x%x\n", dummy);
	iounmap(hang_addr);
	return -EIO;
}

static const struct of_device_id apps_wdog_of_match[] = {
	{ .compatible	= "qcom,msm-watchdog" },
	{},
};

static void __iomem *wdogbase;

static int msm_apps_wdog_probe(void)
{
	int ret;
	struct device_node *node;
	uint32_t base;

	node = of_find_matching_node(NULL, apps_wdog_of_match);
	if (node) {
		ret = of_property_read_u32(node, "reg", &base);
		if (ret) {
			pr_err("PC-Save test: Cannot read wdog base\n");
			return ret;
		}
		wdogbase = ioremap(base, SZ_4K);
		if (!wdogbase) {
			pr_err("PC-save test: Cannot map wdog register space\n");
			return -ENOMEM;
		}
	} else {
		pr_info("PC-save test: wdog node not found\n");
		return -EINVAL;
	}
	return 0;
}

static void msm_apps_wdog_disable(void)
{
	__raw_writel(1, wdogbase + WDT0_RST);
	__raw_writel(0, wdogbase + WDT0_EN);
	mb();
}

static int pc_save_set(const char *val, struct kernel_param *kp)
{
	int ret;
	uint32_t dummy;
	uint32_t address = 0xfda10000;
	void *hang_addr;
	void *global_bus_timeout_disable;
	struct regulator *r;
	void *sec_wdog_virt;
	uint32_t sec_status;

	ret = param_set_int(val, kp);
	if (ret)
		return ret;
	if (pc_save != 1)
		return -EPERM;
	ret = msm_apps_wdog_probe();
	if (ret)
		return ret;

	hang_addr = ioremap(address, SZ_4K);
	if (!hang_addr) {
		pr_err("PC-save test: unable to map hang address\n");
		return -ENOMEM;
	}
	global_bus_timeout_disable = ioremap(AHB_BUS_TIMEOUT, SZ_4K);
	if (!global_bus_timeout_disable) {
		pr_err("PC-save test: unable to map bus timeout disable\
					AHB_BUS_TIMEOUT register\n");
		iounmap(hang_addr);
		iounmap(wdogbase);
		return -ENOMEM;
	}
	r = regulator_get(NULL, "gdsc_vfe");
	ret = IS_ERR(r);
	if (!ret) {
		ret = regulator_enable(r);
		if (ret < 0) {
			pr_err("PC-save test: unable to enable regulator\n");
			return ret;
		}
	} else {
		pr_err("PC-save test: Unable to get regulator reference\n");
		return ret;
	}
	bus_timeout_clk_access(bus_timeout_camera_clocks_off,
				bus_timeout_camera_clocks_on,
				ARRAY_SIZE(bus_timeout_camera_clocks_off),
				ARRAY_SIZE(bus_timeout_camera_clocks_on));

	sec_wdog_virt = ioremap(SEC_WDOG, SZ_4K);
	if (!sec_wdog_virt) {
		pr_err("unable to map sec wdog page\n");
		iounmap(hang_addr);
		iounmap(wdogbase);
		iounmap(global_bus_timeout_disable);
		return -ENOMEM;
	}

	/* Disable sec watchdog */
	writel_relaxed(0x1, sec_wdog_virt);
	sec_status = readl_relaxed(sec_wdog_virt + 0x04);
	writel_relaxed(sec_status & ~0x1, (sec_wdog_virt + 0x04));
	/* Make sure sec watchdog is disabled before continuing */
	mb();

	/* Disable apps watchdog */
	msm_apps_wdog_disable();

	/* Set sec watchdog bite time < bark time */
	writel_relaxed(0x7FFFF, (sec_wdog_virt + 0x0C));
	/* 1ms bite time in sleep clock cycles */
	writel_relaxed(0x20, (sec_wdog_virt + 0x10));
	writel_relaxed(sec_status | 0x1, (sec_wdog_virt + 0x04));
	/* Make sure sec watchdog is enabled before continuing */
	mb();

	/* Disable Bus timeout */
	writel_relaxed(0x0, global_bus_timeout_disable);
	/* Make sure bus timeout is disabled before continuing */
	mb();

	dummy = readl_relaxed(hang_addr);

	mdelay(15000);
	pr_err("Bus timeout test failed\n");
	iounmap(hang_addr);
	iounmap(wdogbase);
	iounmap(global_bus_timeout_disable);
	iounmap(sec_wdog_virt);
	return -EIO;
}

static int bus_timeout_init(void)
{
	return 0;
}

static void bus_timeout_exit(void)
{
}

module_init(bus_timeout_init);
module_exit(bus_timeout_exit);
MODULE_LICENSE("GPL v2");
