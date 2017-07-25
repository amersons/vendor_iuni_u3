/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include "msm_bus_test.h"

#define MBPS(n) ((n) * 1000000)
#define NUM_ITERATIONS 1000
#define NUM_STRESS_ITERATIONS 20000
#define MAX_NUM_THREADS 20

static unsigned int iter;
static unsigned int nthreads;

void configureclient(struct msm_bus_test_scale_pdata *pdata, int target)
{
	int i, j;
	for (i = 0; i < NUM_USECASES; i++) {
		for (j = 0; j < NUM_VECTORS; j++) {
			pdata->usecase[i].vectors[j].dst =
				MSM_BUS_TEST_SLAVE_FIRST;
			pdata->usecase[i].vectors[j].ab = 0;
			pdata->usecase[i].vectors[j].ib = MBPS(j);
		}
		pdata->usecase[i].num_paths = NUM_VECTORS;
	}

	if (target == 8960 || target == 8660 || target == 9615 ||
		target == 8064 || target == 8930 || target == 'A' ||
		target == 'a') {
		for (i = 0; i < NUM_USECASES; i++) {
			pdata->usecase[i].vectors[0].src =
				MSM_BUS_TEST_MASTER_AMPSS_M0;
			pdata->usecase[i].vectors[1].src =
				MSM_BUS_TEST_MASTER_SPS;
			pdata->usecase[i].vectors[2].src =
				MSM_BUS_TEST_MASTER_LPASS;
			pdata->usecase[i].vectors[3].src =
				MSM_BUS_TEST_MASTER_ADM0_PORT0;
			pdata->usecase[i].vectors[4].src =
				MSM_BUS_TEST_MASTER_ADM0_PORT1;
		}
	} else if (target == 8974 || target == 9625 || target == 8226
			|| target == 8610 || target == 'B' ||
			target == 'b' || target == 8084) {
		for (i = 0; i < NUM_USECASES; i++) {
			pdata->usecase[i].vectors[0].src =
				MSM_BUS_TEST_MASTER_AMPSS_M0;
			pdata->usecase[i].vectors[1].src =
				MSM_BUS_TEST_MASTER_LPASS_PROC;
			pdata->usecase[i].vectors[2].src =
				MSM_BUS_TEST_MASTER_CRYPTO_CORE0;
			pdata->usecase[i].vectors[3].src =
				MSM_BUS_TEST_MASTER_SDCC_1;
			pdata->usecase[i].vectors[4].src =
				MSM_BUS_TEST_MASTER_QDSS_BAM;
		}
	} else {
		printf("Warning! Couldn't configure client. Target ID %d"
			" is not valid. Tests may fail\n"
			"Please provide a valid target-id. See usage\n",
			target);
	}

	printf("%s\n", "TestClient");
	pdata->active_only = 0;
	pdata->num_usecases = NUM_USECASES;
	pdata->name = "TestClient";
}

static void print_usage()
{
	printf("Usage:\n");
	puts("  Supported targets: 8660, 8960, 8930, 9615, 8064, 8974, 9625, "
		"8226, 8610, 8084\n"
		"Supported families: A & B\n"
		"Usage with shell script:\n"
		"\tWith target IDs:\n"
		"--target <target-id ex. 8974> --nominal\n"
		"	Nominal is set by default\n"
		"	Example: ./msm_bus_test.sh --nominal --target 8974\n"
		"--target <target-id> --adversarial\n"
		"	Example: ./msm_bus_test.sh --target 8226 --adversarial\n"
		"--target <target-id> --stress [Number of threads]\n"
		"	Example: ./msm_bus_test.sh --target 9615 --stress 2\n"
		"--target <target-id> --repeatability [Number of iterations]\n"
		"	Example: ./msm_bus_test.sh --target 8610 --repeatability 100\n"
		"\nWith with families:\n"
		"./msm_bus_test.sh --family A --nominal\n\n"
		"Usage directly through test app\n"
		"insmod /system/lib/modules/msm_bus_ioctl.ko\n"
		"./msm_bus_test --target <Target-ID> --test-type <test-type args>\n"
		"Example: ./msm_bus_test --target 8974 --stress 5\n"
		"Example: ./msm_bus_test --family B --repeatability 100\n");
	exit(1);
}

static int parse_args(int argc, char **argv, int *target)
{
	struct option lopts[] = {
		{ "nominal",       no_argument,   NULL, 'n'},
		{ "adversarial",   no_argument,   NULL, 'a'},
		{ "stress",        required_argument,   NULL, 's'},
		{ "repeatability", required_argument,   NULL, 'r'},
		{ "target", required_argument,   NULL, 't'},
		{ "family", required_argument,   NULL, 'f'},
		{ NULL,            0,             NULL,  0},
	};
	int command, target_id = 0, ret = 0;
	const char *optstr = "n:a:s:r:t:f";

	while ((command = getopt_long(argc, argv, optstr, lopts, NULL)) != -1) {
		switch (command) {
		case 'n':
			printf("\nRunning nominal tests...\n");
			ret = 'n';
			break;
		case 'r':
			if (optarg) {
				printf("Iterations: %d\n\n", atoi(optarg));
				iter = atoi(optarg);
			} else {
				printf("Using default iterations\n");
				iter = NUM_ITERATIONS;
			}

			printf("Running stress tests...\n");
			ret = 'r';
			break;
		case 'a':
			printf("Running adversarial tests...\n");
			ret = 'a';
			break;
		case 's':
			if (optarg) {
				printf("number of threads: %d\n\n",
					atoi(optarg));
				nthreads = atoi(optarg);
			} else {
				printf("Using max number of threads\n");
				nthreads = MAX_NUM_THREADS;
				iter = NUM_ITERATIONS;
			}

			printf("\nRunning repeatability test for %d "
				"iterations\n", iter);
			ret = 's';
			break;
		case 't':
			if (optarg) {
				printf("\nGot target ID: %d\n", atoi(optarg));
				target_id = atoi(optarg);
				*target = target_id;
				break;
			}
		case 'f':
			if (optarg) {
				printf("\nGot Family %c\n", optarg[0]);
				target_id = optarg[0];
				*target = target_id;
				break;
			}
		default:
			printf("\nDefault option: Running nominal tests...\n");
			ret = 'n';
			break;
		}
	}

	if (target_id == 0) {
		printf("\nTarget id not found. Usage:\n");
		print_usage();
		ret = 0;
	}

	return ret;
}

static int nominal(int fd, int target)
{
	uint32_t clid;
	int i, retval = 0;
	struct msm_bus_test_update_req_data rdata;
	struct msm_bus_test_cldata cldata;

	configureclient(&cldata.pdata, target);
	retval = ioctl(fd, MSM_BUS_TEST_REG_CL, &cldata);
	clid = cldata.clid;
	printf("Getting client id...\n");
	if (clid != 0) {
		printf("Got client id: %u\n", clid);
		rdata.clid = clid;
	} else {
		printf("Client could not be registered\n");
		goto err;
	}

	rdata.index = (rand() % NUM_USECASES);
	printf("Updating request for cl %u, index: %d\n", clid, rdata.index);
	retval = ioctl(fd, MSM_BUS_TEST_UPDATE_REQ, &rdata);
	rdata.index = 0;
	retval = ioctl(fd, MSM_BUS_TEST_UPDATE_REQ, &rdata);
	printf("Unregistering client: %u\n", cldata.clid);
	retval = ioctl(fd, MSM_BUS_TEST_UNREG_CL, &cldata);
	printf("Client unregistered: %u\n", clid);
err:
	return retval;
}

static int adversarial(int fd, int target)
{
	struct msm_bus_test_update_req_data rdata;
	struct msm_bus_test_cldata cldata;

	configureclient(&cldata.pdata, target);
	printf("Test 1: Using invalid master id\n");
	cldata.pdata.usecase[0].vectors[0].src = MSM_BUS_TEST_SLAVE_FIRST;

	ioctl(fd, MSM_BUS_TEST_REG_CL, &cldata);
	printf("Getting client id...\n");
	if (cldata.clid != 0) {
		printf("Got client id: %u\n", cldata.clid);
		rdata.clid = cldata.clid;
	} else
		printf("Client could not be registered: %d\n", cldata.clid);

	printf("Test 2: Using invalid slave id\n");
	cldata.pdata.usecase[0].vectors[0].src = MSM_BUS_TEST_MASTER_FIRST;
	cldata.pdata.usecase[0].vectors[0].dst = MSM_BUS_TEST_MASTER_MDP_PORT0;

	ioctl(fd, MSM_BUS_TEST_REG_CL, &cldata);
	printf("Getting client id...\n");
	if (cldata.clid != 0) {
		printf("Got client id: %u\n", cldata.clid);
		rdata.clid = cldata.clid;
	} else
		printf("Success. Client could not be registered: %d\n",
			cldata.clid);

	printf("Test 3: Using negative index\n");
	cldata.pdata.usecase[0].vectors[0].src = MSM_BUS_TEST_MASTER_FIRST;
	cldata.pdata.usecase[0].vectors[0].dst = MSM_BUS_TEST_SLAVE_FIRST;
	ioctl(fd, MSM_BUS_TEST_REG_CL, &cldata);
	printf("Getting client id...\n");
	if (cldata.clid != 0) {
		printf("Got client id: %u\n", cldata.clid);
		rdata.clid = cldata.clid;
	}

	rdata.clid = cldata.clid;
	rdata.index = -5;
	if (ioctl(fd, MSM_BUS_TEST_UPDATE_REQ, &rdata))
		printf("Success: Request couldn't be updated:\n");

	printf("Test 4: Using out of bounds index\n");
	rdata.clid = cldata.clid;
	rdata.index = 35;
	if (ioctl(fd, MSM_BUS_TEST_UPDATE_REQ, &rdata))
		printf("Success: Request couldn't be updated:\n");

err:
	return 0;
}

static int repeatability(int fd, int ni, int target)
{
	int i, retval;
	for (i = 0; i < ni; i++) {
		printf("Iteration %d\n", i);
		retval = nominal(fd, target);
		if (retval) {
			printf("\nRepeatability test failed at "
				"iteration: %d\n", i);
			return retval;
		}
	}
	return 0;
}

void *rep_threadfn(void *thargs)
{
	int ret = 0;
	ret = repeatability(((int *)thargs)[0], ((int *)thargs)[1],
			((int *)thargs)[2]);
	if (ret)
		printf("\nError :%d\n", ret);
	return (void *)ret;
}

static int stress(int fd, int target)
{
	int ret = 0;
	unsigned int i, j;
	pthread_t thread[MAX_NUM_THREADS];
	int iret[MAX_NUM_THREADS];
	int thargs[3];

	printf("\nRunning stress test 1\n"
		"Register client, update request with different clock values,"
		"unregister client...\n");
	ret = repeatability(fd, NUM_ITERATIONS, target);
	if (ret)
		printf("Stress test 1 failed..\n");

	printf("\nRunning stress test 2\n"
		"Concurrency test: Spawn 20 threads which register client, "
		"upadte request, and unregister client..\n");

	thargs[0] = fd;
	thargs[1] = NUM_ITERATIONS;
	thargs[2] = target;
	for (i = 0; i < nthreads; i++) {
		iret[i] = pthread_create(&thread[i], NULL, rep_threadfn,
			(void *) thargs);
		if (iret[i]) {
			printf("\nFailed to create %d thread for stress "
				"test..\n", i);
			for (j = 0; j < i; j++)
				pthread_join(thread[j], NULL);
			printf("\nStress test failed in thread creation\n");
			ret = -1;
			return ret;
		}
		printf("\nRunning test thread %d\n", i);
	}

	for (i = 0; i < nthreads; i++)
		pthread_join(thread[i], NULL);

	return 0;
}

int main(int argc, char **argv)
{
	int fd, test, target = 0;

	test = parse_args(argc, argv, &target);
	if (test  < 0)
		return 1;

	printf("In main for msm_bus_test\n");
	fd = open("/dev/msmbustest", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("Unable to open /dev/msmbustest");
		exit(1);
	}

	printf("\nGot test :%c\n", test);
	switch (test) {
	case 'n':
		nominal(fd, target);
		break;
	case 's':
		stress(fd, target);
		break;
	case 'a':
		adversarial(fd, target);
		break;
	case 'r':
		printf("\nStarting repeatability tests:\n");
		repeatability(fd, iter, target);
		break;
	default:
		nominal(fd, target);
		break;
	}

	close(fd);
	exit(0);
}

