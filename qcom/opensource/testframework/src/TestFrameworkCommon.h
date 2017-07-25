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

#ifndef _TESTFRAMEWORKCOMMON_H_
#define _TESTFRAMEWORKCOMMON_H_

#include <utils/KeyedVector.h>
#include <utils/RefBase.h>
#include <utils/String16.h>
#include <utils/threads.h>
#include <string.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/Log.h>

using namespace android;

#define TF_EVENT_ID_SIZE_MAX 32
#define MAX_PATH 256
#define TF_EXCHANGE_INTERVAL 1000

class TfHashTable;

//update types
enum {
    START,
    STOP,
    SYNC
};

class TestFrameworkCommon {
public:
    TestFrameworkCommon();
    ~TestFrameworkCommon();

    int TfInit();
    int TfTracersInit();
    void TfUpdate(int logtype = -1, int updatetype = SYNC);
    int TfOpenMarker();
    int TfOpenTracer();
    int TfGetLoggingType();
    bool TfIsValid();
    bool TfIsServiceRunning();
    int TfWrite(const char *str);
    int TfWrite(int evType, const char *evArg);

    bool TfSearchFilterInTable(const char *eventgrp, const char *eventid);
    int TfGetPropertyService();
    int TfGetPropertyProbeFreq();
    int TfGetPropertyFilters();
    int TfGetPropertyLogType();
    int TfGetPropertyEventType();
    void TfGetPropertyTraceGatesInterval(int &openInterval,
                                         int &closeInterval);
    bool IsTraceGatesOpen();
protected:
    const char *TfFindDebugfs(void);
    int TfMountDebugfs(void);
    bool TfIsTracerEnabled();
    void TfEnable();
    void TfDisable();
    void TfAddFilterToTable(const char *key);

protected:
    bool mTfInitDone;
    int mTfTracerFd;
    int mTfMarkerFd;

    char mDebugfs[MAX_PATH+1];
    int mDebugfsFound;

    TfHashTable *mFilterTable;

    int mElapsedTime;
    int mLastRecordedTime;

    int mLogType;
    int mEventType;
    int mClosedInterval;
    int mOpenInterval;

    bool mIsService;

    int mProbeFreq;

    char mFilters[PROPERTY_VALUE_MAX];
    int mFiltersLen;

    bool mTurnOnByApi;
};

#endif
