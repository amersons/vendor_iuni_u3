/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef _TESTFRAMEWORKSERVICE_H_
#define _TESTFRAMEWORKSERVICE_H_

#include <utils/KeyedVector.h>
#include <utils/RefBase.h>
#include <utils/String16.h>
#include <utils/threads.h>
#include <string.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/Log.h>
#include "TestFrameworkCommon.h"

using namespace android;

#define TRACING_BASE "/sys/kernel/debug/tracing/"
#define NODE_STR(x) TRACING_BASE#x

class ITestFrameworkService
                           #ifdef TF_FEATURE_USE_BINDER
                           : public IInterface
                           #endif
{
public:
    DECLARE_META_INTERFACE(TestFrameworkService)

    virtual int TFSInit() = 0;
    virtual bool TFSUpdate(int updatetype = SYNC) = 0;
    virtual void StopTestFrameworkService() = 0;
};

class BnTestFrameworkService : public BnInterface<ITestFrameworkService>
{
};

class TestFrameworkService :
                             #ifdef TF_FEATURE_USE_BINDER
                             public BnTestFrameworkService,
                             #else
                             public ITestFrameworkService,
                             #endif
                             public TestFrameworkCommon
{
public:
    static ITestFrameworkService *RunTestFrameworkService();
    void StopTestFrameworkService();
    TestFrameworkService();
    virtual ~TestFrameworkService();

    status_t onTransact(uint32_t code,
                        const Parcel &data,
                        Parcel *reply,
                        uint32_t flags);

    virtual int TFSInit();
    virtual bool TFSUpdate(int updatetyep = SYNC);

private:
    int32_t mNextConnId;
    pthread_mutex_t mMutex;
};

#endif
