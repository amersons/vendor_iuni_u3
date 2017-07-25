#ifndef __MSM_ION_TEST__
#define __MSM_ION_TEST__

#include <linux/ioctl.h>

enum ion_test_heap_type {
	SYSTEM_MEM = 1, /* SYSTEM_HEAP */
	SYSTEM_MEM2, /* IOMMU_HEAP */
	SYSTEM_CONTIG,
	CARVEOUT,
	CP_CARVEOUT,
	CP,
};

struct ion_test_data {
	unsigned int align;
	unsigned int size;
	enum ion_test_heap_type heap_type_req;
	unsigned long heap_mask; /* Auto-detected if possible */
	unsigned long flags;
	unsigned long vaddr;
	unsigned long cache;
	int shared_fd;
	unsigned int valid; /* Valid for this target. Auto-detected */
};

struct ion_heap_data {
	enum ion_test_heap_type type;
	unsigned int size;
	unsigned long heap_mask;
	unsigned int valid;
};

#define MSM_ION_MAGIC     'M'

#define IOC_ION_KCLIENT_CREATE	 _IO(MSM_ION_MAGIC, 0)
#define IOC_ION_KCLIENT_DESTROY	 _IO(MSM_ION_MAGIC, 1)

#define IOC_ION_KALLOC	 _IOW(MSM_ION_MAGIC, 2, struct ion_test_data)
#define IOC_ION_KFREE	 _IO(MSM_ION_MAGIC, 3)

#define IOC_ION_KPHYS	 _IO(MSM_ION_MAGIC, 4)

#define IOC_ION_KMAP	 _IO(MSM_ION_MAGIC, 5)
#define IOC_ION_KUMAP	 _IO(MSM_ION_MAGIC, 6)

#define IOC_ION_UIMPORT	_IOW(MSM_ION_MAGIC, 7, struct ion_test_data)

#define IOC_ION_WRITE_VERIFY	_IO(MSM_ION_MAGIC, 8)
#define IOC_ION_VERIFY	_IO(MSM_ION_MAGIC, 9)

#define IOC_ION_UBUF_FLAGS	_IOR(MSM_ION_MAGIC, 10, unsigned long)
#define IOC_ION_UBUF_SIZE	_IOR(MSM_ION_MAGIC, 11, unsigned long)

#define IOC_ION_SEC	_IO(MSM_ION_MAGIC, 12)
#define IOC_ION_UNSEC	_IO(MSM_ION_MAGIC, 13)

#define IOC_ION_FIND_PROPER_HEAP _IOWR(MSM_ION_MAGIC, 14, struct ion_heap_data)


static inline void write_pattern(unsigned long buf, size_t size)
{
	uint8_t *addr = (uint8_t *)buf;
	uint8_t count = 0;
	unsigned int i;
	for (i = 0; i < size; i++) {
		addr[i] = count;
		if (count == 0xFF)
			count = 0;
		else
			count++;
	}
}

static inline int verify_pattern(unsigned long buf, size_t size)
{
	uint8_t *addr = (uint8_t *)buf;
	uint8_t count = 0;
	unsigned int i;
	for (i = 0; i < size; i++) {
		if (addr[i] != count)
			return -EIO;
		if (count == 0xFF)
			count = 0;
		else
			count++;
	}
	return 0;
}
#endif
