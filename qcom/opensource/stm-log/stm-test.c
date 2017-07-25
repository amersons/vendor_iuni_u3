/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/coresight-stm.h>
#include "stm-log.h"

#define FILE_READ_MAX    8192  /* simplify handling by restricting to
                                  maximum of 8K file read as this is only
                                  a reference/test app for STM logging
                                */
static char buf[FILE_READ_MAX];
static int buf_count = 0;

int main(int argc, char **argv)
{
    int opt, fd = -1;
    int ent = 0, prot = 0, Ots= 1, Og = 0, Oopts = 0;
    char *string = NULL;
    int isString = 0, isFile = 0;

    while ((opt = getopt(argc, argv, "e:p:t:g:s:f:")) != -1) {
        switch (opt) {
        case 'e':
            ent = atoi(optarg);
            if (ent < 0)
                ent = 0;
            if (ent > 255)
                ent = 255;
            break;
        case 'p':
            prot = atoi(optarg);
            if (prot < 0)
                prot = 0;
            if (prot > 255)
                prot = 255;
            break;
        case 't':
            Ots = (atoi(optarg) != 0);
            break;
        case 'g':
            Og = (atoi(optarg) != 0);
            break;
        case 's':
            if (string)
                free(string);
            string = strdup(optarg);
            isString = 1;
            isFile = 0;
            break;
        case 'f':
            if (string)
                free(string);
            string = strdup(optarg);
            isString = 0;
            isFile = 1;
            break;
        default:
            printf("\n");
            printf("%s [-e <entity>] [-p <protocol>] [-t <0|1>] [-g <0|1>] -s <string>\n", argv[0]);
            printf("%s [-e <entity>] [-p <protocol>] [-t <0|1>] [-g <0|1>] -f <file>\n", argv[0]);
            return 1;
        }
    }

    if (!isString && !isFile) {
        printf("Must either specify string or file\n");
        return 1;
    }
    if (!string) {
        printf("Internal error\n");
        return 1;
    }

    printf("Entity=%d Protocol=%d Opt-TS=%d Opt-G=%d: %s='%s'\n",
           ent, prot, Ots, Og,
           (isString) ? "string" : "file",
           string);

    Oopts = (Ots ? STM_OPTION_TIMESTAMPED : 0)
            | (Og ? STM_OPTION_GUARANTEED : 0);

    do {
        if (isString) {
            stm_log_ex(ent, prot, Oopts, string);
        }
        else {
            fd = open(string, O_RDONLY);
            if (fd == -1) {
                printf("Failed to open file (%s)\n", strerror(errno));
                break;
            }
            buf_count = read(fd, buf, FILE_READ_MAX);
            if (buf_count >= 0) {
                stm_logbin_ex(ent, prot, Oopts, buf_count, buf);
            }
       }
    } while (0);

    free(string);
    if (fd != -1)
        close(fd);
    return 0;
}
