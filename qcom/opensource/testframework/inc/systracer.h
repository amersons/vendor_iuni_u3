/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef ANDROID_SYS_TRACER_H
#define ANDROID_SYS_TRACER_H

#ifdef __cplusplus
extern "C" {
#endif

/*Note: Google came up with c interface for tracer in mr2, can be used
 *      directly instead of below interface, keeping for compatability
 *      with older code
 */


// These tags must be kept in sync with frameworks/native/include/utils/Trace.h.
#ifndef ATRACE_TAG_NEVER
#define ATRACE_TAG_NEVER            0       // The "never" tag is never enabled.
#define ATRACE_TAG_ALWAYS           (1<<0)  // The "always" tag is always enabled.
#define ATRACE_TAG_GRAPHICS         (1<<1)
#define ATRACE_TAG_INPUT            (1<<2)
#define ATRACE_TAG_VIEW             (1<<3)
#define ATRACE_TAG_WEBVIEW          (1<<4)
#define ATRACE_TAG_WINDOW_MANAGER   (1<<5)
#define ATRACE_TAG_ACTIVITY_MANAGER (1<<6)
#define ATRACE_TAG_SYNC_MANAGER     (1<<7)
#define ATRACE_TAG_AUDIO            (1<<8)
#define ATRACE_TAG_VIDEO            (1<<9)
#define ATRACE_TAG_CAMERA           (1<<10)
#define ATRACE_TAG_LAST             ATRACE_TAG_CAMERA
#endif

//not implemented
#ifdef ATRACE_CALL
#undef ATRACE_CALL
#undef ATRACE_NAME
#endif
#define ATRACE_CALL()
#define ATRACE_NAME(name)

// ATRACE_INT traces a named integer value.  This can be used to track how the
// value changes over time in a trace.
#ifdef ATRACE_INT
#undef ATRACE_INT
#endif
#define ATRACE_INT(name, value) traceCounter(ATRACE_TAG, name, value)

// ATRACE_ENABLED returns true if the trace tag is enabled.  It can be used as a
// guard condition around more expensive trace calculations.
#ifdef ATRACE_ENABLED
#undef ATRACE_ENABLED
#endif
#define ATRACE_ENABLED() isTagEnabled(ATRACE_TAG)

#undef ATRACE_BEGIN
#define ATRACE_BEGIN traceBegin
#undef ATRACE_END
#define ATRACE_END traceEnd

void traceBegin(uint64_t tag, const char* name);
void traceEnd(uint64_t tag);
void traceCounter(uint64_t tag, const char* name, int32_t value);
int isTagEnabled(uint64_t tag);


#ifdef __cplusplus
}
#endif

#endif // ANDROID_TEST_FRAMEWORK_H
