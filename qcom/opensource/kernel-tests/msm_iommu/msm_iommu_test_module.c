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

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/iommu.h>
#include <linux/vmalloc.h>
#include <asm/sizes.h>
#include <mach/socinfo.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>

#include "iommutest.h"

#define IOMMU_TEST_DEV_NAME "msm_iommu_test"
#define CLIENT_NAME "iommu_test_client"

static struct class *iommu_test_class;
static int iommu_test_major;
static struct device *iommu_test_dev;

#ifdef CONFIG_IOMMU_LPAE
static const unsigned int LPAE_ENABLED = 1;
#else
static const unsigned int LPAE_ENABLED;
#endif

struct iommu_cb {
	const char *name;
	unsigned int secure_context;
};

struct iommu {
	const char *name;
	unsigned int secure;
	unsigned int no_cb;
	struct iommu_cb *cb_list;
};

struct msm_iommu_test {
	unsigned int no_iommu;
	struct iommu *iommu_list;
	unsigned int iommu_rev;
};

static struct iommu_access_ops *iommu_access_ops;

static void msm_iommu_set_access_ops(void)
{
	iommu_access_ops = msm_get_iommu_access_ops();
}

static void free_iommu_list(struct msm_iommu_test *iommu_test)
{
	if (iommu_test->iommu_list) {
		unsigned int i;

		for(i = 0; i < iommu_test->no_iommu; ++i) {
			kfree(iommu_test->iommu_list[i].cb_list);
		}
		kfree(iommu_test->iommu_list);
		iommu_test->iommu_list = 0;
	}
}

#ifdef CONFIG_OF_DEVICE

static int parse_iommu_instance(struct msm_iommu_test *iommu_test,
				struct device_node *iommu_node,
				unsigned int iommu_idx)
{
	struct device_node *iommu_child;
	unsigned int cb_idx = 0;
	int ret = 0;
	unsigned int dummy;
	struct iommu *iommu_inst = &iommu_test->iommu_list[iommu_idx];

	ret = of_property_read_string(iommu_node, "label",
				      &iommu_inst->name);

	if (ret)
		goto out;

	iommu_inst->secure = 0;
	if (!of_property_read_u32(iommu_node, "qcom,iommu-secure-id",&dummy))
		iommu_inst->secure = 1;

	for_each_child_of_node(iommu_node, iommu_child) {
		++iommu_inst->no_cb;
	}

	if (iommu_inst->no_cb > 0) {
		unsigned int sz = sizeof(*(iommu_inst->cb_list)) *
			    iommu_inst->no_cb;

		iommu_inst->cb_list = kzalloc(sz, GFP_KERNEL);
		if (!iommu_inst->cb_list) {
			ret = -ENOMEM;
			goto out;
		}
		for_each_child_of_node(iommu_node, iommu_child) {
			ret = of_property_read_string(iommu_child, "label",
				&iommu_inst->cb_list[cb_idx].name);
			if (ret) {
				pr_err("%s: Unable to find label", __func__);
				goto free_mem;
			}

			iommu_inst->cb_list[cb_idx].secure_context =
					of_property_read_bool(iommu_child,
					"qcom,secure-context");

			++cb_idx;
		}
	}
	goto out;

free_mem:
	kfree(iommu_inst->cb_list);
	iommu_inst->cb_list = 0;
out:
	return ret;
}

static int parse_iommu(struct msm_iommu_test *iommu_test, const char *match_str)
{
	struct device_node *iommu_node  = NULL;
	unsigned int i = 0;
	int ret = 0;

	while (i < iommu_test->no_iommu) {
		iommu_node = of_find_compatible_node(iommu_node, NULL,
						     match_str);

		if (!iommu_node)
			break;

		if (of_device_is_available(iommu_node)) {
			ret = parse_iommu_instance(iommu_test, iommu_node, i);
			if (ret) {
				of_node_put(iommu_node);
				goto out;
			}
			++i;
		}
	}
out:
	return ret;
}

static int iommu_find_iommus_available(struct msm_iommu_test *iommu_test)
{
	struct device_node *iommu_node  = NULL;
	int ret = 0;
	const char *match_str = "qcom,msm-smmu-v1";

	iommu_node = of_find_compatible_node(iommu_node, NULL,  match_str);
	while (iommu_node) {
		if (of_device_is_available(iommu_node)) {
			++iommu_test->no_iommu;
			iommu_test->iommu_rev = 1;
		}
		iommu_node = of_find_compatible_node(iommu_node, NULL,
						     match_str);
	}

	if (iommu_test->no_iommu == 0) {
		match_str = "qcom,msm-smmu-v0";

		iommu_node = of_find_compatible_node(iommu_node, NULL,
						     match_str);
		while (iommu_node) {
			if (of_device_is_available(iommu_node)) {
				++iommu_test->no_iommu;
				iommu_test->iommu_rev = 0;
			}
			iommu_node = of_find_compatible_node(iommu_node, NULL,
							     match_str);
		}
	}

	if (iommu_test->no_iommu > 0) {
		const unsigned int sz = sizeof(*iommu_test->iommu_list) *
					iommu_test->no_iommu;

		iommu_test->iommu_list = kzalloc(sz, GFP_KERNEL);
		if (!iommu_test->iommu_list) {
			ret = -ENOMEM;
			goto out;
		}
		ret = parse_iommu(iommu_test, match_str);
		if (ret)
			goto free_mem;

	} else {
		pr_debug("No IOMMUs found in target\n");
	}
	goto out;

free_mem:
	free_iommu_list(iommu_test);
out:
	return ret;
}

static int get_target(struct target_struct *target)
{
	struct device_node *root;
	int rc = 0;
	const char *comp;

	root = of_find_node_by_path("/");
	rc = of_property_read_string(root, "compatible", &comp);
	if (!rc)
		strlcpy(target->name, &comp[strlen("qcom,")], 8);
	of_node_put(root);
	return rc;
}

#else
static int iommu_find_iommus_available(struct msm_iommu_test *iommu_test)
{
	return 0;
}

static int get_target(struct target_struct *target)
{
	strlcpy(target->name, "Unsupported", sizeof(target->name));
	return 0;
}

#endif

static int iommu_test_open(struct inode *inode, struct file *file)
{
	struct msm_iommu_test *iommu_test = kzalloc(sizeof(*iommu_test),
						    GFP_KERNEL);
	int ret = 0;

	if (!iommu_test)
		return -ENOMEM;
	pr_debug("iommu test device opened\n");
	file->private_data = iommu_test;

	ret = iommu_find_iommus_available(iommu_test);
	if (!ret)
		msm_iommu_set_access_ops();
	return ret;
}

static int iommu_test_release(struct inode *inode, struct file *file)
{
	struct msm_iommu_test *iommu_test = file->private_data;
	pr_debug("ion test device closed\n");
	free_iommu_list(iommu_test);
	kfree(iommu_test);
	return 0;
}

static void get_next_cb(const struct msm_iommu_test *iommu_test,
			struct get_next_cb *gnc)
{
	gnc->lpae_enabled = LPAE_ENABLED;
	if (gnc->iommu_no < iommu_test->no_iommu) {
		gnc->valid_iommu = 1;
		gnc->iommu_secure =
				iommu_test->iommu_list[gnc->iommu_no].secure;
		if (gnc->cb_no < iommu_test->iommu_list[gnc->iommu_no].no_cb) {
			gnc->valid_cb = 1;
			strlcpy(gnc->iommu_name,
				iommu_test->iommu_list[gnc->iommu_no].name,
				sizeof(gnc->iommu_name));

			strlcpy(gnc->cb_name,
				iommu_test->iommu_list[gnc->iommu_no].
						cb_list[gnc->cb_no].name,
				sizeof(gnc->cb_name));
			gnc->cb_secure = iommu_test->iommu_list[gnc->iommu_no].
					cb_list[gnc->cb_no].secure_context;
		}
	}
}

static int is_ctx_busy(struct device *dev)
{
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	int ret = 0;

	ctx_drvdata = dev_get_drvdata(dev);
	if (ctx_drvdata)
		ret = (ctx_drvdata->attached_domain) ? 1 : 0;

	return ret;
}

static int create_domain(struct iommu_domain **domain)
{
	int domain_id;

	struct msm_iova_partition partition = {
		.start = 0x1000,
		.size = 0xFFFFF000,
	};
	struct msm_iova_layout layout = {
		.partitions = &partition,
		.npartitions = 1,
		.client_name = "iommu_test",
		.domain_flags = 0,
	};

	domain_id = msm_register_domain(&layout);
	*domain = msm_get_iommu_domain(domain_id);
	return domain_id;
}

static int test_map_phys_range(struct iommu_domain *domain,
			 unsigned long iova,
			 phys_addr_t phys,
			 unsigned long size,
			 int cached)
{
	int ret;
	struct scatterlist *sglist;
	int prot = IOMMU_WRITE | IOMMU_READ;
	prot |= cached ? IOMMU_CACHE : 0;

	sglist = vmalloc(sizeof(*sglist));
	if (!sglist) {
		ret = -ENOMEM;
		pr_err("Unable to allocate memory for sglist: %s\n", __func__);
		goto out;
	}

	sg_init_table(sglist, 1);
	sg_dma_len(sglist) = size;
	sglist->offset = 0;
	sg_dma_address(sglist) = phys;

	ret = iommu_map_range(domain, iova, sglist, size, prot);
	if (ret) {
		pr_err("%s: could not map %lx in domain %p\n",
			__func__, iova, domain);
	}

	vfree(sglist);
out:
	return ret;
}

static int test_map_phys(struct iommu_domain *domain,
			 unsigned long iova,
			 phys_addr_t phys,
			 unsigned long size,
			 int cached)
{
	int ret;
	int prot = IOMMU_WRITE | IOMMU_READ;
	prot |= cached ? IOMMU_CACHE : 0;

	ret = iommu_map(domain, iova, phys, size, prot);
	if (ret) {
		pr_err("%s: could not map %lx in domain %p\n",
			__func__, iova, domain);
	}
	return ret;
}

static int do_VA2PA_HTW(phys_addr_t pa, unsigned int sz, int domain_id,
			struct iommu_domain *domain, const char *ctx_name)
{
	int ret;
	unsigned long iova;
	phys_addr_t result_pa = 0x12345;

	ret = msm_iommu_map_contig_buffer(pa, domain_id, 0,
					  sz, sz, 0, &iova);
	if (ret) {
		pr_err("%s: iommu map failed: %d\n", __func__, ret);
		goto out;
	}

	result_pa = iommu_iova_to_phys(domain, iova);
	if (pa != result_pa) {
		pr_err("%s: VA2PA FAILED for %s: PA: %pa expected PA: %pa\n",
			__func__, ctx_name, &result_pa, &pa);
		ret = -EIO;
	}
	msm_iommu_unmap_contig_buffer(iova, domain_id, 0, sz);
out:
	return ret;

}

/*
 *  We can only try to use addresses greater than 32 bits
 * if LPAE is enabled in the CPU in addition to in the IOMMU
 */
#if defined(CONFIG_IOMMU_LPAE) && defined(CONFIG_ARM_LPAE)
const unsigned int MAP_SIZES[] = {SZ_4K, SZ_64K, SZ_2M, SZ_32M, SZ_1G};
const unsigned int PAGE_LEVEL_SIZES[] = {SZ_2M, SZ_4K};


int do_lpae_VA2PA_HTW(int domain_id, struct iommu_domain *domain,
		      const char *ctx_name)
{
	phys_addr_t pa = 0x100000000ULL;
	int ret;
	unsigned int i;

	for(i = 0; i < ARRAY_SIZE(MAP_SIZES); ++i) {
		ret = do_VA2PA_HTW(pa, MAP_SIZES[i], domain_id, domain,
				   ctx_name);
		if (ret)
			goto out;
	}
out:
	return ret;
}
#else
const unsigned int MAP_SIZES[] = {SZ_4K, SZ_64K, SZ_1M, SZ_16M};
const unsigned int PAGE_LEVEL_SIZES[] = {SZ_1M, SZ_4K};

int do_lpae_VA2PA_HTW(int domain_id, struct iommu_domain *domain,
		      const char *ctx_name)
{
	return 0;
}
#endif

static int do_mapping_test(unsigned int i, unsigned int j, unsigned int k,
			   struct iommu_domain *domain, const char *ctx_name)
{
	int ret;
	phys_addr_t pa;
	phys_addr_t result_pa;
	unsigned int size;
	unsigned int iova;

	iova = SZ_1G * i;
	iova += (SZ_1G-PAGE_LEVEL_SIZES[0]*10) + PAGE_LEVEL_SIZES[0] * j;
	iova += PAGE_LEVEL_SIZES[0] - PAGE_LEVEL_SIZES[1]*10 +
		PAGE_LEVEL_SIZES[1] * k;
	pa = iova;
	size = SZ_64M;

	/* Test using iommu_map_range */
	pr_debug("Test iommu_map_range\n");
	ret = test_map_phys_range(domain, iova, pa, size, 0);
	if (ret) {
		pr_err("%s: Failed to map VA 0x%x to PA %pa using map_range\n",
			__func__, iova, &pa);
		goto out;
	}

	pr_debug("VA2PA map range\n");
	result_pa = iommu_iova_to_phys(domain, iova);
	if (result_pa != pa) {
		pr_err("%s: VA2PA FAILED for %s: PA: %pa expected PA: %pa using map_range\n",
			__func__, ctx_name, &result_pa, &pa);
		ret = -EIO;
	}
	iommu_unmap_range(domain, iova, size);

	if (ret)
		goto out;

	/* Test using iommu_map */
	pr_debug("Test iommu_map\n");
	ret = test_map_phys(domain, iova, pa, size, 0);
	if (ret) {
		pr_err("%s: Failed to map VA 0x%x to PA %pa\n", __func__,
			iova, &pa);
		goto out;
	}

	result_pa = iommu_iova_to_phys(domain, iova);
	if (result_pa != pa) {
		pr_err("%s: VA2PA FAILED for %s: PA: %pa expected PA: %pa\n",
			__func__, ctx_name, &result_pa, &pa);
		ret = -EIO;
	}
	iommu_unmap(domain, iova, size);
out:
	return ret;
}

static int do_map_range_test(struct iommu_domain *domain, const char *ctx_name)
{
	int ret;
	struct sg_table *table;
	struct scatterlist *sg;
	int prot = IOMMU_WRITE | IOMMU_READ;
	unsigned int num_pages_sizes = ARRAY_SIZE(MAP_SIZES);
	unsigned int i;
	unsigned int tot_size = 0;
	unsigned int iova = MAP_SIZES[num_pages_sizes-1];
	unsigned int result_pa;
	phys_addr_t pa;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table) {
		ret = -ENOMEM;
		pr_err("%s: Unable to allocate sgtable\n", __func__);
		goto out;
	}

	ret = sg_alloc_table(table, num_pages_sizes, GFP_KERNEL);
	if (ret) {
		pr_err("%s: Unable to allocate sglist\n", __func__);
		goto free_table;
	}

	pa = MAP_SIZES[num_pages_sizes-1];
	sg = table->sgl;
	for(i = 0; i < num_pages_sizes; ++i) {
		sg_dma_len(sg) = MAP_SIZES[num_pages_sizes-i-1];
		sg->offset = 0;
		sg_dma_address(sg) = pa;
		sg = sg_next(sg);
		tot_size += MAP_SIZES[num_pages_sizes-i-1];
		pa += MAP_SIZES[num_pages_sizes-i-1];
	}

	ret = iommu_map_range(domain, iova, table->sgl, tot_size, prot);
	if (ret) {
		pr_err("%s: could not map 0x%x in domain %p\n",
			__func__, iova, domain);
		goto free_sglist;
	}

	pa = MAP_SIZES[num_pages_sizes-1];
	for(i = 0; i < num_pages_sizes; ++i) {
		result_pa = iommu_iova_to_phys(domain, iova);
		if (pa != result_pa) {
			pr_err("%s: VA2PA FAILED for %s: IOVA: 0x%x PA: %pa expected PA: %pa\n",
				__func__, ctx_name, iova, &result_pa, &pa);
			ret = -EIO;
			break;
		}
		iova += MAP_SIZES[num_pages_sizes-i-1];
		pa += MAP_SIZES[num_pages_sizes-i-1];
	}

	iova = MAP_SIZES[num_pages_sizes-1];
	iommu_unmap_range(domain, iova, tot_size);
free_sglist:
	sg_free_table(table);
free_table:
	kfree(table);
out:
	return ret;
}

static int do_advanced_VA2PA_HTW(int domain_id, struct iommu_domain *domain,
			    const char *ctx_name)
{
	unsigned int i,j,k;
	int ret;

	pr_debug("Do mapping test\n");
	for(i = 0; i < 2; ++i) {
		for(j = 0; j < 20; ++j) {
			for(k = 0; k < 20; ++k) {
				ret = do_mapping_test(i, j, k,
						      domain, ctx_name);
				if (ret)
					goto out;
			}
		}
	}

	pr_debug("Do map range test\n");
	ret = do_map_range_test(domain, ctx_name);

out:
	return ret;
}

static int do_two_VA2PA_HTW(int domain_id, struct iommu_domain *domain,
		const char *ctx_name)
{
	int ret;
	unsigned long iova1;
	unsigned long iova2;
	phys_addr_t pa;

	ret = msm_iommu_map_contig_buffer(0x10000000, domain_id, 0,
					 SZ_4K, SZ_4K, 0, &iova1);
	if (ret) {
		pr_err("%s: iommu map failed: %d\n", __func__, ret);
		goto out;
	}

	pa = iommu_iova_to_phys(domain, iova1);
	if (pa != 0x10000000) {
		pr_err("%s: VA2PA FAILED 1 for %s: PA: %pa\n", __func__,
			ctx_name, &pa);
		ret = -EIO;
		goto unmap_1;
	}

	ret = msm_iommu_map_contig_buffer(0x20000000, domain_id, 0,
					 SZ_4K, SZ_4K, 0, &iova2);
	if (ret) {
		pr_err("%s: iommu map failed: %d\n", __func__, ret);
		goto unmap_1;
	}

	pa = iommu_iova_to_phys(domain, iova2);
	if (pa != 0x20000000) {
		pr_err("%s: VA2PA FAILED 2 for %s: PA: %pa\n", __func__,
			ctx_name, &pa);
		ret = -EIO;
	}

	msm_iommu_unmap_contig_buffer(iova2, domain_id, 0, SZ_4K);
unmap_1:
	msm_iommu_unmap_contig_buffer(iova1, domain_id, 0, SZ_4K);
out:
	return ret;
}

static int do_basic_VA2PA_HTW(int domain_id, struct iommu_domain *domain,
		const char *ctx_name)
{
	int ret;
	int i;

	ret = do_two_VA2PA_HTW(domain_id, domain, ctx_name);
	if (ret)
		goto out;

	ret = do_lpae_VA2PA_HTW(domain_id, domain, ctx_name);
	if (ret) {
		pr_err("%s: VA2PA LPAE HTW FAILED\n", __func__);
		goto out;
	}

	for(i = 0; i < ARRAY_SIZE(MAP_SIZES); ++i) {
		ret = do_VA2PA_HTW(MAP_SIZES[i], MAP_SIZES[i], domain_id,
				   domain, ctx_name);
		if (ret) {
			pr_err("%s: VA2PA HTW for various page sizes FAILED\n",
				__func__);
			goto out;
		}
	}
out:
	return ret;
}

static int test_VA2PA_HTW(const struct msm_iommu_test *iommu_test,
			  struct test_iommu *tst_iommu)
{
	int ret = 0;
	int domain_id;
	struct iommu_domain *domain;
	struct iommu *smmu = &iommu_test->iommu_list[tst_iommu->iommu_no];
	const char *ctx_name = smmu->cb_list[tst_iommu->cb_no].name;
	struct device *dev;

	domain_id = create_domain(&domain);

	dev  = msm_iommu_get_ctx(ctx_name);
	if (IS_ERR_OR_NULL(dev)) {
		pr_err("context %s not found: %ld\n", ctx_name, PTR_ERR(dev));
		ret = -EINVAL;
		goto unreg_dom;
	}

	if (is_ctx_busy(dev)) {
		ret = -EBUSY;
		goto unreg_dom;
	}

	ret = iommu_attach_device(domain, dev);
	if (ret) {
		pr_err("iommu attach failed: %x\n", ret);
		goto unreg_dom;
	}

	pr_debug("Do basic test\n");
	ret = do_basic_VA2PA_HTW(domain_id, domain, ctx_name);
	if (ret) {
		/* Remap -EBUSY. This is used when context bank is busy */
		if (ret == -EBUSY)
			ret = -EINVAL;
		goto detach;
	}

	if (!(tst_iommu->flags & TEST_FLAG_BASIC)) {
		pr_debug("Do advanced test\n");
		ret = do_advanced_VA2PA_HTW(domain_id, domain, ctx_name);
		if (ret == -EBUSY)
			ret = -EINVAL;
	}
detach:
	iommu_detach_device(domain, dev);
unreg_dom:
	msm_unregister_domain(domain);
	tst_iommu->ret_code = ret;
	return ret;
}

#define CTX_SHIFT 12
#define CTX_OFFSET 0x8000

#define GET_GLOBAL_REG(reg, base) (readl_relaxed((base) + (reg)))

#define SET_V0_CTX_REG(reg, base, ctx, val) \
		writel_relaxed((val), ((base) + (reg) + ((ctx) << CTX_SHIFT)))

#define SET_V1_CTX_REG(reg, base, ctx, val) \
	writel_relaxed((val), \
		((base) + CTX_OFFSET + (reg) + ((ctx) << CTX_SHIFT)))

#define FSRRESTORE_V0	(0x024)
#define FSRRESTORE_V1	(0x05C)
#define FSR_V0		(0x020)
#define FSR_V1		(0x058)

struct meta_data {
	struct completion comp;
	const struct msm_iommu_test *iommu_test;
};

static int fault_handler(struct iommu_domain *domain,
			struct device *dev, unsigned long iova, int flags,
			void *handler_token)
{
	struct msm_iommu_drvdata *drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	struct meta_data *int_data = handler_token;

	drvdata = dev_get_drvdata(dev->parent);
	ctx_drvdata = dev_get_drvdata(dev);

	if (int_data->iommu_test->iommu_rev == 0) {
		SET_V0_CTX_REG(FSR_V0, drvdata->base, ctx_drvdata->num,
			       0x4000000a);
	} else if (int_data->iommu_test->iommu_rev == 1) {
		SET_V1_CTX_REG(FSR_V1, drvdata->base, ctx_drvdata->num,
			       0x4000000a);
	} else {
		pr_err("%s: Unknown IOMMU rev: %d\n", __func__,
			int_data->iommu_test->iommu_rev);
	}

	complete(&int_data->comp);
	return IRQ_HANDLED;
}

static int trigger_interrupt(struct device *dev, int iommu_rev)
{
	struct msm_iommu_drvdata *drvdata;
	struct msm_iommu_ctx_drvdata *ctx_drvdata;
	int ret = 0;

	drvdata = dev_get_drvdata(dev->parent);
	ctx_drvdata = dev_get_drvdata(dev);
	if (iommu_rev == 0) {
		SET_V0_CTX_REG(FSRRESTORE_V0, drvdata->base, ctx_drvdata->num,
			       0x4000000a);
	} else if (iommu_rev == 1) {
		SET_V1_CTX_REG(FSRRESTORE_V1, drvdata->base, ctx_drvdata->num,
			       0x4000000a);
	} else {
		pr_err("%s: Unknown IOMMU rev: %d\n", __func__, iommu_rev);
		ret = -ENODEV;
	}
	return ret;

}

static int test_iommu_INT(const struct msm_iommu_test *iommu_test,
			  struct test_iommu *tst_iommu)
{
	int ret = 0;
	int domain_id = 0;
	struct iommu_domain *domain;
	struct iommu *smmu = &iommu_test->iommu_list[tst_iommu->iommu_no];
	const char *ctx_name = smmu->cb_list[tst_iommu->cb_no].name;
	struct device *dev;
	struct msm_iommu_drvdata *drvdata;
	struct meta_data int_data;

	int_data.iommu_test = iommu_test;

	domain_id = create_domain(&domain);

	dev = msm_iommu_get_ctx(ctx_name);
	if (IS_ERR_OR_NULL(dev)) {
		pr_err("context %s not found: %ld\n", ctx_name, PTR_ERR(dev));
		ret = -EINVAL;
		goto unreg_dom;
	}

	if (is_ctx_busy(dev)) {
		ret = -EBUSY;
		goto unreg_dom;
	}

	ret = iommu_attach_device(domain, dev);
	if (ret) {
		pr_err("iommu attach failed: %x\n", ret);
		goto unreg_dom;
	}

	init_completion(&int_data.comp);
	iommu_set_fault_handler(domain, fault_handler, &int_data);

	drvdata = dev_get_drvdata(dev->parent);
	iommu_access_ops->iommu_clk_on(drvdata);
	ret = trigger_interrupt(dev, iommu_test->iommu_rev);
	if (ret)
		goto detach;

	ret = wait_for_completion_timeout(&int_data.comp,
					  msecs_to_jiffies(5000));
	if (ret)
		ret = 0;
	else
		ret = -EFAULT;

detach:
	iommu_access_ops->iommu_clk_off(drvdata);
	iommu_detach_device(domain, dev);
unreg_dom:
	msm_unregister_domain(domain);

	tst_iommu->ret_code = ret;
	return ret;
}

static int test_iommu_bfb(const struct msm_iommu_test *iommu_test,
			  struct test_iommu *tst_iommu)
{
	int ret = 0;
	int domain_id = 0;
	struct iommu_domain *domain;
	struct iommu *smmu = &iommu_test->iommu_list[tst_iommu->iommu_no];
	const char *ctx_name = smmu->cb_list[tst_iommu->cb_no].name;
	struct msm_iommu_drvdata *drvdata;
	struct device *dev;
	unsigned int i;

	if (iommu_test->iommu_rev == 0)
		goto out;

	domain_id = create_domain(&domain);

	dev  = msm_iommu_get_ctx(ctx_name);
	if (IS_ERR_OR_NULL(dev)) {
		pr_err("context %s not found: %ld\n", ctx_name, PTR_ERR(dev));
		ret = -EINVAL;
		goto unreg_dom;
	}

	if (!is_ctx_busy(dev))
		ret = iommu_attach_device(domain, dev);

	if (ret) {
		pr_err("iommu attach failed: %x\n", ret);
		goto unreg_dom;
	}

	drvdata = dev_get_drvdata(dev->parent);
	iommu_access_ops->iommu_clk_on(drvdata);

	for (i = 0; i < tst_iommu->bfb_size; ++i) {
		unsigned int reg_value = GET_GLOBAL_REG(drvdata->base,
							tst_iommu->bfb_regs[i]);
		if (reg_value != tst_iommu->bfb_data[i]) {
			pr_err("Register at offset 0x%x has incorrect value. Expected: 0x%x, found: 0x%x\n",
				tst_iommu->bfb_regs[i], tst_iommu->bfb_data[i],
				reg_value);
			ret = -EINVAL;
		}
	}
	iommu_access_ops->iommu_clk_off(drvdata);

	iommu_detach_device(domain, dev);
unreg_dom:
	msm_unregister_domain(domain);
out:
	tst_iommu->ret_code = ret;
	return ret;
}

static long iommu_test_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int ret = 0;
	struct msm_iommu_test *iommu_test = file->private_data;
	struct get_next_cb gnc;
	struct test_iommu tst_iommu;
	struct target_struct target;

	switch (cmd) {
	case IOC_IOMMU_GET_NXT_IOMMU_CB:
	{
		if (copy_from_user(&gnc, (void __user *)arg, sizeof(gnc)))
			ret = -EFAULT;

		if (!ret) {
			get_next_cb(iommu_test, &gnc);
			if (copy_to_user((void __user *)arg, &gnc, sizeof(gnc)))
				ret = -EFAULT;
			else
				ret = 0;
		}
		break;
	}
	case IOC_IOMMU_GET_TARGET:
	{
		get_target(&target);
		if (copy_to_user((void __user *)arg, &target, sizeof(target)))
			ret = -EFAULT;
		else
			ret = 0;
		break;
	}
	case IOC_IOMMU_TEST_IOMMU_VA2PA_HTW:
	{
		if (copy_from_user(&tst_iommu, (void __user *)arg, sizeof
				   (tst_iommu)))
			ret = -EFAULT;

		if (!ret) {
			ret = test_VA2PA_HTW(iommu_test, &tst_iommu);
			if (copy_to_user((void __user *)arg, &tst_iommu,
					 sizeof(tst_iommu)))
				ret = -EFAULT;
		}
		break;
	}
	case IOC_IOMMU_TEST_IOMMU_INT:
	{
		if (copy_from_user(&tst_iommu, (void __user *)arg, sizeof
				   (tst_iommu)))
			ret = -EFAULT;

		if (!ret) {
			ret = test_iommu_INT(iommu_test, &tst_iommu);
			if (copy_to_user((void __user *)arg, &tst_iommu,
					 sizeof(tst_iommu)))
				ret = -EFAULT;
		}
		break;
	}
	case IOC_IOMMU_TEST_IOMMU_BFB:
	{
		if (copy_from_user(&tst_iommu, (void __user *)arg, sizeof
				   (tst_iommu)))
			ret = -EFAULT;

		if (!ret) {
			ret = test_iommu_bfb(iommu_test, &tst_iommu);
			if (copy_to_user((void __user *)arg, &tst_iommu,
					 sizeof(tst_iommu)))
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

/*
 * Register ourselves as a device to be able to test the iommu code
 * from userspace.
 */

static const struct file_operations iommu_test_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = iommu_test_ioctl,
	.open = iommu_test_open,
	.release = iommu_test_release,
};

static int iommu_test_device_create(void)
{
	int ret_val = 0;

	iommu_test_major = register_chrdev(0, IOMMU_TEST_DEV_NAME,
					   &iommu_test_fops);
	if (iommu_test_major < 0) {
		pr_err("Unable to register chrdev: %d\n", iommu_test_major);
		ret_val = iommu_test_major;
		goto out;
	}

	iommu_test_class = class_create(THIS_MODULE, IOMMU_TEST_DEV_NAME);
	if (IS_ERR(iommu_test_class)) {
		ret_val = PTR_ERR(iommu_test_class);
		pr_err("Unable to create class: %d\n", ret_val);
		goto err_create_class;
	}

	iommu_test_dev = device_create(iommu_test_class, NULL,
				       MKDEV(iommu_test_major, 0),
				       NULL, IOMMU_TEST_DEV_NAME);
	if (IS_ERR(iommu_test_dev)) {
		ret_val = PTR_ERR(iommu_test_dev);
		pr_err("Unable to create device: %d\n", ret_val);
		goto err_create_device;
	}
	goto out;

err_create_device:
	class_destroy(iommu_test_class);
err_create_class:
	unregister_chrdev(iommu_test_major, IOMMU_TEST_DEV_NAME);
out:
	return ret_val;
}

static void iommu_test_device_destroy(void)
{
	device_destroy(iommu_test_class, MKDEV(iommu_test_major, 0));
	class_destroy(iommu_test_class);
	unregister_chrdev(iommu_test_major, IOMMU_TEST_DEV_NAME);
}

static int iommu_test_init(void)
{
	return iommu_test_device_create();
}

static void iommu_test_exit(void)
{
	return iommu_test_device_destroy();
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Test for MSM IOMMU implementation");
module_init(iommu_test_init);
module_exit(iommu_test_exit);
