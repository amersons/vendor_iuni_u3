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

#define LOG_TAG "TestFrameworkApi"

#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
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
#include "TestFrameworkCommon.h"
#include <utils/Singleton.h>

#define TF_LOG_BUF_SIZE 768

class TestFrameworkClient: public TestFrameworkCommon, public Singleton<TestFrameworkClient>  {
public:
    TestFrameworkClient();
    ~TestFrameworkClient();

    int tf_turnon();
    int tf_turnoff();
    int tf_write(const char *buf);
    int tf_print(int eventtype, const char *eventgrp,
                 const char *eventid, const char *fmt, va_list ap);
private:
    int tf_logging_status();
    const char *tf_get_str_eventtype(int event);

private:
    android::sp<ITestFramework> mTfDispacther;
    int tfTs;
};

ANDROID_SINGLETON_STATIC_INSTANCE(TestFrameworkClient) ;

static TestFrameworkClient &tfApi = TestFrameworkClient::getInstance();

//implementation of TF APIs
int tf_turnon() {
    return tfApi.tf_turnon();
}

int tf_turnoff() {
    return tfApi.tf_turnoff();
}

int tf_write(const char *buf) {
    return tfApi.tf_write(buf);
}

int tf_print(int eventtype, const char *eventgrp,
             const char *eventid, const char *fmt, ...) {
    int ret = 0;
    va_list ap;
    va_start (ap, fmt);
    ret = tfApi.tf_print(eventtype,eventgrp,eventid,fmt, ap);
    va_end (ap);
    return ret;
}

//implementation testframeworkclient
TestFrameworkClient::TestFrameworkClient() {
    mTfDispacther = NULL;
    tfTs = -(TF_EXCHANGE_INTERVAL);
}

TestFrameworkClient::~TestFrameworkClient() {
    mTfDispacther = NULL;
}

int TestFrameworkClient::tf_turnon()
{
    int ret = 0;

    if (TfIsServiceRunning()) {
        #ifdef TF_FEATURE_USE_BINDER
        BpTestFramework::Connect(mTfDispacther);
        #endif
        if (mTfDispacther != 0) {
            mTfDispacther->DispatchTurnOn(TF_TESTFRAMEWORK);
        }
    }
    else {
        if (!TfIsValid()) {
            TfTracersInit();
        }
        TfUpdate(TF_TESTFRAMEWORK);
        mTurnOnByApi = true;
    }

    return ret;
}

int TestFrameworkClient::tf_turnoff()
{
    int ret = 0;
    if (TfIsServiceRunning()) {
        if (mTfDispacther != 0) {
            mTfDispacther->DispatchTurnOff();
        }
    }
    else {
        mTurnOnByApi = false;
        TfUpdate(TF_DISABLE);
    }
    return ret;
}

int TestFrameworkClient::tf_write(const char *buf)
{
    int ret = 0, status = 0;
    bool send = false;

    status = tf_logging_status();

    switch(status) {
    case TF_LOGCAT:
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, buf);
        break;
    case TF_TESTFRAMEWORK:
    case TF_ALL:
        send = true;
        break;
    case TF_DISABLE:
        break;
    }

    if (send) {
#ifdef TF_FEATURE_MSGS_THROUGH_BINDER
        mTfDispacther->DispatchMsg(buf);
#else
        ret = TfWrite(buf);
#endif
    }

    return ret;
}

int TestFrameworkClient::tf_print(int eventtype, const char *eventgrp,
             const char *eventid, const char *fmt, va_list ap) {
    char evarg[TF_LOG_BUF_SIZE];
    int ret = 0, idx=0, status = TF_DISABLE;
    bool send = false;

    status = tf_logging_status();

    if (TF_DISABLE == status ||
        !(mEventType & eventtype) ||
        !TfSearchFilterInTable(eventgrp, eventid) ||
        !IsTraceGatesOpen()) {
        return 0;
    }

    switch(status) {
    case TF_LOGCAT: {
        __android_log_vprint(ANDROID_LOG_ERROR, eventgrp, fmt, ap);
    }
    break;
    case TF_LOGCAT_FTRACE:
    break;
    case TF_TESTFRAMEWORK: {
        char modifiedEventID[TF_EVENT_ID_SIZE_MAX];

        if (!TfIsValid()) {
            TfTracersInit();
        }

        //add pid to eve
        snprintf(modifiedEventID, TF_EVENT_ID_SIZE_MAX, "%s-%d", eventid, getpid());
        snprintf(evarg, TF_LOG_BUF_SIZE, "name=%s [class=%s, info=", modifiedEventID, eventgrp);
        idx = strlen(evarg);
        vsnprintf (evarg+idx, TF_LOG_BUF_SIZE-idx, fmt, ap);
        idx = strlen(evarg);
        snprintf (evarg+idx, TF_LOG_BUF_SIZE-idx, " {%s}", tf_get_str_eventtype(eventtype));
        idx = strlen(evarg);
        if (idx < TF_LOG_BUF_SIZE-2) {
            strlcpy(evarg+idx, "]\n", TF_LOG_BUF_SIZE-idx);
        }
        send = true;
    }
    break;
    case TF_ALL: {
        char modifiedEventID[TF_EVENT_ID_SIZE_MAX];

        if (!TfIsValid()) {
            TfTracersInit();
        }

        //add pid to events
        snprintf(modifiedEventID, TF_EVENT_ID_SIZE_MAX, "%s-%d", eventid, getpid());
        __android_log_vprint(ANDROID_LOG_ERROR, eventgrp, fmt, ap);
        snprintf(evarg, TF_LOG_BUF_SIZE, "name=%s [class=%s, info=", modifiedEventID, eventgrp);
        idx = strlen(evarg);
        vsnprintf (evarg+idx, TF_LOG_BUF_SIZE-idx, fmt, ap);
        idx = strlen(evarg);
        snprintf (evarg+idx, TF_LOG_BUF_SIZE-idx, " {%s}", tf_get_str_eventtype(eventtype));
        idx = strlen(evarg);
        if (idx < TF_LOG_BUF_SIZE-2) {
            strlcpy(evarg+idx, "]\n", TF_LOG_BUF_SIZE-idx);
        }
        send = true;
    }
    break;
    case TF_DISABLE:
    default:
        break;
    }


    if (send) {
#ifdef TF_FEATURE_MSGS_THROUGH_BINDER
        if (mTfDispacther != 0) {
            mTfDispacther->DispatchMsg(eventtype, evarg);
        }
#else
        ret = TfWrite(eventtype, evarg);
#endif
    }

    return ret;
}

int TestFrameworkClient::tf_logging_status() {
    int status = TF_DISABLE, time_now = 0;
    bool timeout = 0;

    //probe frequency set to 1, so lets not probe
    //any params whatever set initially they will be used
    //let it update only initially..
    if (mProbeFreq == 1) {
        mProbeFreq--;
    }
    else if (mProbeFreq <= 0) {
        return mLogType;
    }
    time_now = ns2ms(systemTime());
    timeout = (time_now - tfTs >= mProbeFreq);

    //eventhough binder doesn't incur much overhead, lets not use it
    //everytime, one in few milliseconds fetch data from tf service
    if (timeout) {
        if (TfIsServiceRunning()) {
            #ifdef TF_FEATURE_USE_BINDER
            BpTestFramework::Connect(mTfDispacther);
            #endif
            if (mTfDispacther != 0) {
                mTfDispacther->DispatchGetInfo(mLogType, mEventType,
                                              mOpenInterval, mClosedInterval);

                if (!mEventType) {
                    mTfDispacther->ConnectReset();
                }

                if ((mLogType != TF_DISABLE) && mTfDispacther->IsConnectedAgain()) {
                    TfGetPropertyFilters();
                }
            }
        }
        else {
            TfUpdate();
            if (mLogType != TF_DISABLE) {
                TfGetPropertyFilters();
            }
        }
        status = mLogType;
        tfTs = time_now;
    }
    else {
        status = mLogType;
    }

    return status;
}

const char *TestFrameworkClient::tf_get_str_eventtype(int event) {
    const char *str = "";
    switch(event) {
    case TF_EVENT_START:
        str = "TF_EVENT_START";
        break;
    case TF_EVENT_STOP:
        str = "TF_EVENT_STOP";
        break;
    case TF_EVENT:
        str = "TF_EVENT";
        break;
    case TF_EVENT_JAVA_START:
        str = "TF_EVENT_JAVA_START";
        break;
    case TF_EVENT_JAVA_STOP:
        str = "TF_EVENT_JAVA_STOP";
        break;
    case TF_EVENT_JAVA:
        str = "TF_EVENT_JAVA";
        break;
    default:
        break;
    }
    return str;
}
