#include <linux/ioctl.h>

#define MSM_SPS_TEST_IOC_MAGIC     0xBB

/* Specify the test type */
#define MSM_SPS_TEST_TYPE _IOW(MSM_SPS_TEST_IOC_MAGIC, 1, int)

/* Continue testing all testcases in case of failure */
#define MSM_SPS_TEST_IGNORE_FAILURE _IOW(MSM_SPS_TEST_IOC_MAGIC, 2, int)
