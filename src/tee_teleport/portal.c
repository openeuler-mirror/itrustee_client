/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <securec.h>

#include "scp.h"
#include "tee_client_type.h"
#include "tee_client_api.h"
#include "tee_sys_log.h"
#include "tc_ns_client.h"
#include "tee_version_check.h"
#include "portal.h"
#include "posix_proxy.h"

#define DEF_PORTAL_SIZE (16 * 1024 * 1024)   /* 16MB */
static int g_devFd = -EINVAL;
static void *g_portal;
static uint32_t g_portalSize;

static struct ModuleInfo g_teleportModuleInfo = {
    .deviceName = TC_NS_CVM_DEV_NAME,
    .moduleName = "tee_teleport",
    .ioctlNum = TC_NS_CLIENT_IOCTL_GET_TEE_INFO,
};

static int32_t TeleportCheckTzdriverVersion(void)
{
    InitModuleInfo(&g_teleportModuleInfo);
    return CheckTzdriverVersion();
}

int InitPortal(void)
{
    struct AgentIoctlArgs args = { 0 };
    int fd;
    long pageSize;
    int ret;

    if (TeleportCheckTzdriverVersion() != 0) {
        printf("check teleport & tzdriver failed\n");
        return -EFAULT;
    }

    if (g_devFd != -EINVAL) {
        printf("tee client dev has already been opened\n");
        return -EINVAL;
    }

    fd = open(TC_NS_CVM_DEV_NAME, O_RDWR);
    if (fd < 0) {
        printf("open tee client dev failed\n");
        return -EFAULT;
    }

    pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) {
        printf("cannot get page size\n");
        close(fd);
        return -EFAULT;
    }
    /* alloc shared memory for portal */
    args.buffer = aligned_alloc((unsigned long)pageSize, DEF_PORTAL_SIZE);
    if (args.buffer == NULL) {
        printf("cannot allocate memory\n");
        close(fd);
        return -ENOMEM;
    }
    args.bufferSize = DEF_PORTAL_SIZE;

    ret = ioctl(fd, (unsigned long)TC_NS_CLIENT_IOCTL_PORTAL_REGISTER, &args);
    if (ret != 0) {
        if (errno == ENOTSUP) {
            printf("bad iTrustee version, tee_teleport not supported\n");
        } else if (errno == ENOMEM) {
            printf("please check memory settings (tee_teleport cannot run with 1GB TEE memory)\n");
        } else {
            printf("register portal failed\n");
        }
        free(args.buffer);
        close(fd);
        g_devFd = -EINVAL;
        return -EFAULT;
    }

    g_devFd = fd;
    g_portal = args.buffer;
    g_portalSize = args.bufferSize;
    return 0;
}

int GetPortal(void **portal, uint32_t *portalSize)
{
    if (portal == NULL || portalSize == NULL) {
        printf("cannot get portal, bad param\n");
        return -EINVAL;
    }

    if (g_portal == NULL || g_portalSize == 0)
        return -EINVAL;

    *portal = g_portal;
    *portalSize = g_portalSize;
    return 0;
}

int TriggerPortal(void)
{
    struct AgentIoctlArgs args = { 0 };
    return ioctl(g_devFd, (unsigned long)TC_NS_CLIENT_IOCTL_PORTAL_WORK, &args);
}

void DestroyPortal(void)
{
    if (g_devFd == -EINVAL)
        return;
    close(g_devFd);
    free(g_portal);
    g_devFd = -EINVAL;
    g_portal = NULL;
    g_portalSize = 0;
}

#ifdef CROSS_DOMAIN_PERF
int PosixProxyRegisterTaskletRequest(int devFd, struct PosixProxyIoctlArgs *args)
{
    if (args == NULL || devFd < 0)
        return -EFAULT;

    return ioctl(devFd, (unsigned long)TC_NS_CLIENT_IOCTL_POSIX_PROXY_REGISTER_TASKLET, args);
}

int PosixProxyInitDev(void)
{
    int devFd = open(TC_NS_CVM_DEV_NAME, O_RDWR);
    if (devFd < 0) {
        printf("open tee client dev failed\n");
        return -EFAULT;
    }
    PosixProxySetDevFD(devFd);
    return devFd;
}

void PosixProxyExitDev(int devFd)
{
    if (devFd >= 0)
        close(devFd);
    PosixProxySetDevFD(-1);
}
#endif