/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>

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


#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#define NUM_ITERATIONS (10)

static int ocmem_dev_fd;
static int iterations;
static int verbosity;

#define pr_debug(msg, args...) do {	\
			if (verbosity)	\
				printf("\n"msg, ##args);\
		} while (0)

#define pr_err(msg, args...) do {	\
			fprintf(stderr, "\n"msg, ##args);\
		} while (0)

enum test_types {
	NOMINAL,
	ADVERSARIAL,
	STRESS,
	REPEAT,
};

struct option testopts[] = {
	{"Nominal", no_argument, NULL, 'n'},
	{"Adversarial", no_argument, NULL, 'a'},
	{"Stress", no_argument, NULL, 's'},
	{"Repeatability", required_argument, NULL, 'r'},
	{"Verbose", no_argument, NULL, 'v'},
	{"Help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

static void usage(int ret)
{
	printf("Usage: ocmem_test [OPTIONS] [TEST_TYPE]...\n"
	       "Runs the kernel mode ocmem tests specified by the TEST_TYPE\n"
	       "parameter. If no TEST_TYPE is specified, then the nominal\n"
	       " test is run.\n"
	       "\n"
	       "OPTIONS can be:\n"
	       "  -v, --verbose         run with debug messages enabled\n"
	       "\n"
	       "TEST_TYPE can be:\n"
	       "  -n, --nominal         run standard functionality tests\n"
	       "  -a, --adversarial     run tests that try to break the\n"
	       "                          driver\n"
	       "  -s, --stress          run tests that try to maximize the\n"
	       "                          capacity of the driver\n"
	       "  -r, --repeatability   run specified iterations of both the\n"
	       "                          nominal and adversarial tests\n");
	exit(ret);
}

static unsigned int parse_command(int argc, char *const argv[])
{
	int command;
	unsigned ret = 0;

	while ((command = getopt_long(argc, argv, "vnashr:", testopts,
				      NULL)) != -1) {
		switch (command) {
		case 'v':
			verbosity = 1;
			break;
		case 'n':
			ret |= 1 << NOMINAL;
			break;
		case 'a':
			ret |= 1 << ADVERSARIAL;
			break;
		case 's':
			ret |= 1 << STRESS;
			break;
		case 'r':
			ret |= 1 << REPEAT;
			if (optarg) {
				iterations = atoi(optarg);
				if (iterations <= 0)
					iterations = NUM_ITERATIONS;
				printf("Iterations: %d\n\n", iterations);
			} else {
				iterations = NUM_ITERATIONS;
				printf("Using default iterations: %d\n",
					iterations);
			}
			break;
		case 'h':
			usage(0);
		default:
			usage(-1);
		}
	}

	return ret;
}

static int nominal_test(void)
{
	int errors = 0;
	int ret = 0;

	pr_debug("OCMEM nominal test iocl invoked\n");

	ret = ioctl(ocmem_dev_fd, OCMEM_TEST_TYPE_NOMINAL);

	if (ret == -ENOTTY || ret == -EFAULT || ret == -EINVAL) {
		pr_err("Failed to invoke ioctl (errno:%d)\n", ret);
		exit(-1);
	}

	if (ret < 0) {
		pr_err("OCMEM nominal test FAILED; failures %d\n", ret);
		errors++;
	}
	return errors;
}

static int stress_test(void)
{
	int errors = 0;
	int ret = 0;

	pr_debug("OCMEM nominal test ioctl invoked\n");

	ret = ioctl(ocmem_dev_fd, OCMEM_TEST_TYPE_STRESS);

	if (ret == -ENOTTY || ret == -EFAULT || ret == -EINVAL) {
		pr_err("Failed to invoke ioctl (errno:%d)\n", ret);
		exit(-1);
	}

	if (ret < 0) {
		pr_err("OCMEM stress test FAILED; failures %d\n", ret);
		errors++;
	}

	return errors;
}

static int adversarial_test(void)
{
	int errors = 0;
	int ret = 0;

	pr_debug("OCMEM adversarial test ioctl invoked\n");

	ret = ioctl(ocmem_dev_fd, OCMEM_TEST_TYPE_ADVERSARIAL);

	if (ret == -ENOTTY || ret == -EFAULT || ret == -EINVAL) {
		pr_err("Failed to invoke ioctl (errno:%d)\n", ret);
		exit(-1);
	}

	if (ret < 0) {
		pr_err("OCMEM adversarial test FAILED; failures %d\n", ret);
		errors++;
	}

	return errors;
}

static int repeat_test(void)
{
	unsigned int i = 0;
	int errors = 0;

	pr_debug("OCMEM repeat test invoked for %d iterations\n", iterations);

	for (i = 0; i < iterations; i++) {
		pr_debug("Repeat test iteration %d\n", i);
		errors += nominal_test();
		errors += stress_test();
	}
	return errors;
}

static int (*test_func[]) () = {
	[NOMINAL] = nominal_test,
	[ADVERSARIAL] = adversarial_test,
	[STRESS] = stress_test,
	[REPEAT] = repeat_test,
};

int main(int argc, char **argv)
{
	int rc = -1;
	int num_failures = 0;
	unsigned int i = 0;

	unsigned int test_mask = parse_command(argc, argv);

	ocmem_dev_fd = open("/dev/ocmemtest", O_RDWR | O_SYNC);

	if (ocmem_dev_fd < 0) {
		pr_err("Failed to open OCMEM device\n");
		return rc;
	}

	pr_debug("OCMEM opened ocmem_dev_fd (%d) successfully\n",
					ocmem_dev_fd);

	if (verbosity) {
		if (ioctl(ocmem_dev_fd, OCMEM_TEST_VERBOSE_MODE,
					&verbosity) < 0)
			pr_err("Failed to set verbose mode in DLKM\n");
	}

	for (i = 0; i < (int)ARRAY_SIZE(test_func); i++) {
		/* Look for the test that was selected */
		if (!(test_mask & (1U << i)))
			continue;

		/* This test was selected, so run it */
		rc = test_func[i]();

		if (rc) {
			pr_err("%s test case FAILED! rc:%d\n",
				testopts[i].name, rc);
			num_failures += rc;
		}
	}

	if (num_failures)
		pr_err("OCMEM: NUMBER OF TESTS FAILED: %d\n", num_failures);

	close(ocmem_dev_fd);

	pr_debug("OCMEM test exiting with %d failures\n", num_failures);

	return -num_failures;
}
