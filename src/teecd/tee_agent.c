/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2013-2022. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "tee_agent.h"
#include <errno.h>     /* for errno */
#include <sys/types.h> /* for open close */
#include <sys/ioctl.h> /* for ioctl */
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include "tc_ns_client.h"
#include "tee_log.h"
#include "fs_work_agent.h"
#include "misc_work_agent.h"
#include "secfile_load_agent.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "teecd"

int AgentInit(unsigned int id, void **control)
{
    int ret;
    struct AgentIoctlArgs args = { 0 };

    if (control == NULL) {
        return -1;
    }
#ifdef CONFIG_AGENTD
    int fd = open(TC_NS_CVM_DEV_NAME, O_RDWR);
#else
    int fd = open(TC_TEECD_PRIVATE_DEV_NAME, O_RDWR);
#endif
    if (fd < 0) {
        tloge("open tee client dev failed, fd is %d\n", fd);
        return -1;
    }
    args.id         = id;
    args.bufferSize = TRANS_BUFF_SIZE;
    ret             = ioctl(fd, TC_NS_CLIENT_IOCTL_REGISTER_AGENT, &args);
    if (ret != 0) {
        (void)close(fd);
        tloge("private ioctl failed\n");
        return -1;
    }
#if __SIZEOF_POINTER__ == 8
    *control = args.buffer;
#else
    *control = (void *)(uintptr_t)args.addr;
#endif
    return fd;
}

void AgentExit(unsigned int id, int fd)
{
    int ret;

    if (fd == -1) {
        return;
    }

    ret = ioctl(fd, TC_NS_CLIENT_IOCTL_UNREGISTER_AGENT, id);
    if (ret != 0) {
        tloge("ioctl failed\n");
    }

    (void)close(fd);
}

struct AgentOps {
    int id;
    int (*agentInit)(void);
    void (*agentThreadCreate)(void);
    void (*agentThreadJoin)(void);
    void (*agentFini)(void);
};

static struct AgentOps g_agentOps[] = {
#ifdef CONFIG_AGENT_FS
    {AGENT_FS_ID, FsAgentInit, FsAgentThreadCreate, FsAgentThreadJoin, FsAgentExit},
#endif
#ifdef CONFIG_AGENT_MISC
    {AGENT_MISC_ID, MiscAgentInit, MiscAgentThreadCreate, MiscAgentThreadJoin, MiscAgentExit},
#endif
#ifdef CONFIG_AGENT_SECLOAD
    {SECFILE_LOAD_AGENT_ID, SecLoadAgentInit, SecLoadAgentThreadCreate, SecLoadAgentThreadJoin, SecLoadAgentExit},
#endif
};
static int g_agentNum = sizeof(g_agentOps) / sizeof(struct AgentOps);

int ProcessAgentInit(void)
{
    int index;
    int index2;
    for (index = 0; index < g_agentNum; index++) {
        if (g_agentOps[index].agentInit != NULL) {
            if (g_agentOps[index].agentInit() != 0) {
                break;
            }
        }
    }

    if (index == g_agentNum) {
        return 0;
    }

    /* not all agent is init success */
    for (index2 = 0; index2 < index; index2++) {
        if (g_agentOps[index2].agentFini != NULL) {
            g_agentOps[index2].agentFini();
        }
    }
    return -1;
}

void ProcessAgentThreadCreate(void)
{
    int index;
    for (index = 0; index < g_agentNum; index++) {
        if (g_agentOps[index].agentThreadCreate != NULL) {
            g_agentOps[index].agentThreadCreate();
        }
    }
}

void ProcessAgentThreadJoin(void)
{
    int index;
    for (index = 0; index < g_agentNum; index++) {
        if (g_agentOps[index].agentThreadJoin != NULL) {
            g_agentOps[index].agentThreadJoin();
        }
    }
}

void ProcessAgentExit(void)
{
    int index;
    for (index = 0; index < g_agentNum; index++) {
        if (g_agentOps[index].agentFini != NULL) {
            g_agentOps[index].agentFini();
        }
    }
}

#define SEC_MIN 0xFFFFF
#define NSEC_PER_MILLIS 1000000
static int SyncSysTimeToSecure(void)
{
    int ret;
    TC_NS_Time tcNsTime;
    struct timespec realTime;
    struct timespec sysTime;

    ret = clock_gettime(CLOCK_REALTIME, &realTime);
    if (ret != 0) {
        tloge("get real time failed ret=0x%x\n", ret);
        return ret;
    }

    ret = clock_gettime(CLOCK_MONOTONIC, &sysTime);
    if (ret != 0) {
        tloge("get system time failed ret=0x%x\n", ret);
        return ret;
    }

    if (realTime.tv_sec <= sysTime.tv_sec || (realTime.tv_sec - sysTime.tv_sec) < SEC_MIN) {
        tlogd("real time is not ready\n");
        return -1;
    }
    tcNsTime.seconds = (uint32_t)realTime.tv_sec;
    tcNsTime.millis  = (uint32_t)(realTime.tv_nsec / NSEC_PER_MILLIS);

    int fd = open(TC_TEECD_PRIVATE_DEV_NAME, O_RDWR);
    if (fd < 0) {
        tloge("Failed to open %s: %d\n", TC_TEECD_PRIVATE_DEV_NAME, errno);
        return fd;
    }
    ret = ioctl(fd, TC_NS_CLIENT_IOCTL_SYC_SYS_TIME, &tcNsTime);
    if (ret != 0) {
        tloge("failed to send sys time to teeos\n");
    }

    close(fd);
    return ret;
}

void TrySyncSysTimeToSecure(void)
{
    int ret;
    static int syncSysTimed = 0;

    if (syncSysTimed == 0) {
        ret = SyncSysTimeToSecure();
        if (ret != 0) {
            tlogw("failed to sync sys time to secure\n");
        } else {
            syncSysTimed = 1;
        }
    }
}
