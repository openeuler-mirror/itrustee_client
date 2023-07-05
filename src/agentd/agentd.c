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

#include <unistd.h>
#include <sys/ioctl.h>
#include "tc_ns_client.h"
#include "tee_client_api.h"
#include "tee_log.h"
#include "tee_version_check.h"
#include "tee_agent.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "agentd"

static struct ModuleInfo g_agentdModuleInfo = {
    .deviceName = TC_NS_CVM_DEV_NAME,
    .moduleName = "agentd",
    .ioctlNum = TC_NS_CLIENT_IOCTL_GET_TEE_INFO,
};

int AgentdCheckTzdriverVersion(void)
{
    InitModuleInfo(&g_agentdModuleInfo);
    return CheckTzdriverVersion();
}

int main(void)
{
    if (AgentdCheckTzdriverVersion() != 0) {
        tloge("check tee agentd & tee driver version failed\n");
        return -1;
    }

    int ret = ProcessAgentInit();
    if (ret != 0) {
        tloge("agent init failed\n");
        return ret;
    }

    ProcessAgentThreadCreate();
    ProcessAgentThreadJoin();
    ProcessAgentExit();
    return 0;
}
