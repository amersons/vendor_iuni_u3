/*
 -----------------------------------------------------------------------------
 Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are
 met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 -----------------------------------------------------------------------------
*/

/******************************************************************************

  @file  v4l2overlay.c
  @brief This file contains test code to verify functionalities of v4l2overlay

  DESCRIPTION
  v4l2overlay is display v4l2 test program. It executes specific v4l2 overlay
  ioctls as well as the standard linux and msm fb ioctls.

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#include <linux/fb.h>
#include <linux/stddef.h>

#include <sys/mman.h>
#include <sys/ioctl.h>

#include "v4l2ops.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct Rect {
	int x;
	int y;
	int w;
	int h;
};

enum device {
	INVALID,
	MSM8x55,
	MSM7x27A,
};

/* Which platform are we running on */
struct platform_parameters {
	const char *platform_name;
	enum device device_name;
};

/* Fixed image parameters, depend on platform */
struct image_parameters {
	const char *image_file[3];//for multi-plane use case only on user ptr
	unsigned int num_plane;//for multi-plane use case only on user ptr
	unsigned int pixelformat;
	struct Rect dimensions;
	struct Rect default_crop;
};

/* Variable test parameters */
struct test_parameters {
	struct Rect crop;
	struct Rect destination;
	int buf_count;
	enum v4l2_memory memory;
	int rotation;
	int hflip;
	int vflip;
	int run;
};

/* ION API's: start */
int chooseMEMDev()
{
	ION = &(ion[ION_NUM_DEFAULT]);
	strcpy(ION->mem_name, ION_NAME_STR);

	return 0;
}

int openMEMDev()
{
	int i = 0;
	ION->fd = open(ION->mem_name, O_RDWR|O_DSYNC);
	if (ION->fd < 0) {
		printf("ERROR: Can't open ion %s\n", ION->mem_name);
		return -1;
	}
	else
		printf("[%s(%d)]: %s opened ION successfully!\n",__func__ ,
					__LINE__, ION->mem_name);
	for (i; i < MAX_PLANE; i++) {
		MEM[i] = &ion[i];
		MEM[i]->fd = ION->fd;
	}
	return 0;
}

int allocMEM(unsigned int size, unsigned int i)
{
	int result = -1;
	struct ion_fd_data fd_data;
	struct ion_allocation_data ionAllocData;
	fd_data.fd = 0;

	// First Allocate MEM
	MEM[i]->mem_page_size = sysconf(_SC_PAGESIZE);
	MEM[i]->mem_size = size;
	MEM[i]->mem_size = (MEM[i]->mem_size + MEM[i]->mem_page_size - 1) &
			(~(MEM[i]->mem_page_size - 1));
	ionAllocData.len = MEM[i]->mem_size;
	ionAllocData.align = sysconf(_SC_PAGESIZE);
	ionAllocData.flags =
		ION_HEAP(ION_IOMMU_HEAP_ID) |
		ION_HEAP(ION_CP_MM_HEAP_ID) |
		ION_HEAP(ION_CP_WB_HEAP_ID) |
		ION_HEAP(ION_SF_HEAP_ID)    |
		ION_HEAP(ION_CAMERA_HEAP_ID);
	result = ioctl(MEM[i]->fd, ION_IOC_ALLOC,  &ionAllocData);
	if (result) {
		printf("ERROR! MEM_ALLOCATE failed.\n");
		free_buffer(i);
		return -1;
	} else {
		fd_data.handle = ionAllocData.handle;
		handle_data[i].handle = ionAllocData.handle;

		if (ioctl(MEM[i]->fd, ION_IOC_MAP, &fd_data)) {
			printf("ERROR! ION_IOC_MAP failed.\n");
			free_buffer(i);
			return -1;
		} else {
			MEM[i]->mem_buf = mmap(NULL, MEM[i]->mem_size, PROT_READ |
					PROT_WRITE, MAP_SHARED, fd_data.fd, 0);
			MEM[i]->mem_fd = fd_data.fd;
			if (MEM[i]->mem_buf == MAP_FAILED) {
				printf("ERROR: MEM MMAP failed!\n");
				free_buffer(i);
				return -1;
			}

			memset(MEM[i]->mem_buf, 0x00, MEM[i]->mem_size);
			printf("MEM Allocation successful (%d bytes at %p)\n",
						MEM[i]->mem_size, MEM[i]->mem_buf);
		}
	}

	return 0;
}

void free_buffer(unsigned int i)
{
	//unmap
	if ((MEM[i]->mem_buf !=NULL) && (MEM[i]->mem_buf != MAP_FAILED)){
		munmap(MEM[i]->mem_buf, MEM[i]->mem_size);
		ioctl(MEM[i]->fd, ION_IOC_FREE, &handle_data[i]);
		close(MEM[i]->mem_fd);
	}
}

void close_ion_device()
{
	if (ION->fd >= 0)
		close(ION->fd);
}

/* ION API: end*/

static enum device get_hardware_device(void)
{
	char line[512];
	enum device d = INVALID;
	FILE *f = fopen("/proc/cpuinfo", "r");

	if (!f) {
		perror("open");
		return INVALID;
	}

	while (fgets(line, 512, f) != NULL) {
		if (strstr(line, "Hardware")) {
			if (strstr(line, "MSM8X55"))
				d = MSM8x55;
			else if (strstr(line, "MSM7x27a"))
				d = MSM7x27A;
			else {
				d = INVALID;
				printf("Could not parse platform, hardware line %s\n",
				 line);
			}
			break;
		}
	}

	fclose(f);

	if (d == INVALID)
		printf("%s: Failed\n", __func__);
	return d;
}

static char* read_image(const char *image_path, long *size)
{
	char *buffer;
	FILE *f = fopen(image_path, "r");

	if (!f) {
		perror("Could not read image");
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	*size = ftell (f);
	fseek (f, 0, SEEK_SET);

	buffer = malloc(*size + 1);

	if (!buffer) {
		perror("malloc");
		goto end;
	}

	*size = fread(buffer, 1, *size, f);

end:
	fclose(f);
	return buffer;
}

static void release_image(char *buffer)
{
	free(buffer);
}

/* echo 255 > /sys/class/leds/lcd-backlight/brightness */
static void lights_on(void)
{
	FILE *bfd = fopen("/sys/class/leds/lcd-backlight/brightness", "w");
	if(!bfd)
		perror("warning: could not enable backlight");
	else {
		fprintf(bfd, "255");
		fclose(bfd);
	}
}

#define clear_framebuffer(f,v) draw_framebuffer((f),(v), 0)

static void draw_framebuffer(int fb_fd, char *fbp,
	struct fb_var_screeninfo* fb_vinfo, int solid_color)
{
	int i,j;
	char *fb_buf_cur = fbp;

	printf("%s: Turning on backlight\n", __func__);
	lights_on();

	printf("%s: Drawing framebuffer\n", __func__);
	for(i=0;i<fb_vinfo->xres;i++) {
		for(j=0;j<fb_vinfo->yres;j++) {
			/* Assuming ARGB8888, write 4 zero bytes*/
			/* TODO: Add RGB565 support for 7x27A */
			*fb_buf_cur++ = 0x0;
			*fb_buf_cur++ = solid_color ? 0xFF : 0;
			*fb_buf_cur++ = 0x0;
			*fb_buf_cur++ = 0x0;
		}
	}

	if(ioctl(fb_fd, FBIOPAN_DISPLAY, fb_vinfo) < 0) {
		printf("ERROR: FBIOPAN_DISPLAY failed! line=%d\n", __LINE__);
		return;
	}

	printf("%s: done\n", __func__);
}

static int setup_framebuffer(struct fb_var_screeninfo *fb_vinfo)
{
	int fb_fd;
	char *fbp;
	struct fb_var_screeninfo vinfo;
	long screensize;
	printf("opening framebuffer device\n");

	fb_fd = open("/dev/fb0", O_RDWR);
	if (fb_fd < 0) {
		perror("Cannot open framebuffer device");
		return -1;
	}

	// Get variable screen information
	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
		printf("Error reading variable information.\n");
		close(fb_fd);
		return -1;
	}

	// Figure out the size of the screen in bytes
	screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

	// Map the device to memory
	fbp = (char *)mmap(NULL, screensize+0x100000, PROT_READ | PROT_WRITE, MAP_SHARED,
			fb_fd, 0);
	if ((int)fbp == -1) {
		printf("Error: failed to map framebuffer device to memory.\n");
		close(fb_fd);
		return -1;
	}

	printf("successfully opened framebuffer device, now drawing\n");
	draw_framebuffer(fb_fd, fbp, &vinfo, 0);
	munmap(fbp, screensize);
	*fb_vinfo = vinfo;
	return fb_fd;

}

static void close_framebuffer(int fb_fd)
{
	close(fb_fd);
	printf("closed framebuffer device\n");
}

int open_v4l2_overlay_device(void)
{
	/* Try /dev/video0 to /dev/video5
	   This could be better, for example
	   stat() */
	int i;
	char device_name[16];
	int fd;
	struct v4l2_capability cap;

	for (i = 0;i <= 5;i++) {

		sprintf(device_name, "/dev/video%d", i);
	    fd = open(device_name, O_RDWR);
	    if(fd < 0) continue;

		memset(&cap, 0, sizeof(struct v4l2_capability));
		if(0 != ioctl (fd, VIDIOC_QUERYCAP, &cap)) {
			close(fd);
			continue;
		}

		if(cap.capabilities & V4L2_CAP_STREAMING
			&&  cap.capabilities & V4L2_CAP_VIDEO_OUTPUT
			&&  cap.capabilities & V4L2_CAP_VIDEO_OVERLAY) {
		    printf("Found V4L2 video overlay device at %s\n", device_name);
		    printf("Driver: %s\n", cap.driver);
		    printf("Card: %s\n", cap.card);
		    return fd;
		}

		close(fd);
	}

	printf("%s: Could not find V4L2 overlay device", __func__);
	return -1;
}

void close_v4l2_overlay_device(int fd)
{
	close(fd);
}

static int runtest(struct platform_parameters *platform,
	struct image_parameters *image, struct test_parameters *test)
{
	int v4l2_fd;
	int fb_fd;
	struct fb_var_screeninfo vinfo;
	char *img_buffer[3] = {NULL , NULL, NULL};//single input frame or plane
	long img_buf_size[3];//frame size or plane size
	int err = 0;
	struct mem_buffer *mem_buf;
	int index = 0, count, i;
	int buffers_mapped = 0;
	int streaming = 0;

	for (i = 0; i < image->num_plane; i++) {
		img_buffer[i] = read_image(image->image_file[i],
					&img_buf_size[i]);

		if(!img_buffer[i]) {
			printf("ERROR: Could not read test image %d\n",i);
			return -1;
		}
	}

	printf("Opening V4L2 overlay device\n");
	v4l2_fd = open_v4l2_overlay_device();
	printf("Done\n");

	if(v4l2_fd < 0) {
		printf("ERROR: No useable V4L2 overlay device found\n");
		return -1;
	}

	chooseMEMDev();
	err = openMEMDev();
	if (err < 0)
		return -1;

	fb_fd = setup_framebuffer(&vinfo);

	if(fb_fd < 0) {
		printf("ERROR: Could not setup framebuffer\n");
		err = -1;
		goto done;
	}

	err = v4l2_set_rotation(v4l2_fd, test->rotation);
	if (err < 0)
		goto done;

	err = v4l2_set_flip(v4l2_fd, test->hflip, test->vflip);
	if (err < 0)
		goto done;

	err = v4l2_set_src(v4l2_fd, image->dimensions.w, image->dimensions.h,
			test->crop.x, test->crop.y, test->crop.w,
			test->crop.h, image->pixelformat);
	if (err < 0)
		goto done;

	err = v4l2_set_dst(v4l2_fd, test->destination.x, test->destination.y,
		test->destination.w, test->destination.h, NULL);
	if (err < 0)
		goto done;

	/* set the memory (mmap or userptr), and get a pointer */
	/* test->buf_count is the number of buffers we are
	   requesting */
	err = v4l2_set_buffer(v4l2_fd, test->memory, test->buf_count,
		&mem_buf);
	if (err < 0)
		goto done;

	if (test->memory == V4L2_MEMORY_USERPTR) {
		printf("allocate ION buffer\n");
		for (i = 0; i < image->num_plane; i++) {
			if (allocMEM(img_buf_size[i] * test->buf_count, i)) {
				printf("allocMEM failed! for buffer %d\n",i);
				return -1;
			}
		}
		userptr = (struct v4l2_overlay_userptr_buffer *)
			malloc(sizeof(struct v4l2_overlay_userptr_buffer));
		if (userptr == NULL) {
			printf("userptr memory allocation failed\n");
			return -1;
		}
		for (i = 0; i < MAX_PLANE; i++) {
			userptr->base[i]   = MEM[i]->mem_fd;
		}
	}
	buffers_mapped = 1;
	/* streaming on */
	err = v4l2_stream_on(v4l2_fd);
	if (err < 0)
		goto done;

	streaming = 1;

	for (count = 0;count <= 5;count++) {
		/* draw a frame */
		int nbytes;
		if (test->memory == V4L2_MEMORY_USERPTR) {
			for (i = 0; i < image->num_plane; i++) {
				memcpy((MEM[i]->mem_buf +
				(index * img_buf_size[i])), img_buffer[i],
				img_buf_size[i]);
				nbytes = img_buf_size[i];
				userptr->offset[i] = index * nbytes;
			}
		} else {
			nbytes = (img_buf_size[0] < mem_buf[index].size) ?
					img_buf_size[0] : mem_buf[index].size;

			memcpy(mem_buf[index].buf, img_buffer[0], nbytes);
		}
		err = v4l2_queue(v4l2_fd, index, test->memory, nbytes);
		if (err < 0)
			goto done;

		if (platform->device_name == MSM7x27A) {
			if (ioctl(fb_fd, FBIOPAN_DISPLAY, &vinfo) < 0)
				printf("%s:ERROR: FBIOPAN_DISPLAY failed!\n",
							__func__);
		}
		err = v4l2_dequeue(v4l2_fd, test->memory, &index);
		if (err < 0)
			goto done;

		if (++index >= test->buf_count)
			index = 0;
	}

done:
	if (streaming)
		(void)v4l2_stream_off(v4l2_fd);

	if(buffers_mapped)
		(void)v4l2_clear_buffer(v4l2_fd, test->memory, test->buf_count,
					mem_buf);
	for (i = 0; i < MAX_PLANE; i++)
		free_buffer(i);
	close_ion_device();

	if (userptr != NULL) {
		free(userptr);
		userptr = NULL;
	}
	for (i = 0; i < image->num_plane; i++) {
		if (img_buffer[i])
			release_image(img_buffer[i]);
	}
	if (fb_fd > 0)
		close_framebuffer(fb_fd);

	close_v4l2_overlay_device(v4l2_fd);

	return err;
}

enum test_indices {
	SIMPLE_OVERLAY,
	ROT_0,
	ROT_90,
	ROT_180,
	ROT_270,
	HFLIP,
	VFLIP,
	SCALE,
	V4L2_MEMORY_USERPTR_TEST
};

void showUsage(void)
{
	printf("usage: TESTS_LIST\n");
	printf("TESTS_LIST is one of more of:\n");
	printf("SIMPLE_OVERLAY\n");
	printf("ROT_0\n");
	printf("ROT_90\n");
	printf("ROT_180\n");
	printf("ROT_270\n");
	printf("HFLIP\n");
	printf("VFLIP\n");
	printf("SCALE\n");
	printf("V4L2_MEMORY_USERPTR_TEST\n");
	printf("ALL\n");
	printf("ALL runs all the test cases\n");
}

static int select_tests(struct test_parameters *active_tests, int argc, char *argv[])
{
	int nr_tests = 0;
	int i;
	int opt;

	struct option longOptions[] = {
		{ "usage",		no_argument,		NULL,	'u'},
		{ "verbose",		no_argument,		NULL,	'v'},
		{ "nominal",		no_argument,		NULL,	'n'},
		{ "adversarial",	no_argument,		NULL,	'a'},
		{ "repeatability",	no_argument,		NULL,	'r'},
		{ "stress",		no_argument,		NULL,	's'},
		{ "test",		required_argument,	NULL,	't'},
	};

	if (argc == 1) {
		printf("This program needs arguments....\n\n");
		showUsage();
		return 1;
	}

	optind = 0;
	while((opt = getopt_long(argc, argv, "uvnarst:", longOptions,
					NULL)) != -1) {
		switch(opt) {
			case 'u':
				showUsage();
				return 0;
			case 'v':
				printf("verbose is enabled\n");
				break;
			case 'n':
				active_tests[SIMPLE_OVERLAY].run = 1;
				break;
			case 'a':
				printf("adversarial test not supported \n");
				return 0;
			case 'r':
				active_tests[SIMPLE_OVERLAY].run = 1;
				active_tests[ROT_0].run = 1;
				active_tests[ROT_90].run = 1;
				active_tests[ROT_180].run = 1;
				active_tests[ROT_270].run = 1;
				active_tests[HFLIP].run = 1;
				active_tests[VFLIP].run = 1;
				active_tests[SCALE].run = 1;
				break;
			case 's':
				printf("stress test not supported currently\n");
				return 0;
			case 't':
				if (!strcmp(optarg, "SIMPLE_OVERLAY"))
					active_tests[SIMPLE_OVERLAY].run = 1;
				else if (!strcmp(optarg, "ROT_0"))
					active_tests[ROT_0].run = 1;
				else if (!strcmp(optarg, "ROT_90"))
					active_tests[ROT_90].run = 1;
				else if (!strcmp(optarg, "ROT_180"))
					active_tests[ROT_180].run = 1;
				else if (!strcmp(optarg, "ROT_270"))
					active_tests[ROT_270].run = 1;
				else if (!strcmp(optarg, "HFLIP"))
					active_tests[HFLIP].run = 1;
				else if (!strcmp(optarg, "VFLIP"))
					active_tests[VFLIP].run = 1;
				else if (!strcmp(optarg, "SCALE"))
					active_tests[SCALE].run = 1;
				else if (!strcmp(optarg, "V4L2_MEMORY_USERPTR_TEST"))
					active_tests[V4L2_MEMORY_USERPTR_TEST].run = 1;
				else if (!strcmp(optarg, "ALL")) {
					active_tests[SIMPLE_OVERLAY].run = 1;
					active_tests[ROT_0].run = 1;
					active_tests[ROT_90].run = 1;
					active_tests[ROT_180].run = 1;
					active_tests[ROT_270].run = 1;
					active_tests[HFLIP].run = 1;
					active_tests[VFLIP].run = 1;
					active_tests[SCALE].run = 1;
				} else {
					printf("Not a valid test\n");
					showUsage();
					return 0;
				}
				break;
			default :
				printf("Invalid argument: %c\n", opt);
				showUsage();
				return 0;
		}
	}
	return 1;
}

int main(int argc, char *argv[])
{
	struct platform_parameters platform;
	struct image_parameters *active_image;
	struct test_parameters  *active_tests;
	int test_nr, pass_cnt = 0, fail_cnt = 0;
	int total_tests;
	int nr_selected_tests;
	int result = -1;
	int i;

	/*
	 * multi-plane is supported only for user pointer memory.
	 * Set num_plane and image_file to be used accordingly.
	 * In memory map only single plane is supported.
	 */
	struct image_parameters test_image_8x55 = {
		.image_file[0] = "/usr/share/pixmaps/yuv420.raw",
		.image_file[1] = "/usr/share/pixmaps/yuv420.raw",
		.image_file[2] = "/usr/share/pixmaps/yuv420.raw",
		.num_plane   = 1,
		.pixelformat = V4L2_PIX_FMT_YUV420,
		.dimensions = { 0, 0, 176, 144},
		.default_crop = { 0, 0, 176, 144}
	};

	struct image_parameters test_image_7x27A = {
		.image_file[0] = "/usr/share/pixmaps/rgb565.raw",
		.image_file[1] = "/usr/share/pixmaps/rgb565.raw",
		.image_file[2] = "/usr/share/pixmaps/rgb565.raw",
		.num_plane   = 1,
		.pixelformat = V4L2_PIX_FMT_RGB565,
		.dimensions = { 0, 0, 320, 240},
		.default_crop = { 0, 0, 320, 240}
	};

	struct test_parameters tests_8x55[] = {
		{
			.crop = test_image_8x55.default_crop,
			.destination = test_image_8x55.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = -1,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_8x55.default_crop,
			.destination = test_image_8x55.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = 0,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_8x55.default_crop,
			.destination = test_image_8x55.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = 90,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_8x55.default_crop,
			.destination = test_image_8x55.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = 180,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_8x55.default_crop,
			.destination = test_image_8x55.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = 270,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_8x55.default_crop,
			.destination = test_image_8x55.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = -1,
			.hflip = 1,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_8x55.default_crop,
			.destination = test_image_8x55.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = -1,
			.hflip = 0,
			.vflip = 1,
			.run = 0
		},
		{
			.crop = test_image_8x55.default_crop,
			.destination = {0, 0, 300, 200},
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = -1,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_8x55.default_crop,
			.destination = test_image_8x55.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_USERPTR,
			.rotation = -1,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
	};

	struct test_parameters tests_7x27A[] = {
		{
			.crop = test_image_7x27A.default_crop,
			.destination = test_image_7x27A.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = -1,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_7x27A.default_crop,
			.destination = test_image_7x27A.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = 0,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_7x27A.default_crop,
			.destination = test_image_7x27A.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = 90,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_7x27A.default_crop,
			.destination = test_image_7x27A.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = 180,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_7x27A.default_crop,
			.destination = test_image_7x27A.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = 270,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_7x27A.default_crop,
			.destination = test_image_7x27A.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = -1,
			.hflip = 1,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_7x27A.default_crop,
			.destination = test_image_7x27A.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = -1,
			.hflip = 0,
			.vflip = 1,
			.run = 0
		},
		{
			.crop = test_image_7x27A.default_crop,
			.destination = {0, 0, 300, 200},
			.buf_count = 3,
			.memory = V4L2_MEMORY_MMAP,
			.rotation = -1,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
		{
			.crop = test_image_7x27A.default_crop,
			.destination = test_image_7x27A.dimensions,
			.buf_count = 3,
			.memory = V4L2_MEMORY_USERPTR,
			.rotation = -1,
			.hflip = 0,
			.vflip = 0,
			.run = 0
		},
	};

	printf("=============================================\n");
	printf("     V4L2 video overlay test application\n");
	printf("=============================================\n\n\n");
	platform.device_name = get_hardware_device();
	if (platform.device_name == INVALID) {
		printf("Unable to determine platform, aborting\n");
		exit(1);
	}

	if (platform.device_name == MSM8x55) {
		platform.platform_name = "MSM8x55";
		active_image = &test_image_8x55;
		active_tests = &tests_8x55[0];
		total_tests = ARRAY_SIZE(tests_8x55);
	} else {
		platform.platform_name = "MSM7x27A";
		active_image = &test_image_7x27A;
		active_tests = &tests_7x27A[0];
		total_tests = ARRAY_SIZE(tests_7x27A);
	}

	nr_selected_tests = select_tests(active_tests, argc, argv);
	if (!nr_selected_tests) exit(0);

	printf("Detected platform: %s\n\n", platform.platform_name);
	printf("Beginning test run, %d total tests\n", nr_selected_tests);
	for (i=0, test_nr = 0;i < total_tests;i++) {
		struct test_parameters *active_test = active_tests + i;
		if (!active_test->run) continue;

		printf("Test: %s:%d\n", platform.platform_name, test_nr++);
		result = runtest(&platform, active_image, active_test);
		if(result != 0) {
			fail_cnt++;
			printf("Test FAILED.\n");
			printf("Failed test parameters:\n");
			printf("Source image %s, rect (%d, %d, %d, %d)\n",
				active_image->image_file[0], active_image->dimensions.x,
				active_image->dimensions.y, active_image->dimensions.w,
				active_image->dimensions.h);
			printf("Crop rect (%d, %d, %d, %d), dest rect (%d, %d, %d, %d)\n",
				active_tests[test_nr].crop.x, active_tests[test_nr].crop.y,
				active_tests[test_nr].crop.w, active_tests[test_nr].crop.h,
				active_tests[test_nr].destination.x, active_tests[test_nr].destination.y,
				active_tests[test_nr].destination.w, active_tests[test_nr].destination.h);
			printf("Rotation: %d, hflip: %d, vflip: %d\n",
				active_tests[test_nr].rotation, active_tests[test_nr].hflip,
				active_tests[test_nr].vflip);
		} else {
			pass_cnt++;
			printf("Test passed.\n");
		}
		/* Let the last frame be seen on display for some time */
		sleep(5);
	}

	printf("DONE. All tests finished. %d passed, %d failed\n", pass_cnt, fail_cnt);

	return 0;
}

