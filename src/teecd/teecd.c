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

#include "teecd.h"
#include <signal.h>
#include <pthread.h>
#include <limits.h>
#include "tc_ns_client.h"
#include "tee_client_api.h"
#include "tee_log.h"
#include "tee_ca_daemon.h"

#if defined(CONFIG_HIDL) || defined(CONFIG_CMS_CAHASH_AUTH) || defined(CONFIG_ARMPC_PLATFORM)
#include "tcu_authentication.h"
#endif

#include "tee_agent.h"

#if defined(DYNAMIC_DRV_DIR) || defined(DYNAMIC_CRYPTO_DRV_DIR) || defined(DYNAMIC_SRV_DIR)
#include "tee_load_dynamic.h"
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "teecd"

static int TeecdInit(void)
{
    if (GetTEEVersion() != 0) {
        tloge("get tee version failed\n");
    }

    if (TeecdCheckTzdriverVersion() != 0) {
        tloge("check tee client & tee driver version failed\n");
        return -1;
    }

#if defined(CONFIG_HIDL) || defined(CONFIG_CMS_CAHASH_AUTH) || defined(CONFIG_ARMPC_PLATFORM)
    /* Trans the xml file to tzdriver: */
    TcuAuthentication();
#endif
    return 0;
}

#ifdef CONFIG_LIBTEECD_SHARED
int teecd_main(void)
#else
int main(void)
#endif
{
    pthread_t caDaemonThread         = ULONG_MAX;

    if (TeecdInit() != 0) {
        return -1;
    }

    int ret = ProcessAgentInit();
    if (ret != 0) {
        return ret;
    }

    /* sync time to tee should be before ta&driver load to tee for v3.1 signature */
    TrySyncSysTimeToSecure();

#ifdef DYNAMIC_CRYPTO_DRV_DIR
    LoadDynamicCryptoDir();
#endif

    (void)pthread_create(&caDaemonThread, NULL, CaServerWorkThread, NULL);

    /*
     * register our signal handler, catch signal which default action is exit
     */
    /* ignore SIGPIPE(happened when CA created socket then be killed),teecd will not restart. */
    (void)signal(SIGPIPE, SIG_IGN);

    ProcessAgentThreadCreate();

#ifdef DYNAMIC_DRV_DIR
    LoadDynamicDrvDir();
#endif

#ifdef DYNAMIC_SRV_DIR
    LoadDynamicSrvDir();
#endif

    (void)pthread_join(caDaemonThread, NULL);
    ProcessAgentThreadJoin();

    ProcessAgentExit();
    return 0;
}
