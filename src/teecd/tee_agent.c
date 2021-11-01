/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2013-2021. All rights reserved.
 * iTrustee licensed under the Mulan PSL v2.
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

#include "tc_ns_client.h"
#include "tee_client_api.h"
#include "tee_log.h"
#include "tee_ca_daemon.h"
#include "secfile_load_agent.h"
#include "misc_work_agent.h"
#include "fs_work_agent.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "teecd"

/* smc dev */
static int g_fsFd   = -1;
static int g_miscFd = -1;

int GetMiscFd(void)
{
    return g_miscFd;
}

int GetFsFd(void)
{
    return g_fsFd;
}

static int AgentInit(unsigned int id, void **control)
{
    int ret;
    struct AgentIoctlArgs args = { 0 };

    if (control == NULL) {
        return -1;
    }
    int fd = open(TC_NS_CLIENT_DEV_NAME, O_RDWR);
    if (fd < 0) {
        tloge("open tee client dev failed, fd is %d\n", fd);
        return -1;
    }

    /* register agent */
    args.id         = id;
    args.bufferSize = TRANS_BUFF_SIZE;
    ret             = ioctl(fd, (int)TC_NS_CLIENT_IOCTL_REGISTER_AGENT, &args);
    if (ret) {
        (void)close(fd);
        tloge("ioctl failed\n");
        return -1;
    }

    *control = args.buffer;
    return fd;
}

static void AgentExit(unsigned int id, int fd)
{
    int ret;

    if (fd == -1) {
        return;
    }

    ret = ioctl(fd, (int)TC_NS_CLIENT_IOCTL_UNREGISTER_AGENT, id);
    if (ret) {
        tloge("ioctl failed\n");
    }

    (void)close(fd);
}

static struct SecStorageType *g_fsControl                = NULL;
static struct MiscControlType *g_miscControl             = NULL;
static struct SecAgentControlType *g_secLoadAgentControl = NULL;

static int g_fsThreadFlag = 0;

static int ProcessAgentInit()
{
    int ret;
#ifdef CONFIG_AGENT_FS
    g_fsFd = AgentInit(AGENT_FS_ID, (void **)(&g_fsControl));
    if (g_fsFd < 0) {
        tloge("fs agent init failed\n");
        g_fsThreadFlag = 0;
    } else {
        g_fsThreadFlag = 1;
    }
#endif
    g_miscFd = AgentInit(AGENT_MISC_ID, (void **)(&g_miscControl));
    if (g_miscFd < 0) {
        tloge("misc agent init failed\n");
        goto ERROR1;
    }

    ret = AgentInit(SECFILE_LOAD_AGENT_ID, (void **)(&g_secLoadAgentControl));
    if (ret < 0) {
        tloge("secfile load agent init failed\n");
        goto ERROR2;
    }

    SetSecLoadAgentFd(ret);

    return 0;
ERROR2:
    AgentExit(AGENT_MISC_ID, g_miscFd);
    g_miscFd      = -1;
    g_miscControl = NULL;

ERROR1:
    if (g_fsThreadFlag == 1) {
        AgentExit(AGENT_FS_ID, g_fsFd);
        g_fsFd         = -1;
        g_fsControl    = NULL;
        g_fsThreadFlag = 0;
    }
    return -1;
}

static void ProcessAgentExit()
{
    if (g_fsThreadFlag == 1) {
        AgentExit(AGENT_FS_ID, g_fsFd);
        g_fsFd      = -1;
        g_fsControl = NULL;
    }

    AgentExit(AGENT_MISC_ID, g_miscFd);
    g_miscFd      = -1;
    g_miscControl = NULL;

    AgentExit(SECFILE_LOAD_AGENT_ID, GetSecLoadAgentFd());
    SetSecLoadAgentFd(-1);
    g_secLoadAgentControl = NULL;
}

int main(void)
{
    pthread_t fsThread               = -1;
    pthread_t miscThread             = -1;
    pthread_t caDaemonThread         = -1;
    pthread_t secfileLoadAgentThread = -1;
    int32_t type = TEECD_CONNECT;
    if (GetTEEVersion() != 0) {
        tloge("get tee version failed\n");
    }

    int ret = ProcessAgentInit();
    if (ret) {
        return ret;
    }

    (void)pthread_create(&caDaemonThread, NULL, CaServerWorkThread, &type);

    SetFileNumLimit();

    /*
     * register our signal handler, catch signal which default action is exit
     */
    /* ignore SIGPIPE(happened when CA created socket then be killed),teecd will not restart. */
    signal(SIGPIPE, SIG_IGN);

    if (g_fsThreadFlag == 1) {
        (void)pthread_create(&fsThread, NULL, FsWorkThread, g_fsControl);
    }
    (void)pthread_create(&miscThread, NULL, MiscWorkThread, g_miscControl);
    (void)pthread_create(&secfileLoadAgentThread, NULL, SecfileLoadAgentThread, g_secLoadAgentControl);

    if (g_fsThreadFlag == 1) {
        (void)pthread_join(fsThread, NULL);
    }
    (void)pthread_join(miscThread, NULL);
    (void)pthread_join(caDaemonThread, NULL);
    (void)pthread_join(secfileLoadAgentThread, NULL);

    ProcessAgentExit();
    return 0;
}
