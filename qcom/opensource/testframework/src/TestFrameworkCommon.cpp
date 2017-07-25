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

#define LOG_TAG "TestFrameworkCommon"

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
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <inc/testframework.h>
#include "TestFramework.h"
#include "TestFrameworkService.h"
#include "TestFrameworkHash.h"

#define _STR(x) #x
#define STR(x) _STR(x)

TestFrameworkCommon::TestFrameworkCommon() {
    TF_LOGD("TestFrameworkCommon created");
    mTfInitDone = false;
    mTfTracerFd = -1;
    mTfMarkerFd = -1;
    mLogType = TF_DISABLE;
    mDebugfsFound = 0;
    mFilterTable = NULL;
    mElapsedTime = 0;
    mLastRecordedTime = ns2ms(systemTime());
    mEventType = TF_EVENT | TF_EVENT_START | TF_EVENT_STOP |
                 TF_EVENT_JAVA | TF_EVENT_JAVA_START | TF_EVENT_JAVA_STOP;
    mClosedInterval = 0;
    mOpenInterval = 0;
    mIsService = 1;
    mProbeFreq = TF_EXCHANGE_INTERVAL;
    mFilters[0] = '\0';
    mFiltersLen = 0;
    mTurnOnByApi = false;

    TfInit();
}

TestFrameworkCommon::~TestFrameworkCommon() {
    if (mFilterTable) {
        delete mFilterTable;
        mFilterTable = NULL;
    }
}

int TestFrameworkCommon::TfWrite(const char *str) {
    int ret = 0;

    if (mTfMarkerFd < 0) {
        return -1;
    }

    if (str) {
        ret = ::write(mTfMarkerFd, str, strlen(str));
    }

    return ret;
}


int TestFrameworkCommon::TfWrite(int evType, const char *evArg) {
    struct iovec iov[2];
    char evid[TF_EVENT_ID_SIZE_MAX];
    int num, ret = 0;

    if (mTfMarkerFd < 0) {
        return -1;
    }

    switch(evType)
    {
        case TF_EVENT_START:
            strlcpy(evid, "uspace_start", sizeof(evid));
            break;
        case TF_EVENT_STOP:
            strlcpy(evid, "uspace_stop", sizeof(evid));
            break;
        case TF_EVENT:
            strlcpy(evid, "uspace_single", sizeof(evid));
            break;
        case TF_EVENT_JAVA_START:
            strlcpy(evid, "uspace_start", sizeof(evid));
            break;
        case TF_EVENT_JAVA_STOP:
            strlcpy(evid, "uspace_stop", sizeof(evid));
            break;
        case TF_EVENT_JAVA:
            strlcpy(evid, "uspace_single", sizeof(evid));
            break;
        default:
            evid[0] = '\0';
    }

    num = 1;
    //iov[0].iov_base = (void *) evArg;
    //iov[0].iov_len = strlen(evArg)+1;
    //iov[1].iov_base = (void *) evid;
    //iov[1].iov_len = strlen(evid)+1;

    if (evArg) {
        ret = ::write(mTfMarkerFd, evArg, strlen(evArg));
        //ret = writev(mTfMarkerFd, iov, num);
    }

    return ret;
}

int TestFrameworkCommon::TfInit() {
    if (NULL == mFilterTable) {
        mFilterTable = new TfHashTable(256, (HashFreeFunc) free,
                                       (HashCompareFunc) strcmp,
                                       (HashCompute) TfHashTable::ComputeUtf8Hash);
    }

    mIsService = !(0 == TfGetPropertyService());
    mProbeFreq = TfGetPropertyProbeFreq();

    return 0;
}

int TestFrameworkCommon::TfTracersInit() {
    char *debugfs;
    char path[MAX_PATH];

    if (mTfInitDone) {
        return 0;
    }

    //open tracing/tracing_on
    if (mTfTracerFd < 0) {
        TfOpenTracer();
    }

    //open tracing/trace_marker
    if (mTfMarkerFd < 0) {
        TfOpenMarker();
    }

    if (mTfTracerFd >= 0 && mTfMarkerFd >= 0) {
        mTfInitDone = true;
    }

    return 0;
}

void TestFrameworkCommon::TfEnable() {
    if (mTfTracerFd >= 0) {
        lseek(mTfTracerFd, 0, SEEK_SET);
        write(mTfTracerFd, "1", 1);
        TF_LOGD("TfEnable");
    }
    else {
      TF_LOGE("tf_turnon: could not open trace file");
    }
}

void TestFrameworkCommon::TfDisable() {
    if (mTfTracerFd >= 0) {
        lseek(mTfTracerFd, 0, SEEK_SET);
        write(mTfTracerFd, "0", 1);
        TF_LOGD("TfDisable");
    }
    else {
        TF_LOGE("tf_turnoff: could not open trace file");
    }
}

void TestFrameworkCommon::TfUpdate(int logtype, int updatetype) {

    if (updatetype != SYNC || !mTurnOnByApi) {
        mLogType = logtype;
        if (logtype < 0) {
            mLogType = TfGetPropertyLogType();
        }
    }

    int pf = TfGetPropertyProbeFreq();
    if ((1 != pf) || (0 != mProbeFreq)) {
        mProbeFreq = pf;
    }

    if (updatetype != SYNC && mTurnOnByApi) {
        //start, stop commands can close api turned on logging
        mTurnOnByApi = false;
    }

    if (mLogType != TF_DISABLE) {
        mEventType = TfGetPropertyEventType();
        TfGetPropertyTraceGatesInterval(mOpenInterval, mClosedInterval);
    }

    if (((mLogType == TF_ALL) || (mLogType == TF_TESTFRAMEWORK)) &&
        !TfIsTracerEnabled()) {
        TfEnable();
    }
    else if (((mLogType != TF_ALL) && (mLogType != TF_TESTFRAMEWORK)) &&
             TfIsTracerEnabled()) {
        TfDisable();
    }
    return;
}

int TestFrameworkCommon::TfGetPropertyService() {
    char property[PROPERTY_VALUE_MAX];
    int service = 0;

    if (property_get("debug.tf.service", property, "0") > 0) {
        service = atoi(property);
    }
    return service;
}

int TestFrameworkCommon::TfGetPropertyProbeFreq() {
    char property[PROPERTY_VALUE_MAX];
    int period = 0;

    if (property_get("debug.tf.probeperiod", property, "1000") > 0) {
        period = atoi(property);
    }
    return period;
}

//usage:
//setprop debug.testframework.enable logcat - logs to logcat only
//setprop debug.testframework.enable tf - logs to testframework
//setprop debug.testframework.enable testframework - logs to tf only
//setprop debug.testframework.enable all - logs to logcat and tf
//setprop debug.testframework.enable * - logs to logcat and tf
int TestFrameworkCommon::TfGetPropertyLogType() {
    int status = TF_DISABLE;
    char property[PROPERTY_VALUE_MAX];

    if (property_get("debug.tf.enable", property, "0") > 0) {
        switch(property[0]) {
            case 'L':
            case 'l':
                status = TF_LOGCAT;
                break;
            case 'T':
            case 't':
            case '1':
                status = TF_TESTFRAMEWORK;
                break;
            case '*':
            case 'a':
            case 'A':
                status = TF_ALL;
                break;
            case 'd':
            case 'D':
            case '0':
            default:
                status = TF_DISABLE;
                break;
        }
    }

    return status;
}

int TestFrameworkCommon::TfGetPropertyFilters() {
    char property[PROPERTY_VALUE_MAX];
    char key[PROPERTY_VALUE_MAX];
    char *tptr = NULL, *sptr = NULL, *eptr = NULL;
    int len = 0;

    if (property_get("debug.tf.filters", property, "*:*") > 0) {
        sptr = property;
        len = strlen(property);
        eptr = sptr + len;

        if (mFiltersLen != len || strcmp(mFilters, property)) {
            strlcpy(mFilters, property, sizeof(mFilters));
            mFiltersLen = len;
            //clear table
            if (mFilterTable) {
                mFilterTable->TfHashTableClear();
            }

            do {
                tptr = strchr(sptr, '%');
                if (!tptr && sptr < eptr) {
                    tptr = eptr;
                }
                if(tptr) {
                    strncpy(key, sptr, tptr-sptr);
                    key[tptr-sptr] = '\0';
                    //don't bother about duplicate entries, hashtable
                    //will take care of them
                    TfAddFilterToTable(key);
                    sptr = tptr+1;
                }
            } while(tptr && sptr < eptr);
        }
    }
    return 0;
}

int TestFrameworkCommon::TfGetPropertyEventType() {
    int status = TF_EVENT | TF_EVENT_STOP;
    char property[PROPERTY_VALUE_MAX];

    if (property_get("debug.tf.eventtype", property, "255") > 0) {
        status = atoi(property);
    }

    return status;
}

void TestFrameworkCommon::TfGetPropertyTraceGatesInterval(int &openInterval,
                                                          int &closeInterval) {
    int interval = 1;
    char property[PROPERTY_VALUE_MAX];
    char *tptr = NULL;

    if (property_get("debug.tf.interval", property, "0 0") > 0) {
        tptr = strchr(property, ' ');
        if (tptr) {
            mClosedInterval = atoi(tptr+1);
            *tptr = '\0';
            mOpenInterval = atoi(property);
    }
    }
    return;
}

bool TestFrameworkCommon::TfIsTracerEnabled() {
    bool ret = false;
    int n = 0;
    char buf[1];
    buf[0] = 0;
    if (mTfTracerFd) {
        lseek(mTfTracerFd, 0, SEEK_SET);
        n = read(mTfTracerFd, buf, 1);
        if ((n > 0) && (buf[0] == '1')) {
            ret = true;
        }
    }

    return ret;
}

bool TestFrameworkCommon::TfIsValid() {
    return mTfInitDone;
}

bool TestFrameworkCommon::TfIsServiceRunning() {
    return mIsService;
}

int TestFrameworkCommon::TfGetLoggingType() {
    return mLogType;
}

const char *TestFrameworkCommon::TfFindDebugfs(void) {
    char type[100];
    FILE *fp;

    if (mDebugfsFound)
        return mDebugfs;

    if ((fp = fopen("/proc/mounts","r")) == NULL)
        return NULL;

    while (fscanf(fp, "%*s %"
                  STR(MAX_PATH)
                  "s %99s %*s %*d %*d\n",
                  mDebugfs, type) == 2) {
        if (strcmp(type, "debugfs") == 0)
            break;
    }
    fclose(fp);

    if (strcmp(type, "debugfs") != 0)
        return NULL;

    mDebugfsFound = 1;

    return mDebugfs;
 }

int TestFrameworkCommon::TfMountDebugfs(void) {
    int ret = 0;

    ret = ::mount("nodev", "/sys/kernel/debug", "debugfs", MS_MGC_VAL, "");
    return ret;
}

int TestFrameworkCommon::TfOpenMarker() {
    char *debugfs;
    char path[MAX_PATH];

    //TfMountDebugfs();
    debugfs = (char *) TfFindDebugfs();

    if (debugfs) {
        strlcpy(path, debugfs, sizeof(path));
        strlcat(path, "/tracing/trace_marker", sizeof(path));
        errno = 0;
        mTfMarkerFd = open(path, O_WRONLY);
        TF_LOGD("tf_marker opened=%d", mTfMarkerFd);
    }

    return mTfMarkerFd;
}

int TestFrameworkCommon::TfOpenTracer() {
    char *debugfs;
    char path[MAX_PATH];

    //TfMountDebugfs();
    debugfs = (char *) TfFindDebugfs();

    if (debugfs) {
        strlcpy(path, debugfs, sizeof(path));
        strlcat(path, "/tracing/tracing_on", sizeof(path));
        errno = 0;
        mTfTracerFd = open(path, O_RDWR);
        TF_LOGD("tf_tracer opened=%d", mTfTracerFd);
    }

    return mTfTracerFd;
}

void TestFrameworkCommon::TfAddFilterToTable(const char *key) {
    if (mFilterTable) {
        mFilterTable->TfHashTableLookup((void *)key, strlen(key)+1, true);
    }
}

bool TestFrameworkCommon::TfSearchFilterInTable(const char *eventgrp,
                                                const char *eventid) {
    char key[128];
    const char* ent = NULL;

    if (mFilterTable) {
        snprintf(key, 128, "%s:%s", eventgrp, eventid);
        ent = (const char *) mFilterTable->TfHashTableLookup(key);
        if (ent == NULL) {
            snprintf(key, 128, "%s:*", eventgrp);
            ent = (const char *) mFilterTable->TfHashTableLookup(key);
            if (ent == NULL) {
                strlcpy(key, "*:*", 128);
                ent = (const char *) mFilterTable->TfHashTableLookup(key);
                if (ent == NULL) {
                    strlcpy(key, "*.*", 128);
                    ent = (const char *) mFilterTable->TfHashTableLookup(key);
                }
            }
        }
    }

    return (ent != NULL);
}

bool TestFrameworkCommon::IsTraceGatesOpen() {
    static bool bClosed = false;
    int time_now = ns2ms(systemTime());
    mElapsedTime += (time_now - mLastRecordedTime);
    mLastRecordedTime = time_now;
    bool timeout = true;
    if(bClosed) {
        timeout = (!mClosedInterval || (mElapsedTime >= mClosedInterval));
        if (timeout) {
           bClosed = false;
           mElapsedTime = 0;
        }
    }
    else {
        bClosed = (mOpenInterval && (mElapsedTime >= mOpenInterval));
        if (bClosed) {
            mElapsedTime = 0;
        }
    }
    return timeout;
}

