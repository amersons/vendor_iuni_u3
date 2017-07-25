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

#define LOG_TAG "TestFramework"

#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <utils/Errors.h>
#include <utils/threads.h>
#include <utils/CallStack.h>
#include <utils/Log.h>
#include <cutils/properties.h>
#include <cutils/atomic.h>
#include <binder/IServiceManager.h>
#include <utils/String16.h>
#include <binder/Parcel.h>
#include <inc/testframework.h>
#include "TestFramework.h"
#include "TestFrameworkService.h"

const android::String16 ITestFramework::descriptor(TESTFRAMEWORK_NAME);
const android::String16& ITestFramework::getInterfaceDescriptor() const {
    return ITestFramework::descriptor;
}

android::sp<ITestFramework> ITestFramework::asInterface(
           const android::sp<android::IBinder>& obj)
{
    android::sp<ITestFramework> intr;
    if (obj != NULL) {
        intr = static_cast<ITestFramework*>(
                obj->queryLocalInterface(ITestFramework::descriptor).get());
        if (intr == NULL) {
            intr = new BpTestFramework(obj);
        }
    }
    return intr;
}
ITestFramework::ITestFramework() { }
ITestFramework::~ITestFramework() { }

bool BpTestFramework::tfConnected = false;
bool BpTestFramework::tfConnectedAgain = false;

int BpTestFramework::Connect(sp<ITestFramework> &tfDispacther) {
    if (tfConnected)
    {
        return 0;
    }

    android::sp<android::IServiceManager> sm = android::defaultServiceManager();
    android::sp<android::IBinder> binder = 0;

    if (sm != 0) {
        binder = sm->checkService(android::String16(TESTFRAMEWORK_NAME));
    }

    if (binder != 0)
    {
        tfDispacther = android::interface_cast<ITestFramework>(binder);
        tfConnected = true;
        tfConnectedAgain = true;
        return 1;
    }

    return 0;
}

void BpTestFramework::ConnectReset() {
    tfConnected = false;
    tfConnectedAgain = false;
}

bool BpTestFramework::IsConnectedAgain() {
    bool ret = tfConnectedAgain;
    tfConnectedAgain = false;
    return ret;
}

bool BpTestFramework::DispatchMsg(const char *str) {
    Parcel data, reply;
    data.writeInterfaceToken(ITestFrameworkService::descriptor);
    data.writeCString(str);
    remote()->transact(TF_WRITE_BUF, data, &reply, IBinder::FLAG_ONEWAY);
    return (reply.readInt32() != 0);
}

bool BpTestFramework::DispatchMsg(int val, const char *str) {
    Parcel data, reply;
    data.writeInterfaceToken(ITestFrameworkService::descriptor);
    data.writeInt32(val);
    data.writeCString(str);
    remote()->transact(TF_WRITE_FMT_MSG, data, &reply, IBinder::FLAG_ONEWAY);
    return (reply.readInt32() != 0);
}

void BpTestFramework::DispatchTurnOff() {
    Parcel data, reply;
    data.writeInterfaceToken(ITestFrameworkService::descriptor);
    data.writeInt32(0);
    remote()->transact(TF_TURNOFF, data, &reply, IBinder::FLAG_ONEWAY);
}

void BpTestFramework::DispatchTurnOn(int loggingtype) {
    Parcel data, reply;
    data.writeInterfaceToken(ITestFrameworkService::descriptor);
    data.writeInt32(loggingtype);
    remote()->transact(TF_TURNON, data, &reply, IBinder::FLAG_ONEWAY);
}

int BpTestFramework::DispatchGetInfo(int &w, int &x, int &y, int &z) {
    Parcel data, reply;
    data.writeInterfaceToken(ITestFrameworkService::descriptor);
    remote()->transact(TF_INFO, data, &reply);
    w = reply.readInt32();
    x = reply.readInt32();
    y = reply.readInt32();
    z = reply.readInt32();
    return 0;
}
