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


/**
 * file: timing_debug.c
 *
 * A simple nesting timer module.
 *
 * IMPORTANT: THIS IS NOT THREAD SAFE!
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "timing_debug.h"

#define MODULE_NAME "TIMING_DEBUG"

#define __TIMING_DEBUG_TIME_MAX_CLOCK_NESTING 100
#define __TIMING_DEBUG_TIME_MAX_TICKS_AND_TOCKS_PER_SESSION 5000
#define __TIMING_DEBUG_TIME_LINE_SIZE 100


static struct timespec __timing_debug_t1[__TIMING_DEBUG_TIME_MAX_CLOCK_NESTING];
static struct timespec __timing_debug_t2[__TIMING_DEBUG_TIME_MAX_CLOCK_NESTING];

static int __timing_debug_time_sp;
static int __timing_debug_time_timing_buf_cur;
static char *__timing_debug_timing_message_buffer[
	__TIMING_DEBUG_TIME_MAX_TICKS_AND_TOCKS_PER_SESSION];

void timing_debug_start_new_timings(void)
{
	__timing_debug_time_timing_buf_cur = 0;
}

void timing_debug_dump_results(void)
{
	int i;
	for (i = 0; i < __timing_debug_time_timing_buf_cur; ++i)
		pr_err("[timing debug] %s",
			__timing_debug_timing_message_buffer[i]);
}

static char *__timing_debug_get_spaces(void)
{
	int i;
	static char spaces[__TIMING_DEBUG_TIME_MAX_CLOCK_NESTING];
	char *it = spaces;
	for (i = 0; i < __timing_debug_time_sp; ++i)
		*it++ = ' ';
	*it++ = '\0';
	return spaces;
}

void timing_debug_tick(char *msg)
{
	snprintf(__timing_debug_timing_message_buffer[
			__timing_debug_time_timing_buf_cur++],
		__TIMING_DEBUG_TIME_LINE_SIZE,
		"%stick %s\n", __timing_debug_get_spaces(), msg);
	getnstimeofday(&__timing_debug_t1[__timing_debug_time_sp]);
	__timing_debug_time_sp++;
}

void timing_debug_tock(char *msg)
{
	struct timespec diff;
	--__timing_debug_time_sp;
	getnstimeofday(&__timing_debug_t2[__timing_debug_time_sp]);
	diff = timespec_sub(__timing_debug_t2[__timing_debug_time_sp],
			__timing_debug_t1[__timing_debug_time_sp]);
	snprintf(__timing_debug_timing_message_buffer[
			__timing_debug_time_timing_buf_cur++],
		__TIMING_DEBUG_TIME_LINE_SIZE,
		"%stock %s => Delta: %ld ms\n",
		__timing_debug_get_spaces(), msg,
		(long int) div_s64(timespec_to_ns(&diff), 1000000));
}

/* TODO: protect with a mutex */
static bool did_init;

int timing_debug_init(void)
{
	int i, rc = 0;
	if (did_init)
		return 0;
	for (i = 0;
	     i < __TIMING_DEBUG_TIME_MAX_TICKS_AND_TOCKS_PER_SESSION;
	     ++i) {
		void *buf = kmalloc(__TIMING_DEBUG_TIME_LINE_SIZE, GFP_KERNEL);
		if(!buf) {
			rc = -ENOMEM;
			goto out;
		}
		__timing_debug_timing_message_buffer[i] = buf;
	}
	did_init = true;
out:
	return rc;
}
