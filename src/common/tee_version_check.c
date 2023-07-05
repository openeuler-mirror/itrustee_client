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

#include "tee_version_check.h"

#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "tee_log.h"
#include "tc_ns_client.h"
#include "tee_client_version.h"

/* debug switch */
#ifdef LOG_NDEBUG
#undef LOG_NDEBUG
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "teec_version"

static struct ModuleInfo *g_moduleInfo = NULL;
static TC_NS_TEE_Info g_tee_info = {0};

void InitModuleInfo(struct ModuleInfo *info)
{
    if (info != NULL) {
        g_moduleInfo = info;
    }
}

static int GetTEEInfo(void)
{
    int ret;

    int fd = open(g_moduleInfo->deviceName, O_RDONLY);
    if (fd == -1) {
        tloge("Failed to open %s: %d\n", g_moduleInfo->deviceName, errno);
        return -1;
    }

    ret = ioctl(fd, g_moduleInfo->ioctlNum, &g_tee_info);
    (void)close(fd);
    if (ret != 0) {
        tloge("Failed to get tee info, err=%d\n", ret);
        return -1;
    }

    return ret;
}

int CheckTzdriverVersion(void)
{
    const uint16_t teecMajorVersion = TZDRIVER_VERSION_MAJOR;
    const uint16_t teecMinorVersion = TZDRIVER_VERSION_MINOR;

    if (g_moduleInfo == NULL || g_moduleInfo->deviceName == NULL || g_moduleInfo->moduleName == NULL) {
        tloge("Bad Params\n");
        return -1;
    }

    if (GetTEEInfo() != 0) {
        tloge("get tee info failed\n");
        return -1;
    }

    if (teecMajorVersion != g_tee_info.tzdriver_version_major) {
        tloge("check major version failed, %s expect tzdriver version=%u, actual version of the tzdriver = %u\n",
            g_moduleInfo->moduleName, teecMajorVersion, g_tee_info.tzdriver_version_major);
        return -1;
    }

    if (teecMinorVersion > g_tee_info.tzdriver_version_minor) {
        tloge("check minor version failed, %s expect tzdriver version %u.%u, actual version of the tzdriver = %u.%u\n",
            g_moduleInfo->moduleName, teecMajorVersion, teecMinorVersion,
            g_tee_info.tzdriver_version_major, g_tee_info.tzdriver_version_minor);
        return -1;
    } else {
        tlogi("current %s expect tzdriver version %u.%u, actual version of the tzdriver = %u.%u\n",
            g_moduleInfo->moduleName, teecMajorVersion, teecMinorVersion,
            g_tee_info.tzdriver_version_major, g_tee_info.tzdriver_version_minor);
    }

    return 0;
}
