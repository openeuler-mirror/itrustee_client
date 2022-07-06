/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2022. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "misc_work_agent.h"
#include <errno.h>     /* for errno */
#include <sys/types.h> /* for open close */
#include <sys/ioctl.h> /* for ioctl */
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <securec.h>
#include "tee_log.h"
#include "tc_ns_client.h"
#include "tee_agent.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "teecd_agent"

static void GetTimeWork(struct MiscControlType *transControl)
{
    struct timeval timeVal;
    errno_t rc;

    if (gettimeofday(&timeVal, NULL) == 0) {
        transControl->ret                  = 0;
        transControl->Args.GetTime.seconds = timeVal.tv_sec;
        transControl->Args.GetTime.millis  = (timeVal.tv_usec / 1000);
        struct tm *tstruct                 = NULL;

        tstruct = localtime(&(timeVal.tv_sec));
        if (tstruct != NULL) {
            /* year(from 1900) months(0~11) days hour min second */
            rc = snprintf_s(transControl->Args.GetTime.timeStr, sizeof(transControl->Args.GetTime.timeStr),
                            sizeof(transControl->Args.GetTime.timeStr) - 1, "%04d-%02d-%02d %02d:%02d:%02d.%03d ",
                            tstruct->tm_year + 1900, tstruct->tm_mon + 1, tstruct->tm_mday, tstruct->tm_hour,
                            tstruct->tm_min, tstruct->tm_sec, (int)(timeVal.tv_usec / 1000));
            if (rc == -1) {
                transControl->ret = -1;
                tloge("snprintf_s error %d\n", rc);
            }
        } else {
            tloge("get localtiem error\n");
        }
    } else {
        transControl->ret                  = -1;
        transControl->Args.GetTime.seconds = 0;
        transControl->Args.GetTime.millis  = 0;
    }
}

void *MiscWorkThread(void *control)
{
    struct MiscControlType *transControl = NULL;
    int ret;
    int miscFd;

    if (control == NULL) {
        return NULL;
    }
    transControl = (struct MiscControlType *)control;

    miscFd = GetMiscFd();
    if (miscFd == -1) {
        tloge("misc file is not open\n");
        return NULL;
    }

    transControl->magic = AGENT_MISC_ID;
    while (1) {
        tlogv("++ misc agent loop ++\n");
        ret = ioctl(miscFd, (int)TC_NS_CLIENT_IOCTL_WAIT_EVENT, AGENT_MISC_ID);
        if (ret != 0) {
            tloge("misc agent wait event failed\n");
            break;
        }

        tlogv("misc agent wake up and working!!\n");
        switch (transControl->cmd) {
            case SEC_NV_INFO: /* bootloaderlock status in nv partition */
                tlogv("sec nv info access\n");
                break;
            case SEC_GET_TIME:
                tlogv("sec get time of day\n");
                GetTimeWork(transControl);
                break;
            default:
                tloge("misc agent error cmd\n");
                break;
        }

        __asm__ volatile("isb");
        __asm__ volatile("dsb sy");

        transControl->magic = AGENT_MISC_ID;

        __asm__ volatile("isb");
        __asm__ volatile("dsb sy");

        ret = ioctl(miscFd, (int)TC_NS_CLIENT_IOCTL_SEND_EVENT_RESPONSE, AGENT_MISC_ID);
        if (ret != 0) {
            tloge("misc agent send reponse failed\n");
            break;
        }
        tlogv("-- misc agent loop --\n");
    }

    return NULL;
}
