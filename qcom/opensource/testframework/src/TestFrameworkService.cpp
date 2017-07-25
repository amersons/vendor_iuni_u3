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

#define LOG_TAG "TestFrameworkService"

#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <stdlib.h>
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
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <inc/testframework.h>
#include "TestFramework.h"
#include "TestFrameworkService.h"

ITestFrameworkService::ITestFrameworkService() { }
ITestFrameworkService::~ITestFrameworkService() { }

const android::String16 ITestFrameworkService::descriptor(TESTFRAMEWORK_NAME);

const android::String16& ITestFrameworkService::getInterfaceDescriptor() const {
    return ITestFrameworkService::descriptor;
}

ITestFrameworkService *TestFrameworkService::RunTestFrameworkService() {
    TestFrameworkService *service =  new TestFrameworkService();
#ifdef TF_FEATURE_USE_BINDER
    defaultServiceManager()->addService(ITestFrameworkService::descriptor, service);
    ProcessState::self()->startThreadPool();
#endif
    return (ITestFrameworkService *)service;
}

void TestFrameworkService::StopTestFrameworkService() {
    //stub for now
    return;
}


TestFrameworkService::TestFrameworkService() {
    TF_LOGD("TestFrameworkService created");
    mNextConnId = 1;
    pthread_mutex_init(&mMutex, 0);
    TfInit();
}

TestFrameworkService::~TestFrameworkService() {
    pthread_mutex_destroy(&mMutex);
    TF_LOGD("TestFrameworkService destroyed");
}

status_t TestFrameworkService::onTransact(uint32_t code,
                                          const android::Parcel &data,
                                          android::Parcel *reply,
                                          uint32_t flags) {
    status_t ret = android::NO_ERROR;
#ifdef TF_FEATURE_USE_BINDER
    int evType = 0;
    int status = 0;

    switch(code) {
        case TF_WRITE_FMT_MSG: {
            CHECK_INTERFACE(ITestFrameworkService, data, reply);
            evType = data.readInt32();
            const char *str = data.readCString();
            TfWrite(evType, str);
        }
        break;

        case TF_WRITE_BUF: {
            CHECK_INTERFACE(ITestFrameworkService, data, reply);
            const char *str;
            str = data.readCString();
            TfWrite(str);
        }
        break;

        case TF_INFO: {
            CHECK_INTERFACE(ITestFrameworkService, data, reply);
            data.readInt32();
            reply->writeInt32(mLogType);
            reply->writeInt32(mEventType);
            reply->writeInt32(mOpenInterval);
            reply->writeInt32(mClosedInterval);
        }
        break;

        case TF_TURNON: {
            CHECK_INTERFACE(ITestFrameworkService, data, reply);
            evType = data.readInt32();
            if (evType == TF_LOGCAT) {
                property_set("debug.tf.enable", "logcat");
            }
            else if (evType == TF_TESTFRAMEWORK) {
                property_set("debug.tf.enable", "1");
            }

            if (!TfIsValid()) {
                TfTracersInit();
            }

            TfUpdate(evType);
        }
        break;

        case TF_TURNOFF: {
            CHECK_INTERFACE(ITestFrameworkService, data, reply);
            data.readInt32();
            property_set("debug.tf.enable", "0");
            TfUpdate(TF_DISABLE);
        }
        break;

        default: {
            ret = BBinder::onTransact(code, data, reply, flags);
        }
        break;
    }
#endif

    return ret;
}

int TestFrameworkService::TFSInit() {
    int ret = 0;

    ret = system("su -c mount -t debugfs nodev /sys/kernel/debug");
    if(ret >= 0) {
        ret = system("su -c chmod 0666 "NODE_STR(tracing_on));
        ret = system("su -c chmod 0222 "NODE_STR(trace_marker));
        ret = system("su -c chmod 0666 "NODE_STR(buffer_size_kb));
        ret = system("su -c chmod 0666 "NODE_STR(set_event));
        ret = system("echo 0 > "NODE_STR(tracing_on));
        ret = system("echo 10240 > "NODE_STR(buffer_size_kb));
        if(ret >= 0) {
            printf("TestFrameworkService Running...\n");
        }
    }
    else {
      TF_LOGE("TFS: mount debugfs failed, errno=%d", errno);
    }

    if(ret < 0) {
        printf("Failed to setup the environment, either CONFIG_FTRACE, "
               "CONFIG_ENABLE_DEFAULT_TRACERS are not enabled or debugfs"
               "could not be mounted, if issue is later, you may try inserting"
               "these rules in init.rc"
               "#debugfs"
               "mount debugfs nodev /sys/kernel/debug"
               "chmod 0666 /sys/kernel/debug/tracing/tracing_on"
               "chmod 0222 /sys/kernel/debug/tracing/trace_marker"
               "write /sys/kernel/debug/tracing/tracing_on 0");
    }
    //testframework init, open tracer, marker, etc.
    TF_LOGD("tfhash: TFSInit");
    ret = TfInit();

    property_set("debug.tf.servicerunning", "1");

    return ret;
}

bool TestFrameworkService::TFSUpdate(int updatetype) {
    char property[PROPERTY_VALUE_MAX];
    int ex = 0;

    pthread_mutex_lock(&mMutex);

    if (!TfIsValid()) {
        TfTracersInit();
    }

    TfUpdate(-1, updatetype);

    pthread_mutex_unlock(&mMutex);

    if (property_get("debug.tf.exit", property, "0") > 0) {
        ex = atoi(property);
    }

    if(ex) {
        return false;
    }
    return true;
}

