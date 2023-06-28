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
#ifndef LIBTEEC_MISC_WORK_AGENT_H
#define LIBTEEC_MISC_WORK_AGENT_H

#include <stdint.h>

#define WIDEVINE_NV_WVLOCK_SIZE   68
#define MISC_CONTROL_TIME_STR_LEN 30
#define AGENT_MISC_ID 0x4d495343

typedef enum {
    SEC_NV_INFO, /* bootloaderlock status in nv partition */
    SEC_GET_TIME,
} MiscCmdType;

struct MiscControlType {
    MiscCmdType cmd; /* for s to n */
    int32_t ret;
    int32_t magic;
    union Args2 {
        /* bootloader lock status in nv partition */
        struct {
            uint8_t bootloaderInfo[WIDEVINE_NV_WVLOCK_SIZE];
        } NvInfo;
        struct {
            uint32_t seconds;
            uint32_t millis;
            char timeStr[MISC_CONTROL_TIME_STR_LEN];
        } GetTime;
    } Args;
};

void *MiscWorkThread(void *control);
int GetMiscAgentFd(void);
void *GetMiscAgentControl(void);

int MiscAgentInit(void);
void MiscAgentThreadCreate(void);
void MiscAgentThreadJoin(void);
void MiscAgentExit(void);

#endif
