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

#define LOG_TAG "TestFrameworkServiceMain"

#include <cutils/properties.h>
#include <signal.h>
#include "TestFrameworkService.h"
#include "TestFramework.h"
#include "TFSShell.h"

static pthread_mutex_t mMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t mCond = PTHREAD_COND_INITIALIZER;
static pthread_t mThreadID;
static volatile int recvedInterrupt = 0;

class TFSCmdArgParser
{
public:
    TFSCmdArgParser(int argc, char *argv[]);
    ~TFSCmdArgParser();
    const char *GetNext(int &len);

private:
    char **mArgv;
    int mArgc;
    int mArgLen;
    int mArgIdx;
    int mCmdBoundaryIdx;
};

TFSCmdArgParser::TFSCmdArgParser(int argc, char *argv[]) {
    mArgv = argv;
    mArgc = argc;
    mArgIdx = 1;
    mCmdBoundaryIdx = 0;
    mArgLen = 0;
}

TFSCmdArgParser::~TFSCmdArgParser() {
}

const char *TFSCmdArgParser::GetNext(int &len) {
    int j = 0;
    int i = mArgIdx;
    char *start = NULL;
    len = 0;

    do {
        if (mArgIdx >= mArgc) {
            start = NULL;
            return start;
        }

        start = mArgv[i] + mCmdBoundaryIdx;

        if (mArgv[i]) {
            if (0 == mCmdBoundaryIdx)
                mArgLen = strlen(mArgv[i]);
            for(j=mCmdBoundaryIdx; j < mArgLen; j++) {
                if ((mArgv[i][j] == ';') || (mArgv[i][j] == '@')) {
                    len = j - mCmdBoundaryIdx;
                    mCmdBoundaryIdx = j+1;
                    mArgv[i][j] = '\0';
                    break;
                }
            }
        }

        if (j >= mCmdBoundaryIdx) {
            len = j - mCmdBoundaryIdx;
            mCmdBoundaryIdx = 0;
            mArgIdx++;
        }
    }while(0 == len);
    return start;
}

static void ExitService(int sig) {
    TFSShell::TFSTtyPrint("Caught signal...\n");
    recvedInterrupt = 1;
}

void *TFSSync(void *s) {
    ITestFrameworkService *service;

    service = (ITestFrameworkService *)s;

    if(!service) {
        TFSShell::TFSTtyPrint("Can not run service...");
        return NULL;
    }

    service->TFSInit();

    pthread_mutex_lock(&mMutex);
    pthread_cond_signal(&mCond);
    pthread_mutex_unlock(&mMutex);

    while(true) {
        if (!service->TFSUpdate() || recvedInterrupt) {
            TFSShell::TFSTtyPrint("Exit..\n");
            break;
        }
        /*sleep 500ms*/
        usleep(500000);
    }

    return NULL;
}

ITestFrameworkService *SpinService() {
    pthread_attr_t attr;
    ITestFrameworkService *service = NULL;
    service = TestFrameworkService::RunTestFrameworkService();
    pthread_mutex_init(&mMutex, 0);
    pthread_cond_init(&mCond, NULL);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&mThreadID, &attr, TFSSync, service);
    pthread_attr_destroy(&attr);

    pthread_mutex_lock(&mMutex);
    pthread_cond_wait(&mCond, &mMutex);
    pthread_mutex_unlock(&mMutex);

    return service;
}

int main(int argc, char *argv[])
{
    TFSShell sh;
    ITestFrameworkService *service = NULL;
    const char *buf = NULL;
    int len = 0;

    //catch the signals
    signal(SIGINT, ExitService);
    signal(SIGQUIT, ExitService);

    TFSCmdArgParser argparser = TFSCmdArgParser(argc, argv);
    buf = argparser.GetNext(len);
    if(buf && len>0 && strstr(buf, ".tf")) {
        ITestFrameworkService *service = NULL;
        service = TestFrameworkService::RunTestFrameworkService();
        service->TFSInit();
        service->TFSUpdate();
        TFSShell::TFSTtyPrint("Executing Script...\n");
        sh.MainLoop(service, (char *)buf);
        service->StopTestFrameworkService();
    }
    else if(buf) {
        ITestFrameworkService *service = NULL;
        service = TestFrameworkService::RunTestFrameworkService();
        service->TFSInit();
        service->TFSUpdate();
        if (len > 0)
            sh.RunCmd((char *)buf, len);
        buf = argparser.GetNext(len);
        while(buf) {
            if (len > 0)
                sh.RunCmd((char *)buf, len);
            buf = argparser.GetNext(len);
        }
        service->StopTestFrameworkService();
    }
    else {
        service = SpinService();
        TFSShell::TFSTtyPrint("Interactive mode...");
        sh.MainLoop(service);

        service->StopTestFrameworkService();

        pthread_mutex_destroy(&mMutex);
        pthread_cond_destroy(&mCond);

        void *dummy;
        pthread_join(mThreadID, &dummy);
        status_t err = (status_t) dummy;
    }

    return 0;
}


