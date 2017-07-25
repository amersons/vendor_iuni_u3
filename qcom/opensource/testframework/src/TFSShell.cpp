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
#include "TestFrameworkService.h"
#include "TestFramework.h"
#include "TFSShell.h"

#define TFS_VERSION "TF v2.0"
#define PROMPT "\n>>>"
#define MAX_FILTERLEN 64
#define TFS_MIN(x, y) ((x) < (y))?(x):(y)

TFSShell::TFSShell() {
    mBuf = (char *) malloc(sizeof(char) * MAX_BUFLEN);
    mPath = (char *) malloc(sizeof(char) * MAX_PATHLEN);
    mCmds =  (char *) malloc(sizeof(char) * MAX_PATHLEN);
    if(mPath) {
        mPath[0] = '/';
        mPath[1] = '\0';
    }
    mService = NULL;
    mQuit = false;
}

TFSShell::~TFSShell() {
    if(mBuf) {
        free(mBuf);
        mBuf = NULL;
    }
    if(mPath) {
        free(mPath);
        mPath = NULL;
    }
    if(mCmds) {
        free(mCmds);
        mCmds = NULL;
    }
}

int TFSShell::MainLoop(ITestFrameworkService *service, char *script) {
    int ret = -1;
    int arglen = 0;
    FILE *fp = stdin;

    if(!mBuf) {
        return -1;
    }

    mService = service;

    if(script) {
        fp = fopen(script, "rt");
        if(NULL == fp) {
            TFSTtyPrint("Script does not exist");
            return -1;
        }
    }

    while(!mQuit && !feof(fp)) {
        TFSTtyPrint(PROMPT);
        ret = -1;
        fgets(mBuf, MAX_BUFLEN, fp);
        arglen = strlen(mBuf);
        if(arglen > 0 && mBuf[arglen-1] == '\n') {
            mBuf[arglen-1] = '\0';
        }
        ret = RunCmd(mBuf, arglen);
    }

    return 0;
}

int TFSShell::RunCmd(char *buf, int arglen) {
    int ret = -1;
    char *arg = NULL;
    FILE *fp = NULL;
    int n = 0, fd = -1, fd2 = -1;
    int len = 0;
    int total = 0;

    arg = TFSGetCmdArg(buf, arglen);
    switch(buf[0]) {
    case  '#':
        //comment, ignore
        ret = 0;
        break;

    case 'h':
    case 'H':
        //help
        if(arglen != 4 || strncmp(buf, "help", 4)) {
            break;
        }

        TFSCmdHint("help");
        ret = 0;
        break;

    case 'v':
    case 'V':
        TFSTtyPrint(TFS_VERSION);
        ret = 0;
        break;

    case 'e':
    case 'E':
        //exit
        if(arglen == 4 && !strncmp(buf, "exit", 4)) {
            property_set("debug.tf.exit", "1");
            mQuit = true;
            ret = 0;
        }
        else if(arglen == 9 && !strncmp(buf, "eventtype", 9)) {
            arg = TFSGetCmdArg(arg+arglen, arglen);
            //eventtype <type>
            if(arg) {
                arg[arglen] = '\0';
                property_set("debug.tf.eventtype", arg);
            }
            else {
                property_get("debug.tf.eventtype", mBuf, "0");
                TFSTtyPrint(mBuf, strlen(mBuf));
            }
            ret = 0;
        }
        break;

    case 'k':
        if(arglen != 8 || strncmp(buf, "kfilters", 4)) {
            break;
        }
        //kfilters [available|list|set <filters>]
        //filters := !|+<subsystem>:<event>
        arg = TFSGetCmdArg(arg+arglen, arglen);
        if(NULL == arg) {
            //list
            fd = open(NODE_STR(set_event), O_RDONLY);
            while ((n = read(fd, mBuf, MAX_BUFLEN)) > 0) {
                mBuf[n-1] = '\0';
                TFSTtyPrint(mBuf, n);
            }
            close(fd);
        }
        else if(arg[0] == 'l') {
            //list
            fd = open(NODE_STR(set_event), O_RDONLY);
            while ((n = read(fd, mBuf, MAX_BUFLEN)) > 0) {
                mBuf[n-1] = '\0';
                TFSTtyPrint(mBuf, n);
            }
            close(fd);
        }
        else if(arg[0] == 's') {
            //set <filters>
            arg = TFSGetCmdArg(arg+arglen, arglen);
            if(arg) {
                char filter[MAX_FILTERLEN];
                if(arg[0] == '+') {
                    arg = TFSGetCmdArg(arg+1, arglen);
                    fd = open(NODE_STR(set_event), O_WRONLY);
                }
                else {
                    fd = open(NODE_STR(set_event), O_WRONLY|O_TRUNC);
                }
                if(arg) {
                    arg[arglen] = '\0';
                }
                arg = TFSGetFilterArg(arg, arglen);
                while(arg) {
                    strlcpy(filter, arg, TFS_MIN(arglen+1, MAX_FILTERLEN));
                    n = write(fd, filter, arglen);
                    arg = TFSGetFilterArg(arg+arglen, arglen);
                }
                close(fd);
            }
            else {
                //empty filters
                fd = open(NODE_STR(set_event), O_WRONLY|O_TRUNC);
                close(fd);
            }
        }
        else if(arg[0] == 'a') {
            //available events
            fd = open(NODE_STR(available_events), O_RDONLY);
            while ((n = read(fd, mBuf, MAX_BUFLEN)) > 0) {
                mBuf[n-1] = '\0';
                TFSTtyPrint(mBuf, n);
            }
            close(fd);
        }
        else {
            TFSCmdHint("kfilters");
        }
        ret = 0;
        break;

    case 'f':
        if(arglen != 7 || strncmp(buf, "filters", 7)) {
            break;
        }
        //filters [available|list|set <filters>]
        arg = TFSGetCmdArg(arg+arglen, arglen);
        if(NULL == arg) {
            property_get("debug.tf.filters", mBuf, "");
            TFSTtyPrint(mBuf, strlen(mBuf));
        }
        else if(arg[0] == 's' || arg[0] == 's') {
            //ufilters [available|list|set <filters>]
            //filters := !|+<subsystem>:<event>
            bool bRemove = false;
            arg = (char *) TFSGetCmdArg(arg+arglen, arglen);
            if(arg) {
                char filters[MAX_FILTERLEN];
                char * str = NULL;
                filters[0] = '\0';
                if(arg[0] == '+') {
                    arg = (char *)TFSGetCmdArg(arg+1, arglen);
                    if(arg) {
                        arg[arglen] = '\0';
                    }
                    property_get("debug.tf.filters", filters, NULL);
                    arg = TFSGetFilterArg(arg, arglen);
                    while(arg) {
                        bRemove = false;
                        arg[arglen] = '\0';
                        if (arg[0] == '!') {
                            arg++;
                            arglen--;
                            bRemove = true;
                        }
                        str = strstr(filters, arg);
                        if (str) {
                            if (bRemove) {
                                if ((filters < str) && (*(str-1) == '%'))
                                    strlcpy(str-1, str+arglen, MAX_FILTERLEN - (str - filters));
                                else if(filters+strlen(filters) > str+arglen && *(str+arglen) == '%') {
                                    strlcpy(str, str+arglen+1, MAX_FILTERLEN - (str - filters));
                                }
                                else
                                    strlcpy(str, str+arglen, MAX_FILTERLEN);
                            }
                        }
                        else if (!bRemove) {
                            if (filters[0] != '\0')
                                strlcat(filters, "%", MAX_FILTERLEN);
                            strlcat(filters, arg, MAX_FILTERLEN);
                        }
                        arg = TFSGetFilterArg(arg+arglen+1, arglen);
                    }
                }
                else {
                    arg[arglen] = '\0';
                    filters[0] = '\0';
                    arg = TFSGetFilterArg(arg, arglen);
                    while(arg) {
                        arg[arglen] = '\0';
                        if (arg[0] == '!') {
                            arg++;
                            arglen--;
                        }
                        else {
                            str = strstr(filters, arg);
                            if (!str) {
                                if (filters[0] != '\0')
                                    strlcat(filters, "%", MAX_FILTERLEN);
                                strlcat(filters, arg, MAX_FILTERLEN);
                            }
                        }
                        arg = TFSGetFilterArg(arg+arglen+1, arglen);
                    }
                }
                property_set("debug.tf.filters", filters);
            }
            else {
                property_set("debug.tf.filters", "$.$");
            }
        }
        else if(arg[0] == 'l') {
            property_get("debug.tf.filters", mBuf, "");
            TFSTtyPrint(mBuf, strlen(mBuf));
        }
        else if(arg[0] == 'a') {
            //not supported yet
            property_get("debug.tf.filters", mBuf, "");
            TFSTtyPrint(mBuf, strlen(mBuf));
        }
        else {
            TFSCmdHint("filters");
        }
        ret = 0;
        break;

    case 's':
        if(!strncmp(buf, "start", 5)) {
            //start
            arg = TFSGetCmdArg(arg+arglen, arglen);
            if(arg && (arg[0] == 'l' || arg[0] == 'L'))
                property_set("debug.tf.enable", "logcat");
            else if(arg && (arg[0] == '+'))
                property_set("debug.tf.enable", "1");
            else if(!arg) {
                fd = open(NODE_STR(trace), O_WRONLY|O_TRUNC);
                close(fd);
                property_set("debug.tf.enable", "1");
            }
            else
                TFSCmdHint("start");

            if(mService) {
                mService->TFSUpdate(START);
            }
            ret = 0;
        }
        else if(!strncmp(buf, "stop", 4)) {
            //stop
            property_set("debug.tf.enable", "0");
            if(mService) {
                mService->TFSUpdate(STOP);
            }
            ret = 0;
        }
        else if(!strncmp(buf, "size", 4)) {
            //size
            arg = TFSGetCmdArg(arg+arglen, arglen);
            if(arg) {
                arg[arglen] = '\0';
                fd = open(NODE_STR(buffer_size_kb), O_WRONLY|O_TRUNC);
                n = write(fd, arg, arglen);
                close(fd);
            }
            else {
                fd = open(NODE_STR(buffer_size_kb), O_RDONLY);
                n = read(fd, mBuf, MAX_BUFLEN);
                mBuf[n-1] = '\0';
                TFSTtyPrint(mBuf, n);
                close(fd);
            }
            ret = 0;
        }
        if(!strncmp(buf, "sleep", 5)) {
            //sleep
            unsigned long time = 0;
            int sec = 0;
            arg = TFSGetCmdArg(arg+arglen, arglen);
            if(arg) {
                time = strtoul(arg, NULL, 10);
                sec = time/1000;
                time = time - sec * 1000;
                TFSTtyPrint("sleeping...\n");
                if(sec) {
                    sleep(sec);
                }
                if(time) {
                    usleep(time*1000);
                }
                TFSTtyPrint("done sleeping\n");
            }
            else
                TFSCmdHint("sleep");

            ret = 0;
        }

        break;

    case 'd':
        if(arglen != 4 || strncmp(buf, "dump", 4)) {
            break;
        }

        total = 0;

        //dump [path]
        arg = TFSGetCmdArg(arg+arglen, arglen);

        if(arg && !strncmp("clear", arg, 5) && (arglen == 5)) {
            fd = open(NODE_STR(trace), O_WRONLY|O_TRUNC);
            close(fd);
            ret = 0;
            break;
        }

        if(NULL != arg) {
            arg[arglen] = '\0';
            if(mPath) {
                chdir(mPath);
            }
            fd2 = open(arg, O_WRONLY|O_CREAT|O_TRUNC,
                       S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        }

        fd = open(NODE_STR(trace), O_RDONLY);
        while ((n = read(fd, mBuf, MAX_BUFLEN)) > 0) {
            if(NULL == arg) {
                TFSTtyPrint(mBuf, n);
            }
            else {
                if(fd2 >= 0) {
                    write(fd2, mBuf, n);
                    total += n;
                }
                else {
                    TFSTtyPrint("could not dump into file");
                    break;
                }
            }
        }

        if(fd2 >= 0) {
            snprintf(mBuf, MAX_BUFLEN, "written %d bytes...\n", total);
            TFSTtyPrint(mBuf);
        }

        close(fd);
        close(fd2);
        fd2 = -1;
        ret = 0;
        break;

    case 'c':
        if(!strncmp(buf, "cd", 2) && arglen==2 && mPath && mCmds) {
            arg = TFSGetCmdArg(arg+arglen, arglen);
            if(arg) {
                arg[arglen] = '\0';
                snprintf(mCmds, MAX_PATHLEN, "cd %s;cd %s;pwd", mPath, arg);
                fp = popen(mCmds, "r");
                if(fp) {
                    while ( fgets( mPath, MAX_PATHLEN, fp));
                    pclose(fp);
                }
                int l = strlen(mPath);
                mPath[l-1] = '\0';
            }
            ret = 0;
        }
        break;

    case 'p':
        if(!strncmp(buf, "pwd", 3) && arglen==3 && mPath) {
            TFSTtyPrint(mPath);
            ret = 0;
        }
        if(arglen == 11 && !strncmp(buf, "probeperiod", 11)) {
            arg = TFSGetCmdArg(arg+arglen, arglen);
            if(arg) {
                property_set("debug.tf.probeperiod", arg);
            }
            else {
                property_get("debug.tf.probeperiod", mBuf, "");
                TFSTtyPrint(mBuf, strlen(mBuf));
            }
            ret = 0;
        }
        break;

    case '\n':
        ret = 0;
        break;

    default:
        break;
    }

    if(ret < 0 && mPath && mCmds) {
        snprintf(mCmds, MAX_PATHLEN, "cd %s;%s", mPath, buf);
        fp = popen(mCmds, "r");
        if(fp) {
            while ( fgets( buf, MAX_BUFLEN, fp)) {
                TFSTtyPrint(buf);
            }
            pclose(fp);
        }
    }

    return ret;
}

int TFSShell::TFSTtyPrint(const char *buf, int len) {
    int ret = 0;

    if(buf) {
        if(!len) {
            len = strlen(buf);
        }
        ret =  write(STDOUT_FILENO, buf, len);
    }

    return ret;
}

char *TFSShell::TFSGetCmdArg(char *buf, int &len)
{
  char *start = buf;
  int n = 0;
  char *end = NULL;

  len = 0;
  if (!buf) {
      return NULL;
  }

  n = strlen(buf);
  end = start + n;

  while (start && (start < end)) {
    if (*start == ' ' || *start == '\t' || ( *start == '\r' ||
        *start == '\n' || *start == '\"'))
        ++start;
    else
        break;
  }

  buf = start;

  while (start && (start < end)) {
    if (*start == ' ' || *start == '\t' || ( *start == '\r' ||
        *start == '\n' || *start == '\"')) {
      break;
    }
    ++len;
    ++start;
  }

  if(!len && start >= end) {
      len = 0;
      return NULL;
  }
  return buf;
}

char *TFSShell::TFSGetFilterArg(char *buf, int &len)
{
  char *start = buf;
  int n = 0;
  char *end = NULL;

  len = 0;
  if (!buf) {
      return NULL;
  }

  n = strlen(buf);
  end = start + n;

  if(*start == '%') {
      start++;
  }
  buf = start;

  while (start && (start < end))
  {
    if (*start == '%') {
      break;
    }
    ++len;
    ++start;
  }

  if(!len && start >= end) {
      len = 0;
      return NULL;
  }
  return buf;
}

void TFSShell::TFSCmdHelp() {
    TFSCmdHint("filters");
    TFSCmdHint("kfilters");
    TFSCmdHint("start");
    TFSCmdHint("stop");
    TFSCmdHint("sleep");
    TFSCmdHint("dump");
    TFSCmdHint("eventtype");
    TFSCmdHint("version");
    TFSCmdHint("exit");
}

void TFSShell::TFSCmdHint(const char *buf) {

    switch(buf[0]) {
    case 'h':
    case 'H':
        TFSCmdHelp();
        break;
    case 'd':
        TFSTtyPrint("dump [clear|<logfile>] : dumps log\n"
                    "                         'clear' - clears log\n"
                    "                         '<logfile>' - dumps trace log to this file\n\n");
        break;
    case 'v':
        TFSTtyPrint("version : prints version number\n\n");
        break;
    case 'e':
        if(buf[1] == 'v') {
            TFSTtyPrint("eventtype [<type>] : displays (in decimal) current eventtypes allowed\n"
                        "                    <type> event type in decimal\n"
                        "                           TF_EVENT_START      = 0x01\n"
                        "                           TF_EVENT_STOP       = 0x02\n"
                        "                           TF_EVENT            = 0x04\n"
                        "                           TF_EVENT_JAVA_START = 0x08\n"
                        "                           TF_EVENT_JAVA_STOP  = 0x10\n"
                        "                           TF_EVENT_JAVA       = 0x20\n\n");
        }
        else if(buf[1] == 'x') {
            TFSTtyPrint("exit] : exit\n\n");
        }

        break;

    case 's':
        if(buf[2] == 'a') {
            TFSTtyPrint("start [+|logcat] : starts tracing and clears previous log\n"
                        "                   '+' - previous log not cleared,\n"
                        "                         appendeds to previous log\n"
                        "                   'logcat' - output redirected to logcat\n\n");
        }
        else if(buf[2] == 'o') {
            TFSTtyPrint("stop : stops tracing\n\n");
        }
        else if(buf[1] == 'i') {
            TFSTtyPrint("size [<size-in-kb>] : displays current buffer size\n"
                        "                    <size-in-kb> sets to this size\n\n");
        }
        else if(buf[1] == 'l') {
            TFSTtyPrint("sleep [<time-in-ms>] : sleeps for <time-in-ms>\n");
        }
        break;
    case 'f':
        TFSTtyPrint("filters [set [<uspacefilters>]|available|list] : userspace filters\n"
                    "        'set' - clears filters \n"
                    "        'set <uspacefilters>' - sets filters \n"
                    "             <uspacefilters> := [+]<filters> \n"
                    "             <filters> := <filter>{%<filter>} \n"
                    "             <filter> := [!]<subsystem>:<event>\n"
                    "             ex: filters set SF:*%STC:* all SF, STC events set\n"
                    "                 filters set +Display:* - all Display events\n"
                    "                                          added to previous filters\n"
                    "                 filters set +ui:*%!STC:* - all STC events removed,\n"
                    "                                            all ui events added\n"
                    "        'available' - all available events\n"
                    "        'list' - lists all set events\n\n");
        break;
    case 'k':
        TFSTtyPrint("kfilters [set [<kspacefilters>]|available|list] : kernel filters\n"
                    "         'set' - clears filters \n"
                    "         'set <kspacefilters>' - sets filters \n"
                    "              <kspacefilters> := [+]<filters> \n"
                    "              <filters> := <filter>{%<filter>} \n"
                    "              <filter> := [!]<subsystem>:<event>\n"
                    "              ex: filters set power:*%irq:* sets all power, irq events\n"
                    "                  filters set +sched:* - all scheduler events\n"
                    "                                         added to previous filters\n"
                    "                  filters set +kgsl:*%!power:* - all power events removed,\n"
                    "                                                 all kgsl events added\n"
                    "         'available' - all available events\n"
                    "         'list' - lists all set events\n\n");
        break;

    }

}

