/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
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

#ifndef ANDROID_TEST_FRAMEWORK_H
#define ANDROID_TEST_FRAMEWORK_H

#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif

#define TF_EVENT_ID_SIZE_MAX 32

enum {
    TF_EVENT_START = 0x01,
    TF_EVENT_STOP  = 0x02,
    TF_EVENT       = 0x04,
    TF_EVENT_JAVA_START = 0x08,
    TF_EVENT_JAVA_STOP  = 0x10,
    TF_EVENT_JAVA       = 0x20,
};

//logging type
enum {
    TF_DISABLE,
    TF_TESTFRAMEWORK,
    TF_LOGCAT,
    TF_LOGCAT_FTRACE,
    TF_ALL,
};

#if defined CUSTOM_EVENTS_TESTFRAMEWORK || defined GFX_TESTFRAMEWORK
#define CONDITION(cond)     (__builtin_expect((cond)!=0, 0))

#define TF_WRITE(buf) tf_write(buf)
#define TF_PRINT(eventtype, eventgrp, eventid, ...) \
           tf_print(eventtype, eventgrp, eventid, __VA_ARGS__)

#define TF_WRITE_IF(cond, buf) \
    ( (CONDITION(cond)) \
    ? (void) TF_WRITE(buf) \
    : (void)0 )

#define TF_PRINT_IF(cond, eventtype, eventgrp, eventid, ...)  \
          ( (CONDITION(cond)) \
           ? (void) TF_PRINT(eventtype, eventgrp, eventid, __VA_ARGS__) \
           : (void)0 )
#define TF_TURNON() tf_turnon()
#define TF_TURNOFF() tf_turnoff()

int tf_write(const char *buf);
int tf_print(int eventtype, const char *eventgrp,
             const char *eventid, const char *fmt, ...);
int tf_turnon();
int tf_turnoff();

#else

#define TF_WRITE(...)   ((void)0)
#define TF_PRINT(...)   ((void)0)
#define TF_WRITE_IF(...)   ((void)0)
#define TF_PRINT_IF(...)   ((void)0)
#define TF_TURNON() ((void)0)
#define TF_TURNOFF() ((void)0)

#endif

#ifdef __cplusplus
}
#endif

#endif // ANDROID_TEST_FRAMEWORK_H
