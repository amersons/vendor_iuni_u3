/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* MSM ION tester. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <sys/types.h>
#include <ctype.h>
#include <linux/input.h>
#include <linux/msm_ion.h>
#include <linux/kernel.h>
#include <sys/mman.h>
#include "ion_test_plan.h"
#include "ion_test_utils.h"

/* Device path names */
#define MSM_ION_TEST	"/dev/msm_ion_test"
#define MSM_ION		"/dev/ion"

unsigned int TEST_TYPE = NOMINAL_TEST;

static int run_tests(struct ion_test_plan **table, const char *test_plan,
					unsigned int type, size_t size,
					unsigned int *total_tests,
					unsigned int *tests_skipped)
{
	struct ion_test_plan *test;
	unsigned int i;
	int ret = 0;
	int skipped = 0;

	for (i = 0; i < size; i++) {
		test = table[i];
		if (!test) {
			printf("%s test table size mismatch\n", test_plan);
			break;
		}

		if (test->test_type_flags & type) {
			ret = test->test_fn(MSM_ION, MSM_ION_TEST, test,
					    type, &skipped);
			if (ret) {
				debug(INFO, "%s test failed, stopping\n",
								test->name);
				break;
			} else {
				debug(INFO, "%s test passed\n", test->name);
			}
			++(*total_tests);
			if (skipped)
				++(*tests_skipped);
		}
	}
	return ret;
}

int parse_args(int argc, char **argv)
{
	unsigned int level;
	if (argc != 3)
		return 1;
	switch (argv[1][0]) {
	case 'n':
	{
		TEST_TYPE = NOMINAL_TEST;
		break;
	}
	case 'a':
	{
		TEST_TYPE = ADV_TEST;
		break;
	}
	default:
		return -EINVAL;
	}
	level = atoi(argv[2]);
	if (level != INFO && level != ERR)
		return -EINVAL;
	set_debug_level(level);
	return 0;
}

int main(int argc, char **argv)
{
	unsigned int i = 0, type;
	int ret = 0;
	size_t usize, ksize, cpsize;
	struct ion_test_plan **utable, **ktable, **cptable;
	unsigned int total_tests_run = 0;
	unsigned int total_skipped = 0;
	if (parse_args(argc, argv)) {
		debug(ERR, "incorrect arguments passed\n");
		return -EINVAL;
	}
	/* Get test tables */
	utable = get_user_ion_tests(MSM_ION_TEST, &usize);
	if (!utable) {
		debug(ERR, "no user tests\n");
		return -EIO;
	}
	ktable = get_kernel_ion_tests(MSM_ION_TEST, &ksize);
	if (!ktable) {
		debug(ERR, "no kernel tests\n");
		return -EIO;
	}
	cptable = get_cp_ion_tests(MSM_ION_TEST, &cpsize);
	if (!cptable) {
		debug(ERR, "no user tests\n");
		return -EIO;
	}
	/* Run tests */
	if (TEST_TYPE == NOMINAL_TEST) {
		debug(INFO, "\n\nRunning Nominal user space ion tests :\n\n");
		ret = run_tests(utable, "user space", NOMINAL_TEST, usize,
				&total_tests_run, &total_skipped);
		if (ret) {
			debug(INFO, "user space Nominal tests failed\n");
			return ret;
		}
		type = NOMINAL_TEST;
		debug(INFO, "\n\nRunning Nominal kernel space ion tests :\n\n");
		ret = run_tests(ktable, "kernel space", type, ksize,
				&total_tests_run, &total_skipped);
		if (ret) {
			debug(INFO, "kernel space Nominal tests failed\n");
			return ret;
		}
		debug(INFO, "\n\nRunning Nominal cp ion tests :\n\n");
		ret = run_tests(cptable, "CP", type, cpsize,
				&total_tests_run, &total_skipped);
		if (ret) {
			debug(INFO, "cp ion Nominal tests failed\n");
			return ret;
		}
	}
	if (TEST_TYPE == ADV_TEST) {
		type = ADV_TEST;
		debug(INFO, "\n\nRunning Adversarial user space ion tests:\n");
		ret = run_tests(utable, "user space", type, usize,
				&total_tests_run, &total_skipped);
		if (ret) {
			debug(INFO, "user space Adversarial tests failed\n");
			return ret;
		}
		debug(INFO,
			"\n\nRunning Adversarial kernel space ion tests:\n");
		ret = run_tests(ktable, "kernel space", type, ksize,
				&total_tests_run, &total_skipped);
		if (ret) {
			debug(INFO, "kernel space Adversarial tests failed\n");
			return ret;
		}
		debug(INFO, "\n\nRunning Adversarial cp ion tests :\n\n");
		ret = run_tests(cptable, "CP", type, cpsize,
				&total_tests_run, &total_skipped);
		if (ret) {
			debug(INFO, "cp ion Adversarial tests failed\n");
			return ret;
		}
	}
	printf("Ran %u tests out of which %u were skipped\n",
	      total_tests_run, total_skipped);
	return ret;
}
