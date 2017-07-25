/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/coresight-stm.h>
#include <stm-log.h>
#include <string.h>

#ifdef DEBUG
#define LOG_TAG "STM-LOG"
#include <cutils/log.h>
#endif

#define STM_LOG_DEV     "/dev/coresight-stm"
#define STM_LOG_MAGIC_0 0xf0
#define STM_LOG_MAGIC_1 0xf1

#define STM_LOG_STR_MAX 1024
#define STM_LOG_BIN_MAX 2048

static uint8_t dflt_stm_entity_id = OST_ENTITY_NONE;
static uint8_t dflt_stm_proto_id  = 0;
static uint32_t dflt_stm_options  = STM_OPTION_TIMESTAMPED;

typedef struct
{
    uint8_t  magic[2];
    uint8_t  entity;
    uint8_t  proto;
    uint32_t options;
} stmlog_t;

void stm_log_initdefaults(uint32_t init_mask,
                          uint8_t  entity_id,
                          uint8_t  proto_id,
                          uint32_t options)
{
    if ((init_mask & STM_DFLT_ENTITY) != 0) {
        dflt_stm_entity_id = entity_id;
    }
    if ((init_mask & STM_DFLT_PROTOCOL) != 0) {
        dflt_stm_proto_id = proto_id;
    }
    if ((init_mask & STM_DFLT_OPTIONS) != 0) {
        dflt_stm_options = options;
    }
#ifdef DEBUG
    if ((init_mask & ~(STM_DFLT_ENTITY
                       | STM_DFLT_PROTOCOL
                       | STM_DFLT_OPTIONS)) != 0) {
        ALOGE("Invalid init mask 0x%08x", init_mask);
    }
#endif
}

static int STMLOG_WRITE(int len, void *data, int *pfd) {
    int fd;
    int rc = -1;

    if (!pfd || (pfd && *pfd == -1))
        fd = open(STM_LOG_DEV, O_WRONLY);
    else
        fd = *pfd;

    if (fd != -1) {
        do {
            rc = write(fd, data, len);
            if (rc == -1)
                break;
            len -= rc;
        } while (len > 0);

        if (pfd)
            *pfd = fd;
        else
            close(fd);
    }
    return rc;
}

void stm_log(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    stm_log_ex(dflt_stm_entity_id,
               dflt_stm_proto_id,
               dflt_stm_options,
               format,
               ap);
    va_end(ap);
}

void stm_logbin(int length, void *data)
{
    stm_logbin_ex(dflt_stm_entity_id,
                  dflt_stm_proto_id,
                  dflt_stm_options,
                  length,
                  data);
}

void stm_log_ex(uint8_t     entity_id,
                uint8_t     proto_id,
                uint32_t    options,
                const char *format, ...)
{
    int       length;
    char     *buf;
    stmlog_t *log;
    va_list   ap;

    va_start(ap, format);
    length = vsnprintf(NULL, 0, format, ap);
    if (length >= 0) {
        length++;
	if (length > STM_LOG_STR_MAX)
            length = STM_LOG_STR_MAX;

        buf = (char *)malloc(sizeof(stmlog_t) + length);
        if (buf) {
            log = (stmlog_t *)buf;
            log->magic[0] = STM_LOG_MAGIC_0;
            log->magic[1] = STM_LOG_MAGIC_1;
            log->entity   = entity_id;
            log->proto    = proto_id;
            log->options  = options;
            vsnprintf(buf + sizeof(stmlog_t), length, format, ap);
            STMLOG_WRITE(sizeof(stmlog_t) + length, buf, NULL);
            free(buf);
        }
#ifdef DEBUG
        else {
            ALOGE("Out of memory allocating temp buffer");
        }
#endif
    }
    va_end(ap);
}

void stm_logbin_ex(uint8_t     entity_id,
                   uint8_t     proto_id,
                   uint32_t    options,
                   int         length,
                   void        *data)
{
    uint8_t  *usrdata = (uint8_t *)data;
    uint8_t  *buf;
    stmlog_t *stm;
    int       pktlen;
    int       fd = -1;

    if (length < 0)
        return;
    else if (length > STM_LOG_BIN_MAX)
        pktlen = STM_LOG_BIN_MAX;
    else
        pktlen = length;

    buf = (uint8_t *)malloc(sizeof(stmlog_t) + pktlen);
    if (buf) {
        stm = (stmlog_t *)buf;
        stm->magic[0] = STM_LOG_MAGIC_0;
        stm->magic[1] = STM_LOG_MAGIC_1;
        stm->entity   = entity_id;
        stm->proto    = proto_id;
        stm->options  = options;

        do {
            memcpy(buf + sizeof(stmlog_t), usrdata, pktlen);
            STMLOG_WRITE(sizeof(stmlog_t) + pktlen, buf, &fd);
            usrdata += pktlen;
            length -= pktlen;
            if (length < pktlen)
                pktlen = length;
        } while (length > 0);

        free(buf);
        if (fd != -1)
            close(fd);
    }
#ifdef DEBUG
    else {
        ALOGE("Out of memory allocating temp buffer");
    }
#endif
}
