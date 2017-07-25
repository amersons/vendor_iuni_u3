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

#ifndef __V4L2_H__
#define __V4L2_H__

#include <linux/videodev2.h>
#include <linux/ion.h>

struct mem_buffer
{
	int     index;
	int     size;
	void   *buf;
};

struct v4l2_overlay_userptr_buffer {
	unsigned int base[3];//base address of plane or frame
	size_t offset[3];//offset of plane or frame in multi buffer use case
};
struct v4l2_overlay_userptr_buffer *userptr;
/* ION: start */
#define MEM_NAME_STR_LEN  (25)
#define ION_NAME_STR "/dev/ion"
#define ION_NAME_STR_LEN sizeof(ION_NAME_STR)
#define ION_NUM_DEFAULT 0
#define MAX_PLANE 3
struct memDev {
	int fd;
	int mem_fd;
	char mem_name[MEM_NAME_STR_LEN];
	int mem_page_size;
	int mem_size;
	unsigned char *mem_buf;

};
struct memDev * MEM[MAX_PLANE];
struct memDev * ION;
struct memDev ion[MAX_PLANE];
struct ion_handle_data handle_data[MAX_PLANE];
// MEM functions
int chooseMEMDev(void);
int openMEMDev(void);
int allocMEM(unsigned int, unsigned int);
void free_buffer(unsigned int);
void close_ion_device(void);
/* ION: end */

int v4l2_set_src (int fd,
                  int width, int height,
                  int crop_x, int crop_y, int crop_w, int crop_h,
                  unsigned int pixelformat);

int v4l2_set_dst (int fd,
                  int dst_x, int dst_y, int dst_w, int dst_h,
                  void *addr);

int v4l2_set_buffer (int fd,
                     enum v4l2_memory memory,
                     int num_buf,
                     struct mem_buffer **mem_buf);

int v4l2_clear_buffer (int fd,
                       enum v4l2_memory memory,
                       int num_buf,
                       struct mem_buffer *mem_buf);

int v4l2_set_rotation (int fd, int rotation);
int v4l2_set_flip (int fd, int hflip, int vflip);
int v4l2_stream_on (int fd);
int v4l2_stream_off (int fd);
int v4l2_dequeue (int fd, enum v4l2_memory memory, int *index);
int v4l2_queue (int fd, int index, enum v4l2_memory memory, __u32 bytes);

#endif //__V4L2_H__
