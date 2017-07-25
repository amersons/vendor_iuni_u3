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

#ifndef _TESTFRAMEWORK_H_
#define _TESTFRAMEWORK_H_

#include <utils/KeyedVector.h>
#include <utils/RefBase.h>
#include <utils/String16.h>
#include <utils/threads.h>
#include <string.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/Log.h>

//Interface for client
#define TESTFRAMEWORK_NAME "TestFrameworkService.TestFrameworkInterface"

using namespace android;

#ifdef JB
#define TF_LOGE(...) ALOGE(__VA_ARGS__)
#define TF_LOGD(...) ALOGD(__VA_ARGS__)
#else
#define TF_LOGE(...) LOGE(__VA_ARGS__)
#define TF_LOGD(...) LOGD(__VA_ARGS__)
#endif

enum {
    TF_WRITE_BUF = 1,
    TF_WRITE_FMT_MSG = 2,
    TF_INFO = 3,
    TF_TURNON = 4,
    TF_TURNOFF = 5,
};

class ITestFramework: public IInterface
{
public:
    DECLARE_META_INTERFACE(TestFramework)   // no trailing ;
    virtual bool DispatchMsg(const char *str) = 0;
    virtual bool DispatchMsg(int val, const char *str) = 0;
    virtual void DispatchTurnOff() = 0;
    virtual void DispatchTurnOn(int loggingtype) = 0;
    virtual int DispatchGetInfo(int &w, int &x, int &y, int &z) = 0;

    virtual void ConnectReset() = 0;
    virtual bool IsConnectedAgain() = 0;
};

class BpTestFramework: public BpInterface<ITestFramework>
{
public:
    BpTestFramework(const sp<IBinder>& impl)
            : BpInterface<ITestFramework>(impl) {
        tfConnected = false;
    }

    virtual bool DispatchMsg(const char *str);
    virtual bool DispatchMsg(int val, const char *str);
    virtual void DispatchTurnOff();
    virtual void DispatchTurnOn(int loggingtype);
    virtual int DispatchGetInfo(int &w, int &x, int &y, int &z);

    static int Connect(sp<ITestFramework> &tfDispacther);
    void ConnectReset();
    bool IsConnectedAgain();

private:
    static bool tfConnected;
    static bool tfConnectedAgain;
};

#endif
