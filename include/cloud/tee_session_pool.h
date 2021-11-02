/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * iTrustee licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef _TEE_SESSION_POOL_H_
#define _TEE_SESSION_POOL_H_

#include <stdbool.h>
#include "tee_client_api.h"

struct SessionInfo {
    TEEC_Session session;
    bool isDead;
};

struct SessionPool {
    TEEC_Context *context;        /* context owner */
    TEEC_UUID uuid;
    uint32_t poolSize;            /* expected count of sessions to open */
    struct SessionInfo *sessionsInfo;
    uint32_t opened;              /* counf of sessions opend successfully */
    uint32_t inuse;               /* count of sessions in using */
    sem_t keys;                   /* keys value equal opend - inuse */
    uint8_t *usage;               /* a bitmap mark session in-use */
    uint32_t usageSize;           /* bitmap size in bytes */
    pthread_mutex_t usageLock;
};

TEEC_Result TEEC_SessionPoolCreate(TEEC_Context *context, const TEEC_UUID *destination,
    struct SessionPool **sessionPool, uint32_t poolSize);
TEEC_Result TEEC_SessionPoolInvoke(struct SessionPool *sessionPool, uint32_t commandID,
    TEEC_Operation *operation, uint32_t *returnOrigin);
void TEEC_SessionPoolDestroy(struct SessionPool *sessionPool);
void TEEC_SessionPoolQuery(struct SessionPool *sessionPool, uint32_t *size,
    uint32_t *opened, uint32_t *inuse, bool showBitmap);

#endif
