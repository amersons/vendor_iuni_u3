/* Copyright (c) 2012, The Linux Foundation. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/memory.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <mach/irqs.h>
#include <mach/msm_iomap.h>
#include <mach/sps.h>
#include "msm_sps_test.h"

/** Module name string */
#define DRV_MAME "sps_test"
#define DRV_VERSION "2.1"
#define BAM_PHYS_ADDR 0x12244000
#define BAM_PHYS_SIZE 0x4000
#define TEST_SIGNATURE 0xfacecafe
#define BAMDMA_MAX_CHANS 10
#define P_EVNT_DEST_ADDR(n)        (0x102c + 64 * (n))
#define TEST_FAIL		-2000

#define SPS_INFO(msg, args...) do {					\
			if (debug_mode)	\
				pr_info(msg, ##args);	\
			else	\
				pr_debug(msg, ##args);	\
		} while (0)

struct test_context {
	dev_t dev_num;
	struct miscdevice *misc_dev;
	long testcase;
	void *bam;
	int is_bam_registered;
	void *dma;
	u32 signature;
};

struct test_endpoint {
	struct sps_pipe *sps;
	struct sps_connect connect;
	struct sps_register_event reg_event;
	int is_bam2bam;
	struct sps_iovec IOVec;
	struct completion xfer_done;
	u32 signature;
};

static struct test_context *sps_test;
int sps_test_num = 0;
int test_type = 0;
int debug_mode = 0;

/**
 * Allocate memory from pipe-memory or system memory.
 *
 * @param use_pipe_mem
 * @param mem
 */
static void test_alloc_mem(int use_pipe_mem, struct sps_mem_buffer *mem)
{
	if (use_pipe_mem) {
		sps_alloc_mem(NULL, SPS_MEM_LOCAL, mem);
	} else {
		dma_addr_t dma_addr;

		mem->base = dma_alloc_coherent(NULL, mem->size, &dma_addr, 0);
		mem->phys_base = dma_addr;
	}
}

/**
 * Free memory from pipe-memory or system memory.
 *
 * @param use_pipe_mem
 * @param mem
 */
static void test_free_mem(int use_pipe_mem, struct sps_mem_buffer *mem)
{
	if (use_pipe_mem) {
		sps_free_mem(NULL, mem);
	} else {
		dma_addr_t dma_addr = mem->phys_base;

		if (dma_addr)
			dma_free_coherent(NULL, mem->size, mem->base, dma_addr);

		mem->phys_base = 0;
		mem->base = NULL;
	}
}

/**
 * print BAM registers' content
 *
 * @virt_addr
 * @pipe
 */
static void print_bam_reg(void *virt_addr, int pipe)
{
	int i;
	u32 *bam = (u32 *) virt_addr;
	u32 ctrl;
	u32 ver;
	u32 pipes;

	if (bam == NULL)
		return;

	ctrl = bam[0xf80 / 4];
	ver = bam[0xf84 / 4];
	pipes = bam[0xfbc / 4];

	SPS_INFO("-- BAM Controls Registers --\n");

	SPS_INFO("BAM_CTRL: 0x%x.\n", ctrl);
	SPS_INFO("BAM_REVISION: 0x%x.\n", ver);
	SPS_INFO("NUM_PIPES: 0x%x.\n", pipes);

	for (i = 0xf80; i < 0x1000; i += 0x10)
		SPS_INFO("bam 0x%x: 0x%x,0x%x,0x%x,0x%x.\n",
			i,
			bam[i / 4],
			bam[(i / 4) + 1], bam[(i / 4) + 2], bam[(i / 4) + 3]);

	SPS_INFO("-- BAM PIPE %d Management Registers --\n", pipe);

	for (i = 0x0000 + 0x80 * pipe; i < 0x0000 + 0x80 * (pipe + 1);
	    i += 0x10)
		SPS_INFO("bam 0x%x: 0x%x,0x%x,0x%x,0x%x.\n", i, bam[i / 4],
			bam[(i / 4) + 1], bam[(i / 4) + 2], bam[(i / 4) + 3]);

	SPS_INFO("-- BAM PIPE %d Configuration Registers --\n", pipe);

	for (i = 0x1000 + 0x40 * pipe; i < 0x1000 + 0x40 * (pipe + 1);
	    i += 0x10)
		SPS_INFO("bam 0x%x: 0x%x,0x%x,0x%x,0x%x.\n", i, bam[i / 4],
			bam[(i / 4) + 1], bam[(i / 4) + 2], bam[(i / 4) + 3]);
}

/**
 * print BAM DMA registers
 *
 * @virt_addr
 */
void print_bamdma_reg(void *virt_addr)
{
	int ch; /* channel */
	u32 *dma = (u32 *) virt_addr;

	if (dma == NULL)
		return;

	SPS_INFO("-- DMA Registers --\n");
	SPS_INFO("dma_enable=%d.\n", dma[0]);

	for (ch = 0; ch < BAMDMA_MAX_CHANS; ch++)
		SPS_INFO("ch#%d config=0x%x.\n", ch, dma[1 + ch]);

}

/**
 * register a BAM device
 *
 * @bam
 * @use_irq
 *
 * @return void*
 */
static void *register_bam(u32 *h, int use_irq)
{
	int res = 0;
	struct sps_bam_props bam_props = {0};
	u32 bam_handle;

	bam_props.phys_addr = BAM_PHYS_ADDR;
	bam_props.virt_addr = ioremap(BAM_PHYS_ADDR, BAM_PHYS_SIZE);
	bam_props.event_threshold = 0x10;	/* Pipe event threshold */
	bam_props.summing_threshold = 0x10;	/* BAM event threshold */

	SPS_INFO(DRV_MAME ":bam physical base=0x%x\n",
		(u32) bam_props.phys_addr);
	SPS_INFO(DRV_MAME ":bam virtual base=0x%x\n",
		(u32) bam_props.virt_addr);

	if (use_irq)
		bam_props.irq = SPS_BAM_DMA_IRQ;
	else
		bam_props.irq = 0;

	res = sps_register_bam_device(&bam_props, &bam_handle);

	if (res == 0) {
		*h = bam_handle;
		sps_test->is_bam_registered = true;
	} else if (res) {
		/* fall back if BAM-DMA already registered */
		SPS_INFO("sps_test:fallback.use sps_dma_get_bam_handle().\n");
		bam_handle = sps_dma_get_bam_handle();
		if (bam_handle == 0)
			return NULL;
		*h = bam_handle;
	} else
		return NULL;

	sps_test->bam = bam_props.virt_addr;

	return sps_test->bam;
}

static void unregister_bam(void *bam, u32 h)
{
	if (bam != NULL)
		iounmap(bam);

	if (sps_test->is_bam_registered) {
		sps_deregister_bam_device(h);
		sps_test->is_bam_registered = false;
	}

	sps_test->bam = NULL;
}

static int wait_xfer_completion(int use_irq,
				struct completion *xfer_done,
				struct sps_pipe *sps,
				int rx_xfers,
				u32 max_retry,
				u32 delay_ms)
{
	struct sps_event_notify dummy_event = {0};
	/* Note: sps_get_event() is polling the pipe and trigger the event. */
	int i;

	if (use_irq) {
		wait_for_completion(xfer_done);
	} else {
		for (i = 0; i < rx_xfers; i++) {
			u32 retry = 0;
			while (!try_wait_for_completion(xfer_done)) {
				sps_get_event(sps, &dummy_event);

				if (retry++ >= max_retry)
					return -EBUSY;

				if (delay_ms)
					msleep(delay_ms);
			}	/* end-of-while */
		}	/* end-of-for */
	}	/* end-of-if */

	return 0;
}

static int sps_test_sys2bam(int use_irq,
			    int use_pipe_mem,
			    int use_cache_mem_data_buf,
			    int loop_test)
{
	int res = 0;

	struct test_endpoint test_ep[2];
	struct test_endpoint *tx_ep = &test_ep[0];
	struct test_endpoint *rx_ep = &test_ep[1];

	void *bam = NULL;
	u32 h = 0;
	struct sps_connect tx_connect = { 0};
	struct sps_connect rx_connect = { 0};

	struct sps_mem_buffer tx_desc_fifo = { 0};
	struct sps_mem_buffer rx_desc_fifo = { 0};

	struct sps_mem_buffer tx_mem = { 0};
	struct sps_mem_buffer rx_mem = { 0};

	struct sps_pipe *tx_h_sps = NULL;
	struct sps_pipe *rx_h_sps = NULL;

	struct sps_iovec tx_iovec = { 0};
	struct sps_iovec rx_iovec = { 0};
	u32 *desc = NULL;
	u32 *tx_buf = NULL;
	u32 *rx_buf = NULL;

	u32 tx_pipe = 4;
	u32 rx_pipe = 5;

	u32 tx_xfer_flags = 0;
	u32 rx_xfer_flags = 0;

	struct sps_register_event tx_event = { 0};
	struct sps_register_event rx_event = { 0};

	u32 tx_size = 0x400;
	u32 rx_size = 0x800;	/* This the max size and not the actual size */

	u32 tx_connect_options = 0;
	u32 rx_connect_options = 0;
	char *irq_mode = use_irq ? "IRQ mode" : "Polling mode";
	char *mem_type = use_pipe_mem ? "Pipe-Mem" : "Sys-Mem";
	int loop = 0;
	int max_loops = 1;

	memset(test_ep, 0, sizeof(test_ep));
	tx_ep->signature = TEST_SIGNATURE;
	rx_ep->signature = TEST_SIGNATURE;

	SPS_INFO(DRV_MAME ":----Register BAM ----\n");

	bam = register_bam(&h, use_irq);
	if (bam == NULL) {
		res = -1;
		goto register_err;
	}

	tx_ep->sps = sps_alloc_endpoint();
	rx_ep->sps = sps_alloc_endpoint();
	tx_h_sps = tx_ep->sps;
	rx_h_sps = rx_ep->sps;

	res = sps_get_config(tx_h_sps, &tx_connect);
	if (res)
		goto exit_err;
	res = sps_get_config(rx_h_sps, &rx_connect);
	if (res)
		goto exit_err;

	SPS_INFO(DRV_MAME ":----Connect TX = MEM->BAM = Consumer ----\n");
	tx_connect_options =
	SPS_O_AUTO_ENABLE | SPS_O_EOT | SPS_O_ACK_TRANSFERS;
	if (!use_irq)
		tx_connect_options |= SPS_O_POLL;

	tx_connect.source = SPS_DEV_HANDLE_MEM;	/* Memory */
	tx_connect.destination = h;
	tx_connect.mode = SPS_MODE_DEST;	/* Consumer pipe */
	tx_connect.src_pipe_index = rx_pipe;
	tx_connect.dest_pipe_index = tx_pipe;
	tx_connect.event_thresh = 0x10;
	tx_connect.options = tx_connect_options;

	tx_desc_fifo.size = 0x40; /* Use small fifo to force wrap-around */
	test_alloc_mem(use_pipe_mem, &tx_desc_fifo);
	memset(tx_desc_fifo.base, 0x00, tx_desc_fifo.size);
	tx_connect.desc = tx_desc_fifo;

	res = sps_connect(tx_h_sps, &tx_connect);
	if (res)
		goto exit_err;

	SPS_INFO(DRV_MAME ":----Connect RX = BAM->MEM = Producer----\n");
	rx_connect_options =
	SPS_O_AUTO_ENABLE | SPS_O_EOT | SPS_O_ACK_TRANSFERS;
	if (!use_irq)
		rx_connect_options |= SPS_O_POLL;

	rx_connect.source = h;
	rx_connect.destination = SPS_DEV_HANDLE_MEM;	/* Memory */
	rx_connect.mode = SPS_MODE_SRC;	/* Producer pipe */
	rx_connect.src_pipe_index = rx_pipe;
	rx_connect.dest_pipe_index = tx_pipe;
	rx_connect.event_thresh = 0x10;
	rx_connect.options = rx_connect_options;

	rx_desc_fifo.size = 0x40; /* Use small fifo to force wrap-around */
	test_alloc_mem(use_pipe_mem, &rx_desc_fifo);
	memset(rx_desc_fifo.base, 0x00, rx_desc_fifo.size);
	rx_connect.desc = rx_desc_fifo;

	res = sps_connect(rx_h_sps, &rx_connect);
	if (res)
		goto exit_err;

	SPS_INFO(DRV_MAME ":-----Allocate Buffers----\n");
	tx_mem.size = tx_size;
	rx_mem.size = rx_size;

	SPS_INFO(DRV_MAME ":Allocating Data Buffers from %s.\n", mem_type);
	if (use_cache_mem_data_buf) {
		struct sps_mem_buffer *mem;

		mem = &tx_mem;
		mem->base = kzalloc(mem->size, GFP_KERNEL);
		mem->phys_base = 0xdeadbeef;	/* map later */

		mem = &rx_mem;
		mem->base = kzalloc(mem->size, GFP_KERNEL);
		mem->phys_base = 0xdeadbeef;	/* map later */
	} else {
		test_alloc_mem(use_pipe_mem, &tx_mem);
		test_alloc_mem(use_pipe_mem, &rx_mem);
	}

	SPS_INFO(DRV_MAME ":%s.tx mem phys=0x%x.virt=0x%x.\n",
		__func__, tx_mem.phys_base, (u32) tx_mem.base);
	SPS_INFO(DRV_MAME ":%s.rx mem phys=0x%x.virt=0x%x.\n",
		__func__, rx_mem.phys_base, (u32) rx_mem.base);

	tx_buf = tx_mem.base;
	rx_buf = rx_mem.base;

	tx_event.mode = SPS_TRIGGER_CALLBACK;
	tx_event.options = SPS_O_EOT;
	tx_event.user = (void *)tx_ep;
	init_completion(&tx_ep->xfer_done);
	tx_event.xfer_done = &tx_ep->xfer_done;
	SPS_INFO(DRV_MAME ":tx_event.event=0x%x.\n", (u32) tx_event.xfer_done);

	rx_event.mode = SPS_TRIGGER_CALLBACK;
	rx_event.options = SPS_O_EOT;
	rx_event.user = (void *)rx_ep;
	init_completion(&rx_ep->xfer_done);
	rx_event.xfer_done = &rx_ep->xfer_done;
	SPS_INFO(DRV_MAME ":rx_event.event=0x%x.\n", (u32) rx_event.xfer_done);

	/* WARNING: MUST register reg_event to enable interrupt
	   on the pipe level. */
	res = sps_register_event(tx_h_sps, &tx_event);
	if (res)
		goto exit_err;
	res = sps_register_event(rx_h_sps, &rx_event);
	if (res)
		goto exit_err;

	if (loop_test)
		/* Each loop the tx_size is decremented by 4 */
		max_loops = (tx_size / 4) - 4;

	for (loop = 0; loop < max_loops; loop++) {
		init_completion(&tx_ep->xfer_done);
		init_completion(&rx_ep->xfer_done);

		SPS_INFO(DRV_MAME ":-----Prepare Buffers----\n");

		memset(tx_buf, 0xaa, tx_mem.size);
		memset(rx_buf, 0xbb, rx_mem.size);

		if (use_cache_mem_data_buf) {
			struct sps_mem_buffer *mem;

			mem = &tx_mem;
			mem->phys_base = dma_map_single(NULL, mem->base,
							mem->size, 0);
			mem = &rx_mem;
			mem->phys_base = dma_map_single(NULL, mem->base,
							mem->size, 0);
		}

		SPS_INFO(DRV_MAME ":-----Start Transfers----\n");
		tx_xfer_flags =	SPS_IOVEC_FLAG_INT | SPS_IOVEC_FLAG_EOB |
				SPS_IOVEC_FLAG_EOT;
		res = sps_transfer_one(tx_h_sps, tx_mem.phys_base, tx_size,
				       tx_h_sps, tx_xfer_flags);

		SPS_INFO("sps_transfer_one tx res = %d.\n", res);
		if (res)
			goto exit_err;

		rx_xfer_flags = SPS_IOVEC_FLAG_INT | SPS_IOVEC_FLAG_EOB;
		res = sps_transfer_one(rx_h_sps, rx_mem.phys_base, rx_size,
				       rx_h_sps, rx_xfer_flags);
		SPS_INFO("sps_transfer_one rx res = %d.\n", res);
		if (res)
			goto exit_err;

		/* wait until transfer completes, instead of polling */
		SPS_INFO(DRV_MAME ":-----Wait Transfers Complete----\n");

		msleep(10); /* allow transfer to complete */

		print_bam_reg(bam, tx_pipe);
		print_bam_reg(bam, rx_pipe);

		desc = tx_desc_fifo.base;
		SPS_INFO("tx desc-fifo at 0x%x = 0x%x,0x%x,0x%x,0x%x.\n",
			tx_desc_fifo.phys_base, desc[0], desc[1], desc[2],
			desc[3]);
		desc = rx_desc_fifo.base;
		SPS_INFO("rx desc-fifo at 0x%x = 0x%x,0x%x,0x%x,0x%x.\n",
			rx_desc_fifo.phys_base, desc[0], desc[1], desc[2],
			desc[3]);

		res = wait_xfer_completion(use_irq, &rx_ep->xfer_done,
					   rx_h_sps, 1, 3, 0);

		if (res) {
			pr_err("sps_test:%s.transfer not completed.\n",
			       __func__);
			goto exit_err;
		}

		sps_get_iovec(tx_ep->sps, &tx_ep->IOVec);
		sps_get_iovec(rx_ep->sps, &rx_ep->IOVec);

		if (use_cache_mem_data_buf) {
			struct sps_mem_buffer *mem;

			mem = &tx_mem;
			dma_unmap_single(NULL, mem->phys_base, mem->size, 0);
			mem = &rx_mem;
			dma_unmap_single(NULL, mem->phys_base, mem->size, 0);
		}

		tx_iovec = tx_ep->IOVec;
		rx_iovec = rx_ep->IOVec;

		SPS_INFO(DRV_MAME ":-----Check Pass/Fail----\n");

		SPS_INFO("sps_get_iovec tx size = %d.\n", tx_iovec.size);
		SPS_INFO("sps_get_iovec rx size = %d.\n", rx_iovec.size);

		SPS_INFO("tx buf[0] = 0x%x.\n", tx_buf[0]);
		SPS_INFO("rx buf[0] = 0x%x.\n", rx_buf[0]);
		SPS_INFO("tx buf[tx_size/4-1] = 0x%x.\n",
			tx_buf[tx_size / 4 - 1]);
		SPS_INFO("rx buf[tx_size/4-1] = 0x%x.\n",
			rx_buf[tx_size / 4 - 1]);

		/* NOTE: Rx according to Tx size with EOT. */
		if ((tx_iovec.size == tx_size) &&
		    (rx_iovec.size == tx_size) &&
		    (tx_buf[0] == rx_buf[0]) &&
		    (tx_buf[tx_size / 4 - 1] == rx_buf[tx_size / 4 - 1])
		   ) {
			res = 0;
			if (loop_test)
				SPS_INFO("sps:loop#%d PASS.\n", loop);
		} else {
			res = -1;
			break;
		}

		tx_size -= 4;
	}

exit_err:

	SPS_INFO(DRV_MAME ":-----Tear Down----\n");

	if (use_cache_mem_data_buf) {
		struct sps_mem_buffer *mem;

		mem = &tx_mem;
		kfree(mem->base);
		mem = &rx_mem;
		kfree(mem->base);
	} else {
		test_free_mem(use_pipe_mem, &tx_mem);
		test_free_mem(use_pipe_mem, &rx_mem);
	}
	test_free_mem(use_pipe_mem, &tx_desc_fifo);
	test_free_mem(use_pipe_mem, &rx_desc_fifo);

	sps_disconnect(tx_h_sps);
	sps_disconnect(rx_h_sps);

	unregister_bam(bam, h);

register_err:
	SPS_INFO("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	if (res == 0)
		SPS_INFO("SPS #%d BAM-DMA SYS-BAM Test %s DESC %s - PASS.\n",
			sps_test_num?sps_test_num:test_type, irq_mode, mem_type);
	else
		pr_info("SPS #%d BAM-DMA SYS-BAM Test %s DESC %s - FAIL.\n",
			sps_test_num?sps_test_num:test_type, irq_mode, mem_type);
	SPS_INFO("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

	return res;
}

static int sps_test_allloc(void)
{
	int res = -1;

	struct sps_pipe *sps;
	struct sps_mem_buffer mem[10];
	enum sps_mem mem_id = SPS_MEM_LOCAL;
	void *buf1 = NULL;
	void *buf2 = NULL;
	int i;

	sps = NULL;

	memset(mem, 0, sizeof(mem));

	for (i = 1; i < (sizeof(mem) / sizeof(mem[0])); i++) {
		mem[i].size = i * 0x100;
		sps_alloc_mem(sps, mem_id, &mem[i]);
	}
	buf1 = mem[0].base;
	for (i = 1; i < (sizeof(mem) / sizeof(mem[0])); i++) {
		mem[i].size = i * 0x100;
		sps_free_mem(sps, &mem[i]);
	}
	for (i = 1; i < (sizeof(mem) / sizeof(mem[0])); i++) {
		mem[i].size = i * 0x100;
		sps_alloc_mem(sps, mem_id, &mem[i]);
	}
	buf2 = mem[0].base;
	for (i = 1; i < (sizeof(mem) / sizeof(mem[0])); i++) {
		mem[i].size = i * 0x100;
		sps_free_mem(sps, &mem[i]);
	}

	if (buf1 == buf2)	/* All allocation were properly free. */
		res = 0;
	else
		res = -1;

	SPS_INFO("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	if (res == 0)
		SPS_INFO("SPS #%d PIPE-MEM-ALLOC Test - PASS.\n",
			sps_test_num?sps_test_num:test_type);
	else
		pr_info("SPS #%d PIPE-MEM-ALLOC Test - FAIL.\n",
			sps_test_num?sps_test_num:test_type);

	SPS_INFO("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

	return res;
}

static int sps_test_bamdma_alloc_free_chan(void)
{
	int res = -1;

	struct sps_alloc_dma_chan alloc[BAMDMA_MAX_CHANS];
	struct sps_dma_chan chan[BAMDMA_MAX_CHANS];
	u32 h;
	u32 tx_pipe; /* consumer = dest */
	u32 rx_pipe; /* producer = src */
	int i;

	for (i = 0; i < 10; i++) {
		memset(&alloc[i], 0 , sizeof(alloc[i]));
		memset(&chan[i], 0 , sizeof(chan[i]));

		alloc[i].threshold = (1024 / 10) * i;
		alloc[i].priority = i % 8;
		res = sps_alloc_dma_chan(&alloc[i], &chan[i]);
		if (res) {
			pr_err("sps_alloc_dma_chan.res=%d.\n", res);
			goto exit_err;
		}

		h = chan[i].dev;
		rx_pipe = chan[i].src_pipe_index;
		tx_pipe = chan[i].dest_pipe_index;
		SPS_INFO("bamdma.handle=0x%x.tx_pipe=%d.rx_pipe=%d.\n",
			h, tx_pipe, rx_pipe);

		if ((tx_pipe != i*2) && (rx_pipe != (i*2+1))) {
			pr_err("Pipes index not as expected.\n");
			res = -1;
			goto exit_err;
		}

	} /* for() */

	for (i = 0; i < 10; i++) {
		res = sps_free_dma_chan(&chan[i]);
		if (res) {
			pr_err("sps_free_dma_chan.res=%d.\n", res);
			goto exit_err;
		}
	} /* for() */

exit_err:

	SPS_INFO("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	if (res)
		pr_info("SPS #%d BAM-DMA-CONFIG Test - FAIL.\n",
			sps_test_num?sps_test_num:test_type);
	else
		SPS_INFO("SPS #%d BAM-DMA-CONFIG Test - PASS.\n",
			sps_test_num?sps_test_num:test_type);
	SPS_INFO("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

	return res;
}

/* MULTIPLE DESCRIPTORS PER TRANSFER or MULTIPLE TRANSFERS */
static int sps_test_multi(int tx_eot_per_xfer)
{
	int res = 0;

	int use_irq = false;
	int use_pipe_mem = false;
	int use_cache_mem_data_buf = false;
	int loop_test = false;

	struct test_endpoint test_ep[2];
	struct test_endpoint *tx_ep = &test_ep[0];
	struct test_endpoint *rx_ep = &test_ep[1];

	void *bam = NULL;
	u32 h = 0;
	struct sps_connect tx_connect = { 0};
	struct sps_connect rx_connect = { 0};

	struct sps_mem_buffer tx_desc_fifo = { 0};
	struct sps_mem_buffer rx_desc_fifo = { 0};

	struct sps_mem_buffer tx_mem = { 0};
	struct sps_mem_buffer rx_mem = { 0};

	struct sps_pipe *tx_h_sps = NULL;
	struct sps_pipe *rx_h_sps = NULL;

	struct sps_iovec tx_iovec = { 0};
	struct sps_iovec rx_iovec = { 0};
	u32 *desc = NULL;
	u32 *tx_buf = NULL;
	u32 *rx_buf = NULL;

	u32 tx_pipe = 4;
	u32 rx_pipe = 5;

	u32 tx_xfer_flags = 0;
	u32 rx_xfer_flags = 0;

	struct sps_register_event tx_event = { 0};
	struct sps_register_event rx_event = { 0};

	u32 tx_size = 0x400;
	u32 rx_size = 0x800;	/* This the max size and not the actual size */

	u32 tx_connect_options = 0;
	u32 rx_connect_options = 0;
	char *irq_mode = use_irq ? "IRQ mode" : "Polling mode";
	char *mem_type = use_pipe_mem ? "Pipe-Mem" : "Sys-Mem";
	int loop = 0;
	int max_loops = 1;
	int tx_multi = 1;
	int rx_multi = 1;
	u32 total_tx_size = 0;
	u32 total_rx_size = 0;
	int i;

	memset(test_ep, 0, sizeof(test_ep));
	tx_ep->signature = TEST_SIGNATURE;
	rx_ep->signature = TEST_SIGNATURE;

	SPS_INFO(DRV_MAME ":----Register BAM ----\n");
	bam = register_bam(&h, use_irq);
	if (bam == NULL) {
		res = -1;
		goto register_err;
	}

	tx_ep->sps = sps_alloc_endpoint();
	rx_ep->sps = sps_alloc_endpoint();
	tx_h_sps = tx_ep->sps;
	rx_h_sps = rx_ep->sps;

	res = sps_get_config(tx_h_sps, &tx_connect);
	if (res)
		goto exit_err;
	res = sps_get_config(rx_h_sps, &rx_connect);
	if (res)
		goto exit_err;

	SPS_INFO(DRV_MAME ":----Connect TX = MEM->BAM = Consumer ----\n");
	tx_connect_options =
	SPS_O_AUTO_ENABLE | SPS_O_EOT | SPS_O_ACK_TRANSFERS;
	if (!use_irq)
		tx_connect_options |= SPS_O_POLL;

	tx_connect.source = SPS_DEV_HANDLE_MEM;	/* Memory */
	tx_connect.destination = h;
	tx_connect.mode = SPS_MODE_DEST;	/* Consumer pipe */
	tx_connect.src_pipe_index = rx_pipe;
	tx_connect.dest_pipe_index = tx_pipe;
	tx_connect.event_thresh = 0x10;
	tx_connect.options = tx_connect_options;

	tx_desc_fifo.size = 0x40 + 8; /* Use small fifo to force wrap-around */
	test_alloc_mem(use_pipe_mem, &tx_desc_fifo);
	memset(tx_desc_fifo.base, 0x00, tx_desc_fifo.size);
	tx_connect.desc = tx_desc_fifo;

	res = sps_connect(tx_h_sps, &tx_connect);
	if (res)
		goto exit_err;

	SPS_INFO(DRV_MAME ":----Connect RX = BAM->MEM = Producer----\n");
	rx_connect_options =
	SPS_O_AUTO_ENABLE | SPS_O_EOT | SPS_O_ACK_TRANSFERS;
	if (!use_irq)
		rx_connect_options |= SPS_O_POLL;

	rx_connect.source = h;
	rx_connect.destination = SPS_DEV_HANDLE_MEM;	/* Memory */
	rx_connect.mode = SPS_MODE_SRC;	/* Producer pipe */
	rx_connect.src_pipe_index = rx_pipe;
	rx_connect.dest_pipe_index = tx_pipe;
	rx_connect.event_thresh = 0x10;
	rx_connect.options = rx_connect_options;

	rx_desc_fifo.size = 0x40 + 8; /* Use small fifo to force wrap-around */
	test_alloc_mem(use_pipe_mem, &rx_desc_fifo);
	memset(rx_desc_fifo.base, 0x00, rx_desc_fifo.size);
	rx_connect.desc = rx_desc_fifo;

	res = sps_connect(rx_h_sps, &rx_connect);
	if (res)
		goto exit_err;

	SPS_INFO(DRV_MAME ":-----Allocate Buffers----\n");
	tx_mem.size = tx_size;
	rx_mem.size = rx_size;

	SPS_INFO(DRV_MAME ":Allocating Data Buffers from %s.\n", mem_type);
	if (use_cache_mem_data_buf) {
		struct sps_mem_buffer *mem;

		mem = &tx_mem;
		mem->base = kzalloc(mem->size, GFP_KERNEL);
		mem->phys_base = 0xdeadbeef;	/* map later */

		mem = &rx_mem;
		mem->base = kzalloc(mem->size, GFP_KERNEL);
		mem->phys_base = 0xdeadbeef;	/* map later */
	} else {
		test_alloc_mem(use_pipe_mem, &tx_mem);
		test_alloc_mem(use_pipe_mem, &rx_mem);
	}

	SPS_INFO(DRV_MAME ":%s.tx mem phys=0x%x.virt=0x%x.\n",
		__func__, tx_mem.phys_base, (u32) tx_mem.base);
	SPS_INFO(DRV_MAME ":%s.rx mem phys=0x%x.virt=0x%x.\n",
		__func__, rx_mem.phys_base, (u32) rx_mem.base);

	tx_buf = tx_mem.base;
	rx_buf = rx_mem.base;

	tx_event.mode = SPS_TRIGGER_CALLBACK;
	tx_event.options = SPS_O_EOT;
	tx_event.user = (void *)tx_ep;
	init_completion(&tx_ep->xfer_done);
	tx_event.xfer_done = &tx_ep->xfer_done;
	SPS_INFO(DRV_MAME ":tx_event.event=0x%x.\n", (u32) tx_event.xfer_done);

	rx_event.mode = SPS_TRIGGER_CALLBACK;
	rx_event.options = SPS_O_EOT;
	rx_event.user = (void *)rx_ep;
	init_completion(&rx_ep->xfer_done);
	rx_event.xfer_done = &rx_ep->xfer_done;
	SPS_INFO(DRV_MAME ":rx_event.event=0x%x.\n", (u32) rx_event.xfer_done);

	/*
	 * WARNING: MUST register reg_event to enable interrupt on the
	 * pipe level.
	 */
	res = sps_register_event(tx_h_sps, &tx_event);
	if (res)
		goto exit_err;
	res = sps_register_event(rx_h_sps, &rx_event);
	if (res)
		goto exit_err;

	if (loop_test)
		/* Each loop the tx_size is decremented by 4 */
		max_loops = (tx_size / 0x40) - 4;

	for (loop = 0; loop < max_loops; loop++) {
		init_completion(&tx_ep->xfer_done);
		init_completion(&rx_ep->xfer_done);

		if (loop_test)
			tx_size -= 0x40;

		SPS_INFO(DRV_MAME ":-----Prepare Buffers----\n");

		memset(tx_buf, 0xaa, tx_mem.size);
		memset(rx_buf, 0xbb, rx_mem.size);

		if (use_cache_mem_data_buf) {
			struct sps_mem_buffer *mem;

			mem = &tx_mem;
			mem->phys_base =
			dma_map_single(NULL, mem->base, mem->size, 0);
			mem = &rx_mem;
			mem->phys_base =
			dma_map_single(NULL, mem->base, mem->size, 0);
		}

		SPS_INFO(DRV_MAME ":-----Start Transfers----\n");
		tx_xfer_flags =
		SPS_IOVEC_FLAG_INT | SPS_IOVEC_FLAG_EOB |
		SPS_IOVEC_FLAG_EOT;
		tx_multi = (tx_desc_fifo.size / 8) - 1;
		for (i = 0; i < tx_multi; i++) {
			u32 size = tx_size / tx_multi;
			u32 addr = tx_mem.phys_base + i * size;
			u32 flags = 0;

			if (tx_eot_per_xfer)
				/* every desc has EOT */
				flags = tx_xfer_flags;
			else
				/* only the last has EOT */
				flags = (i < tx_multi - 1) ? 0 : tx_xfer_flags;
			res =
			sps_transfer_one(tx_h_sps, addr, size, tx_h_sps,
					flags);
			SPS_INFO("sps_transfer_one tx res = %d.\n", res);
			if (res)
				goto exit_err;
		}

		rx_xfer_flags = SPS_IOVEC_FLAG_INT | SPS_IOVEC_FLAG_EOB;
		rx_multi = tx_eot_per_xfer ? tx_multi : 1;
		for (i = 0; i < rx_multi; i++) {
			u32 size = rx_size;
			u32 addr = 0;
			u32 flags = rx_xfer_flags;

			if (tx_eot_per_xfer) {
				/* for continues data in rx-buf
				 * the size *must* match the tx transfers
				 */

				size = tx_size / tx_multi;
			}
			addr = rx_mem.phys_base + i * size;

			res =
			sps_transfer_one(rx_h_sps, addr, size, rx_h_sps,
					flags);
			SPS_INFO("sps_transfer_one rx res = %d.\n", res);
			if (res)
				goto exit_err;
		}

		/* wait until transfer completes, instead of polling */
		SPS_INFO(DRV_MAME ":-----Wait Transfers Complete----\n");

		msleep(10);  /* allow transfer to complete */

		print_bam_reg(bam, tx_pipe);
		print_bam_reg(bam, rx_pipe);

		desc = tx_desc_fifo.base;
		SPS_INFO("tx desc-fifo at 0x%x:\n", tx_desc_fifo.phys_base);
		for (i = 0; i < (tx_desc_fifo.size / 8); i++)
			SPS_INFO("%d:0x%x,0x%x.\n", i, desc[i * 2],
				desc[i * 2 + 1]);

		desc = rx_desc_fifo.base;
		SPS_INFO("rx desc-fifo at 0x%x:\n", rx_desc_fifo.phys_base);
		for (i = 0; i < (rx_desc_fifo.size / 8); i++)
			SPS_INFO("%d:0x%x,0x%x.\n", i, desc[i * 2],
				desc[i * 2 + 1]);

		res = wait_xfer_completion(use_irq, &rx_ep->xfer_done,
					   rx_h_sps, rx_multi, 3, 0);

		if (res) {
			pr_err("sps_test:%s.transfer not completed.\n",
			       __func__);
			goto exit_err;
		}

		for (i = 0; i < tx_multi; i++) {
			sps_get_iovec(tx_ep->sps, &tx_ep->IOVec);
			total_tx_size += tx_ep->IOVec.size;
		}

		for (i = 0; i < rx_multi; i++) {
			sps_get_iovec(rx_ep->sps, &rx_ep->IOVec);
			total_rx_size += rx_ep->IOVec.size;
		}

		if (use_cache_mem_data_buf) {
			struct sps_mem_buffer *mem;

			mem = &tx_mem;
			dma_unmap_single(NULL, mem->phys_base, mem->size, 0);
			mem = &rx_mem;
			dma_unmap_single(NULL, mem->phys_base, mem->size, 0);
		}

		tx_iovec = tx_ep->IOVec;
		rx_iovec = rx_ep->IOVec;

		SPS_INFO(DRV_MAME ":-----Check Pass/Fail----\n");

		SPS_INFO("sps_get_iovec total_tx_size = %d.\n", total_tx_size);
		SPS_INFO("sps_get_iovec total_rx_size = %d.\n", total_rx_size);

		SPS_INFO("tx buf[0] = 0x%x.\n", tx_buf[0]);
		SPS_INFO("rx buf[0] = 0x%x.\n", rx_buf[0]);
		SPS_INFO("tx buf[tx_size/4-1] = 0x%x.\n",
			tx_buf[tx_size / 4 - 1]);
		SPS_INFO("rx buf[tx_size/4-1] = 0x%x.\n",
			rx_buf[tx_size / 4 - 1]);

		/* NOTE: Rx according to Tx size with EOT. */
		if ((total_tx_size == tx_size) &&
		    (total_rx_size == tx_size) &&
		    (tx_buf[0] == rx_buf[0]) &&
		    (tx_buf[tx_size / 4 - 1] == rx_buf[tx_size / 4 - 1])
		   ) {
			res = 0;
			if (loop_test)
				SPS_INFO("sps:loop#%d PASS.\n", loop);
		} else {
			res = -1;
			break;
		}
	}

exit_err:

	SPS_INFO(DRV_MAME ":-----Tear Down----\n");

	if (use_cache_mem_data_buf) {
		struct sps_mem_buffer *mem;

		mem = &tx_mem;
		kfree(mem->base);
		mem = &rx_mem;
		kfree(mem->base);
	} else {
		test_free_mem(use_pipe_mem, &tx_mem);
		test_free_mem(use_pipe_mem, &rx_mem);
	}
	test_free_mem(use_pipe_mem, &tx_desc_fifo);
	test_free_mem(use_pipe_mem, &rx_desc_fifo);

	sps_disconnect(tx_h_sps);
	sps_disconnect(rx_h_sps);

	unregister_bam(bam, h);

register_err:
	SPS_INFO("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	if (res == 0)
		SPS_INFO("SPS #%d BAM-DMA MULTI Test %s DESC %s - PASS.\n",
			sps_test_num?sps_test_num:test_type, irq_mode, mem_type);
	else
		pr_info("SPS #%d BAM-DMA MULTI Test %s DESC %s - FAIL.\n",
			sps_test_num?sps_test_num:test_type, irq_mode, mem_type);
	SPS_INFO("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

	return res;
}

static int adversarial(void)
{
	int ret = TEST_FAIL;
	int res = 0;

	struct test_endpoint test_ep[2];
	struct test_endpoint *tx_ep = &test_ep[0];
	struct test_endpoint *rx_ep = &test_ep[1];

	void *bam = NULL;
	u32 h = 0;
	struct sps_connect tx_connect = { 0};
	struct sps_connect rx_connect = { 0};

	struct sps_mem_buffer tx_desc_fifo = { 0};
	struct sps_mem_buffer rx_desc_fifo = { 0};

	struct sps_mem_buffer tx_mem = { 0};
	struct sps_mem_buffer rx_mem = { 0};

	struct sps_pipe *tx_h_sps = NULL;
	struct sps_pipe *rx_h_sps = NULL;

	u32 tx_pipe = 1;
	u32 rx_pipe = 2;

	u32 tx_connect_options = 0;

	memset(test_ep, 0, sizeof(test_ep));
	tx_ep->signature = TEST_SIGNATURE;
	rx_ep->signature = TEST_SIGNATURE;

	SPS_INFO(DRV_MAME ":----Register BAM ----\n");

	bam = register_bam(&h, 0);
	if (bam == NULL) {
		res = -1;
		goto register_err;
	}

	tx_ep->sps = sps_alloc_endpoint();
	rx_ep->sps = sps_alloc_endpoint();
	tx_h_sps = tx_ep->sps;
	rx_h_sps = rx_ep->sps;

	res = sps_get_config(tx_h_sps, &tx_connect);
	if (res)
		goto exit_err;
	res = sps_get_config(rx_h_sps, &rx_connect);
	if (res)
		goto exit_err;

	SPS_INFO(DRV_MAME ":----Connect TX = MEM->BAM = Consumer ----\n");
	tx_connect_options =
	SPS_O_AUTO_ENABLE | SPS_O_EOT | SPS_O_ACK_TRANSFERS;
	tx_connect_options |= SPS_O_POLL;

	tx_connect.source = SPS_DEV_HANDLE_MEM;	/* Memory */
	tx_connect.destination = h;
	tx_connect.mode = SPS_MODE_DEST;	/* Consumer pipe */
	tx_connect.src_pipe_index = rx_pipe;
	tx_connect.dest_pipe_index = tx_pipe;
	tx_connect.event_thresh = 0x10;
	tx_connect.options = tx_connect_options;

	tx_desc_fifo.size = 0x40;
	test_alloc_mem(1, &tx_desc_fifo);
	memset(tx_desc_fifo.base, 0x00, tx_desc_fifo.size);
	tx_connect.desc = tx_desc_fifo;

	res = sps_connect(tx_h_sps, &tx_connect);
	if (res)
		ret = 0;

exit_err:
	SPS_INFO(DRV_MAME ":-----Tear Down----\n");
	test_free_mem(1, &tx_mem);
	test_free_mem(1, &rx_mem);
	test_free_mem(1, &tx_desc_fifo);
	test_free_mem(1, &rx_desc_fifo);
	sps_disconnect(tx_h_sps);
	unregister_bam(bam, h);

register_err:
	SPS_INFO("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	if (ret)
		pr_info("SPS Adversarial Test - FAIL.\n");
	else
		SPS_INFO("SPS Adversarial Test - PASS.\n");
	SPS_INFO("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

	return ret;
}

/**
 * Write File.
 *
 * @note Trigger the test from user space by:
 * echo n > /dev/sps_test
 *
 */
ssize_t sps_test_write(struct file *filp, const char __user *buf,
		       size_t size, loff_t *f_pos)
{
	int ret = 0;
	unsigned long missing;
	static char str[10];
	int i, n = 0;

	memset(str, 0, sizeof(str));
	missing = copy_from_user(str, buf, sizeof(str));
	if (missing)
		return -EFAULT;

	/* Do string to int conversion up-to newline or NULL */
	for (i = 0; i < sizeof(str) && (str[i] >= '0') && (str[i] <= '9'); ++i)
		n = (n * 10) + (str[i] - '0');

	sps_test->testcase = n;

	pr_info(DRV_MAME ":sps_test testcase=%d.\n", (u32) sps_test->testcase);

	sps_test_num =  (int) sps_test->testcase;

	switch (sps_test->testcase) {
	case 0:
		/* adversarial testing */
		ret = adversarial();
		break;
	case 1:
		ret = sps_test_allloc();
		break;
	case 2:
		ret = sps_test_bamdma_alloc_free_chan();
		break;
	case 3:
		/* sys2bam , IRQ , sys-mem */
		ret = sps_test_sys2bam(true, false, false, false);
		break;
	case 4:
		/* sys2bam , polling , sys-mem */
		ret = sps_test_sys2bam(false, false, false, false);
		break;
	case 5:
		/* EOT per Transfer = Multiple Transfers */
		ret = sps_test_multi(true);
		break;
	case 6:
		/*
		 * EOT on last descriptor only,
		 * Multiple Descriptors per Transfer.
		 */
		ret = sps_test_multi(false);
		break;
	default:
		pr_info(DRV_MAME ":Invalid Test Number.\n");
	}

	return size;
}

static long sps_test_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int ret = 0;
	int err = 0;

	/* Verify user arguments. */
	if (_IOC_TYPE(cmd) != MSM_SPS_TEST_IOC_MAGIC)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg,
				_IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg,
				_IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	switch (cmd) {
	case MSM_SPS_TEST_IGNORE_FAILURE:
		if (__get_user(debug_mode, (int __user *)arg))
			return -EINVAL;
		if (debug_mode) {
			pr_info(DRV_MAME ":Enter debug mode.\n");
			pr_info(DRV_MAME ":Continue testing all "
					"testcases in case of failure.\n");
		}
		break;
	case MSM_SPS_TEST_TYPE:
		if (__get_user(test_type, (int __user *)arg))
			return -EINVAL;

		switch (test_type) {
		case 1:
			/* nominal testing */

			/* sys2bam , IRQ , sys-mem */
			if (sps_test_sys2bam(true, false, false, false))
				ret = TEST_FAIL;

			/* sys2bam , polling , sys-mem */
			if (!ret || debug_mode) {
				if (sps_test_sys2bam(false, false, false, false))
					ret = TEST_FAIL;
			}

			/* EOT per Transfer = Multiple Transfers */
			if (!ret || debug_mode) {
				if (sps_test_multi(true))
					ret = TEST_FAIL;
			}

			/*
			 * EOT on last descriptor only,
			 * Multiple Descriptors per Transfer.
			 */
			if (!ret || debug_mode) {
				if (sps_test_multi(false))
					ret = TEST_FAIL;
			}

			if (!ret || debug_mode) {
				if (sps_test_allloc())
					ret = TEST_FAIL;
			}

			if (!ret || debug_mode) {
				if (sps_test_bamdma_alloc_free_chan())
					ret = TEST_FAIL;
			}
			break;
		case 2:
			/* adversarial testing */
			if (adversarial())
				ret = TEST_FAIL;
			break;
		default:
			pr_info(DRV_MAME ":Invalid test type.\n");
			return -ENOTTY;
		}

		break;
	default:
		return -ENOTTY;
	}

	return ret;
}

static const struct file_operations sps_test_fops = {
	.owner = THIS_MODULE,
	.write = sps_test_write,
	.unlocked_ioctl = sps_test_ioctl,
};

static struct miscdevice sps_test_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sps_test",
	.fops = &sps_test_fops,
};

/**
 * Module Init.
 */
static int __init sps_test_init(void)
{
	int ret;

	pr_info(DRV_MAME ":sps_test_init.\n");

	pr_info(DRV_MAME ":SW Version = %s.\n", DRV_VERSION);

	sps_test = kzalloc(sizeof(*sps_test), GFP_KERNEL);
	if (sps_test == NULL) {
		pr_err(DRV_MAME ":kzalloc err.\n");
		return -ENOMEM;
	}
	sps_test->signature = TEST_SIGNATURE;

	ret = misc_register(&sps_test_dev);
	if (ret) {
		pr_err(DRV_MAME "register sps_test_dev err.\n");
		return -ENODEV;
	}

	sps_test->misc_dev = &sps_test_dev;

	pr_info(DRV_MAME ":SPS-Test init OK.\n");

	return ret;
}

/**
 * Module Exit.
 */
static void __exit sps_test_exit(void)
{
	pr_info(DRV_MAME ":sps_test_exit.\n");

	msleep(100); /* allow gracefull exit of the worker thread */

	misc_deregister(&sps_test_dev);
	kfree(sps_test);

	pr_info(DRV_MAME ":sps_test_exit complete.\n");
}

module_init(sps_test_init);
module_exit(sps_test_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SPS Unit-Test");

