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

#include <utils/Trace.h>
#include "../inc/systracer.h"

/*Note: Google came up with c interface for tracer in mr2, can be used
 *      directly instead of below interface
 */

void traceBegin(uint64_t tag, const char* name) {
  #if JB_MR2
  atrace_begin(tag, name);
  #else
  android::Tracer::traceBegin(tag, name);
  #endif
  return;
}

void traceEnd(uint64_t tag) {
  #if JB_MR2
  atrace_end(tag);
  #else
  android::Tracer::traceEnd(tag);
  #endif
  return;
}

void traceCounter(uint64_t tag, const char* name, int32_t value) {
  #if JB_MR2
  atrace_int(tag, name, value);
  #else
  android::Tracer::traceCounter(tag, name, value);
  #endif
  return;
}

int isTagEnabled(uint64_t tag) {
  #if JB_MR2
  return atrace_is_tag_enabled(tag);
  #else
  return android::Tracer::isTagEnabled(tag);
  #endif
}


