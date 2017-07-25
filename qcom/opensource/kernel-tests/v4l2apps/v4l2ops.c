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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "v4l2ops.h"

static int _v4l2_pushy_ioctl(int fd, int cmd, void *data)
{
	int num_attempts = 0;
	int ret;

	do {
		ret = ioctl(fd, cmd, data);
		if(ret == 0 || (errno != EINTR && errno != EAGAIN))
			break;

	} while (++num_attempts < 100);

	return ret;
}

static int _v4l2_querycap(int fd, int capabilities)
{
	int ret;
	struct v4l2_capability cap;
	memset(&cap, 0, sizeof(struct v4l2_capability));

	ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0) {
		printf ("VIDIOC_QUERYCAP failed\n");
		return ret;
	}

	if (~cap.capabilities & capabilities)
		return -1;

	return 0;
}

static int _v4l2_cropcap(int fd, enum v4l2_buf_type type,
			struct v4l2_rect *crop)
{
	int ret;
	struct v4l2_cropcap cropcap;
	memset(&cropcap, 0, sizeof(struct v4l2_cropcap));

	cropcap.type = type;
	ret = ioctl(fd, VIDIOC_CROPCAP, &cropcap);
	if (ret < 0) {
		printf ("VIDIOC_CROPCAP failed\n");
		return ret;
	}

	if ((crop->left < cropcap.bounds.left) &&
		(crop->top < cropcap.bounds.top) &&
		(crop->width > cropcap.bounds.width) &&
		(crop->height > cropcap.bounds.height)) {
		printf ("(%d,%d %dx%d) is out of bound(%d,%d %dx%d)\n",
			crop->left, crop->top, crop->width, crop->height,
			cropcap.bounds.left, cropcap.bounds.top,
			cropcap.bounds.width, cropcap.bounds.height);
		return -1;
	}

	return ret;
}

static int _v4l2_g_fmt(int fd, struct v4l2_format *format)
{
	int ret = ioctl(fd, VIDIOC_G_FMT, format);
	if (ret < 0) {
		printf ("VIDIOC_G_FMT failed\n");
		return ret;
	}

	return 0;
}

static int _v4l2_s_fmt(int fd, struct v4l2_format *format)
{
	int ret = ioctl(fd, VIDIOC_S_FMT, format);
	if (ret < 0) {
		printf ("VIDIOC_S_FMT failed\n");
		return ret;
	}

	return 0;
}

static int _v4l2_g_fbuf(int fd, struct v4l2_framebuffer *frame)
{
	int ret = ioctl(fd, VIDIOC_G_FBUF, frame);
	if (ret < 0) {
		printf ("VIDIOC_G_FBUF failed\n");
		return ret;
	}

	return 0;
}

static int _v4l2_s_fbuf(int fd, struct v4l2_framebuffer *frame)
{
	int ret = ioctl(fd, VIDIOC_S_FBUF, frame);
	if (ret < 0) {
		printf ("VIDIOC_S_FBUF failed\n");
	}

	return ret;
}

static int _v4l2_s_crop(int fd, struct v4l2_crop *crop)
{
	int ret = ioctl(fd, VIDIOC_S_CROP, crop);
	if (ret < 0) {
		printf ("VIDIOC_S_CROP failed\n");
	}

	return ret;
}

static int _v4l2_g_ctrl(int fd, struct v4l2_control *ctrl)
{
	int ret = ioctl(fd, VIDIOC_G_CTRL, ctrl);
	if (ret < 0) {
		printf ("VIDIOC_G_CTRL failed\n");
	}

	return ret;
}

static int _v4l2_s_ctrl(int fd, struct v4l2_control *ctrl)
{
	int ret = ioctl(fd, VIDIOC_S_CTRL, ctrl);
	if (ret < 0) {
		printf ("VIDIOC_S_CTRL failed\n");
	}

	return ret;
}

static int _v4l2_streamon(int fd, enum v4l2_buf_type type)
{
	int ret = _v4l2_pushy_ioctl(fd, VIDIOC_STREAMON, &type);
	if (ret < 0) {
		printf ("VIDIOC_STREAMON failed\n");
	}
	return ret;
}

static int _v4l2_streamoff(int fd, enum v4l2_buf_type type)
{
	int ret = _v4l2_pushy_ioctl(fd, VIDIOC_STREAMOFF, &type);
	if (ret < 0) {
		printf ("VIDIOC_STREAMOFF failed \n");
		return ret;
	}

	return ret;
}

static int _v4l2_reqbuf(int fd, struct v4l2_requestbuffers *req)
{
	int ret = _v4l2_pushy_ioctl(fd, VIDIOC_REQBUFS, req);
	if (ret < 0) {
		printf ("VIDIOC_REQBUFS failed\n");
	}

	return ret;
}

static int _v4l2_querybuf (int fd, struct v4l2_buffer *set_buf)
{
	int ret = _v4l2_pushy_ioctl(fd, VIDIOC_QUERYBUF, set_buf);
	if (ret < 0) {
		printf ("VIDIOC_QUERYBUF failed\n");
	}

	return ret;
}


static int _v4l2_queue(int fd, struct v4l2_buffer *buf)
{
	int ret = _v4l2_pushy_ioctl(fd, VIDIOC_QBUF, buf);
	if (ret < 0) {
		printf ("VIDIOC_QBUF failed\n");
	}

	return ret;
}

static int _v4l2_dequeue(int fd, struct v4l2_buffer *buf)
{
	int ret =  _v4l2_pushy_ioctl(fd, VIDIOC_DQBUF, buf);
	if (ret < 0) {
		printf ("VIDIOC_DQBUF failed\n");
	}

	return ret;
}

int v4l2_set_src(int fd, int width, int height,
	int crop_x, int crop_y, int crop_w, int crop_h,
	unsigned int pixelformat)
{
	int capabilities;
	int ret;
	struct v4l2_format format;
	struct v4l2_crop crop;

	memset(&format, 0, sizeof(struct v4l2_format));
	memset(&crop, 0, sizeof(struct v4l2_crop));

	printf ("SetSrc : image(%dx%d) crop(%d,%d %dx%d) pixfmt(0x%08x) \n",
		width, height,
		crop_x, crop_y, crop_w, crop_h,
		pixelformat);

	/* check if capabilities is valid */
	capabilities = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT;
	ret = _v4l2_querycap(fd, capabilities);

	if (ret < 0)
		return ret;

	/* set the size and format of SRC */
	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	ret = _v4l2_g_fmt (fd, &format);
	if (ret < 0)
		return ret;

	format.fmt.pix.width       = width;
	format.fmt.pix.height      = height;
	format.fmt.pix.pixelformat = pixelformat;
	format.fmt.pix.field       = V4L2_FIELD_NONE;
	ret = _v4l2_s_fmt (fd, &format);
	if (ret < 0) {
		return ret;
        }

	/* set the crop area of SRC */
	crop.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	crop.c.left   = crop_x;
	crop.c.top    = crop_y;
	crop.c.width  = crop_w;
	crop.c.height = crop_h;
	ret =  _v4l2_cropcap (fd, crop.type, &crop.c);
	if (ret < 0)
		return ret;

	ret = _v4l2_s_crop (fd, &crop);
	if (ret < 0)
		return ret;

	return 0;
}

int v4l2_set_dst(int fd,
	int dst_x, int dst_y, int dst_w, int dst_h, void *addr)
{
	struct v4l2_format format;
	struct v4l2_framebuffer fbuf;
	int ret;
	memset(&format, 0, sizeof(struct v4l2_format));
	memset(&fbuf, 0, sizeof(struct v4l2_framebuffer));

	printf("SetDst : dst(%d,%d %dx%d) \n", dst_x, dst_y, dst_w, dst_h);

	/* set the size and format of DST */
	ret = _v4l2_g_fbuf(fd, &fbuf);
	if (ret < 0)
		return ret;

	if (addr != NULL)
		/* set the output buffer */
		fbuf.base = addr;

	fbuf.fmt.width  = dst_w;
	fbuf.fmt.height = dst_h;
	fbuf.fmt.pixelformat = V4L2_PIX_FMT_RGB32;
	ret = _v4l2_s_fbuf(fd, &fbuf);
	if (ret < 0)
		return ret;

	/* set the size of WIN */
	format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	ret = _v4l2_g_fmt(fd, &format);
	if (ret < 0)
		return ret;

	format.fmt.win.w.left   = dst_x;
	format.fmt.win.w.top    = dst_y;
	format.fmt.win.w.width  = dst_w;
	format.fmt.win.w.height = dst_h;
	ret = !_v4l2_s_fmt(fd, &format);
	if (ret < 0)
		return ret;

    return 0;
}

int v4l2_set_buffer (int fd,
                     enum v4l2_memory memory,
                     int num_buf,
                     struct mem_buffer **mem_buf)
{
	struct v4l2_requestbuffers req;
	int ret;
	memset(&req, 0, sizeof(struct v4l2_requestbuffers));

	printf ("SetBuffer : memory(%d) num_buf(%d) mem_buf(%p)\n",
	        memory, num_buf, mem_buf);

	req.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	req.count  = num_buf;
	req.memory = memory;
	ret = _v4l2_reqbuf (fd, &req);
	if (ret < 0)
		return ret;

	if (memory == V4L2_MEMORY_MMAP && mem_buf)
	{
		struct mem_buffer *out_buf;
		int i;

		out_buf = calloc(num_buf, sizeof(struct mem_buffer));
		if (!out_buf)
			return -1;

		for (i = 0; i < num_buf; i++)
		{
			struct v4l2_buffer buffer;
			memset(&buffer, 0, sizeof(struct v4l2_buffer));

			buffer.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			buffer.index  = i;
			buffer.memory = V4L2_MEMORY_MMAP;
			ret = _v4l2_querybuf (fd, &buffer);
			if (ret < 0) {
				free(out_buf);
				return ret;
			}

			out_buf[i].index  = buffer.index;
			out_buf[i].size   = buffer.length;
			out_buf[i].buf    = (void*)mmap (NULL, buffer.length,
					PROT_READ | PROT_WRITE , \
					MAP_SHARED , fd, buffer.m.offset);
			if (out_buf[i].buf == MAP_FAILED)
			{
				printf ("mmap failed. index(%d)\n", i);
				free(out_buf);
				return -1;
			}
		}

		*mem_buf = out_buf;
	}

	return 0;
}

int v4l2_clear_buffer(int fd,
                      enum v4l2_memory memory,
                      int num_buf,
                      struct mem_buffer *mem_buf)
{
	int ret;
	/*
	 * The = {0} syntax gives warnings on gcc version in OE,
	 * hence the memset
	 */
	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof(struct v4l2_requestbuffers));

	printf ("ClearBuffer : memory(%d) num_buf(%d) mem_buf(%p)\n", memory,
				num_buf, mem_buf);

	if (memory == V4L2_MEMORY_MMAP && mem_buf)
	{
		int i;

		for (i = 0; i < num_buf; i++)
			if (mem_buf[i].buf)
				if (munmap(mem_buf[i].buf, mem_buf[i].size)
							== -1)
					printf("Failed to unmap v4l2 buffer at"
							"index %d\n", i);

		free(mem_buf);
	}

	req.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	req.count  = 0;
	req.memory = memory;
	ret = _v4l2_reqbuf (fd, &req);
	if (ret < 0)
		return ret;

	return 0;
}

int v4l2_set_rotation(int fd, int rotation)
{
	int ret;
	struct v4l2_control ctrl;

	if(rotation == -1)
		return 0;

	memset(&ctrl, 0, sizeof(struct v4l2_control));

    /* set the rotation value */
	ctrl.id = V4L2_CID_ROTATE;
	ret = _v4l2_g_ctrl (fd, &ctrl);
	if (ret < 0)
		return ret;

	ctrl.value = rotation;
	ret = _v4l2_s_ctrl (fd, &ctrl);
	if (ret < 0)
		return ret;

	return 0;
}

int v4l2_set_flip(int fd, int hflip, int vflip)
{
	int ret;
	struct v4l2_control ctrl;

	if(hflip == 0 && vflip == 0)
		return 0;

	memset(&ctrl, 0, sizeof(struct v4l2_control));

	/* set the hflip value */
	ctrl.id = V4L2_CID_HFLIP;
	ret = _v4l2_g_ctrl (fd, &ctrl);
	if (ret < 0)
		return ret;

	ctrl.value = hflip;
	ret = _v4l2_s_ctrl (fd, &ctrl);
	if (ret < 0)
		return ret;

	/* set the vflip value */
	ctrl.id = V4L2_CID_VFLIP;
	ret = _v4l2_g_ctrl (fd, &ctrl);
	if (ret < 0)
		return ret;

	ctrl.value = vflip;
	ret = _v4l2_s_ctrl (fd, &ctrl);
	if (ret < 0)
		return ret;

	return 0;
}

int v4l2_stream_on(int fd)
{
	int ret;
	ret = _v4l2_streamon (fd, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	if (ret < 0)
		return ret;

	return 0;
}


int v4l2_stream_off(int fd)
{
	int ret;
	ret = _v4l2_streamoff (fd, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	if (ret < 0)
		return ret;

	return 0;
}

int v4l2_dequeue(int fd, enum v4l2_memory memory, int *index)
{
	int ret;
	struct v4l2_buffer buf;
	memset(&buf, 0, sizeof(struct v4l2_buffer));

	buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory = memory;

	ret = _v4l2_dequeue (fd, &buf);
	if (ret < 0)
		return ret;

	*index = buf.index;
	return 0;
}

int v4l2_queue(int fd, int index, enum v4l2_memory memory, __u32 bytes)
{
	int ret, i;
	struct v4l2_buffer buf;
	memset(&buf, 0, sizeof(struct v4l2_buffer));

	buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.index  = index;
	buf.memory = memory;
	buf.bytesused = bytes;
	if (memory == V4L2_MEMORY_USERPTR) {
		buf.m.userptr = (unsigned long) userptr;
		buf.length = bytes;
	}

	ret = _v4l2_queue (fd, &buf);
	if (ret < 0)
		return ret;

	return 0;
}
