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

#ifndef __SECFILE_LOAD_AGENT_H_
#define __SECFILE_LOAD_AGENT_H_

#include <sys/ioctl.h> /* for ioctl */
#include "tee_client_api.h"
#include "tee_log.h"
#include "tc_ns_client.h"

#define MAX_SEC_FILE_NAME_LEN 32
#define SECFILE_LOAD_AGENT_ID 0x4c4f4144

typedef enum {
    LOAD_TA_SEC = 0,
    LOAD_SERVICE_SEC,
    LOAD_LIB_SEC,
} SecAgentCmd;

struct SecAgentControlType {
    SecAgentCmd cmd;
    uint32_t magic;
    int32_t ret;
    int32_t error;
    union {
        struct {
            TEEC_UUID uuid;
        } TaSec;
        struct {
            TEEC_UUID uuid;
            char serviceName[MAX_SEC_FILE_NAME_LEN];
        } ServiceSec;
        struct {
            TEEC_UUID uuid;
            char libName[MAX_SEC_FILE_NAME_LEN];
        } LibSec;
    };
};

void *SecfileLoadAgentThread(void *control);

int GetSecLoadAgentFd(void);
void *GetSecLoadAgentControl(void);
int SecLoadAgentInit(void);
void SecLoadAgentThreadCreate(void);
void SecLoadAgentThreadJoin(void);
void SecLoadAgentExit(void);

#endif
