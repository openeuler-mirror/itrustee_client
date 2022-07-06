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

#include "tee_session_pool.h"

#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#include <securec.h>

#include "tee_client_api.h"
#include "tee_log.h"
#include "tee_client_inner.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "libteec_vendor"

#define SESSION_POOL_CAP_MIN 5
#define SESSION_POOL_CAP_MAX 100

static void FreeSessionPool(struct SessionPool *sp);

static struct SessionPool *AllocSessionPool(TEEC_Context *context,
    const TEEC_UUID *destination, uint32_t poolSize)
{
    struct SessionPool *sp = malloc(sizeof(struct SessionPool));
    if (sp == NULL) {
        tloge("alloc session pool fail failed\n");
        return NULL;
    }
    (void)memset_s(sp, sizeof(*sp), 0, sizeof(*sp));

    /*
     * As there are child pointers in context, session pool handle
     * only save a ref ptr to it. So caller need to guarantee it will
     * not be freed before destroy session pool.
     */
    sp->context = context;
    if (memcpy_s(&sp->uuid, sizeof(TEEC_UUID), destination, sizeof(*destination)) != EOK) {
        free(sp);
        return NULL;
    }

    sp->sessionsInfo = malloc(sizeof(*sp->sessionsInfo) * poolSize);
    sp->usageSize = poolSize / sizeof(uint8_t) + ((poolSize % sizeof(uint8_t)) ? 1 : 0);
    sp->usage = malloc(sp->usageSize);
    if (sp->sessionsInfo == NULL || sp->usage == NULL) {
        tloge("alloc session pool context fail\n");
        FreeSessionPool(sp);
        return NULL;
    }

    sp->poolSize = poolSize;

    (void)pthread_mutex_init(&sp->usageLock, NULL);
    (void)sem_init(&sp->keys, 0, 0);

    return sp;
}

static void FreeSessionPool(struct SessionPool *sp)
{
    if (sp != NULL) {
        if (sp->sessionsInfo != NULL) {
            free(sp->sessionsInfo);
        }
        if (sp->usage != NULL) {
            free(sp->usage);
        }
        free(sp);
    }
}

struct CreateSessionReq {
    struct SessionPool *sp;
    uint32_t reqCount;
    TEEC_Result ret;
};

static void *CreateSessionsFn(void *data)
{
    struct CreateSessionReq *req = (struct CreateSessionReq *)data;
    struct SessionPool *sp = req->sp;
    uint32_t i;
    TEEC_Operation operation = { 0 };
    TEEC_Result ret = TEEC_ERROR_GENERIC;
    uint32_t opened = sp->opened;

    for (i = opened; i < opened + req->reqCount; i++) {
        operation.started = 1;
        ret = TEEC_OpenSession(sp->context, &sp->sessionsInfo[i].session, &sp->uuid, TEEC_LOGIN_IDENTIFY,
            NULL, &operation, NULL);
        if (ret != TEEC_SUCCESS) {
            tloge("open session(%u/%u) failed, ret = 0x%x\n", i + 1, sp->poolSize, ret);
            break;
        } else {
            tlogd("open session(%u/%u) success\n", i + 1, sp->poolSize);
        }
        sp->sessionsInfo[i].isDead = false;
        (void)pthread_mutex_lock(&sp->usageLock);
        SetBit(i, sp->usageSize, sp->usage);
        sp->opened++;
        (void)pthread_mutex_unlock(&sp->usageLock);
        (void)sem_post(&sp->keys);
    }

    /*
     * reqCount = 1, in parent thread
     * reqCount > 1, in child thread
     */
    if (req->reqCount == 1) {
        req->ret = ret;
    } else {
        free(req);
    }

    return NULL;
}

TEEC_Result TEEC_SessionPoolCreate(TEEC_Context *context, const TEEC_UUID *destination,
    struct SessionPool **sessionPool, uint32_t poolSize)
{
    struct CreateSessionReq *req = NULL;
    struct SessionPool *sp = NULL;
    TEEC_Result ret;
    pthread_t worker;

    if (context == NULL || destination == NULL || sessionPool == NULL) {
        return TEEC_ERROR_BAD_PARAMETERS;
    }
    if (poolSize > SESSION_POOL_CAP_MAX || poolSize < SESSION_POOL_CAP_MIN) {
        return TEEC_ERROR_BAD_PARAMETERS;
    }

    sp = AllocSessionPool(context, destination, poolSize);
    if (sp == NULL) {
        tloge("alloc session pool failed\n");
        ret = TEEC_ERROR_OUT_OF_MEMORY;
        goto error;
    }
    req = malloc(sizeof(struct CreateSessionReq));
    if (req == NULL) {
        tloge("alloc req failed\n");
        ret = TEEC_ERROR_OUT_OF_MEMORY;
        goto error;
    }

    /*
     * We try to open 1 session at first to check if it can success,
     * then the rest sessions will be opened in a child thread so that the main
     * thread will not block too much time.
     * If want to know how many session is opened actually, you need to use
     * TEEC_SessionPoolQuery.
     */
    req->sp = sp;
    req->reqCount = 1;
    (void)CreateSessionsFn((void*)req);
    if (req->ret != TEEC_SUCCESS) {
        tloge("open first session failed, ret = 0x%x\n", req->ret);
        ret = req->ret;
        goto error;
    }

    /* req will be freed in child thread */
    req->reqCount = sp->poolSize - 1;
    int32_t err = pthread_create(&worker, NULL, &CreateSessionsFn, req);
    if (err != 0) {
        tloge("create worker failed, error = %d\n", err);
        ret = TEEC_ERROR_GENERIC;
        goto error;
    }
    pthread_detach(worker);

    *sessionPool = sp;

    return TEEC_SUCCESS;

error:
    TEEC_SessionPoolDestroy(sp);
    if (req != NULL) {
        free(req);
    }
    return ret;
}

static TEEC_Session *GetSessionFromPool(struct SessionPool *sp, int32_t *index)
{
    int32_t used;

    while ((sem_wait(&sp->keys)) == -1 && errno == EINTR) {
        continue;       /* restart if interrupted by signal */
    }
    (void)pthread_mutex_lock(&sp->usageLock);
    used = GetAndCleartBit(sp->usage, sp->usageSize);
    if (used != -1) {
        sp->inuse++;
    }
    (void)pthread_mutex_unlock(&sp->usageLock);

    /* shouldn't happen */
    if (used == -1) {
        tloge("can't get session, session bitmap may corrupted\n");
        (void)sem_post(&sp->keys);
        return NULL;
    }

    if (index != NULL) {
        *index = used;
    }

    return &sp->sessionsInfo[used].session;
}

static void PutSessionToPool(struct SessionPool *sp, int32_t index)
{
    (void)pthread_mutex_lock(&sp->usageLock);
    SetBit(index, sp->usageSize, sp->usage);
    sp->inuse--;
    (void)pthread_mutex_unlock(&sp->usageLock);

    if (sem_post(&sp->keys) < 0) {
        tloge("keys may corrupted, err=%d\n", errno);
    }
}

#define BITS_PER_UINT8 8
#define BITS_PER_UINT32 32

static void DumpSessionState(const struct SessionPool *sp)
{
    uint32_t i;
    char *array = NULL;
    uint32_t len;

    len = sp->poolSize + 1;
    array = malloc(len);
    if (array == NULL) {
        return;
    }
    (void)memset_s(array, len, 0, len);

    tloge("Session dead state:\n");
    for (i = 0; i < sp->poolSize; i++) {
        bool isDead = sp->sessionsInfo[i].isDead;

        if (isDead) {
            array[i] = '1';
        } else {
            array[i] = '0';
        }
    }
    tloge("%d-%u: %s\n", 0, sp->poolSize - 1, array);

    free(array);
}

static void DumpSessionPool(const struct SessionPool *sp)
{
    uint32_t i;
    char *bitmap = NULL;

    bitmap = malloc(BITS_PER_UINT32 + 1);
    if (bitmap == NULL) {
        return;
    }
    (void)memset_s(bitmap, BITS_PER_UINT32 + 1, 0, BITS_PER_UINT32 + 1);

    tloge("Session Pool: size=%u, opened=%u\n", sp->poolSize, sp->opened);
    tloge("Pool usage:\n");
    for (i = 0; i < sp->poolSize; i++) {
        uint8_t usage = sp->usage[i / BITS_PER_UINT8];
        uint32_t offset = i % BITS_PER_UINT8;

        if ((usage & (1<<offset)) != 0) {
            bitmap[i % BITS_PER_UINT32] = '1';
        } else {
            bitmap[i % BITS_PER_UINT32] = '0';
        }

        if ((i + 1) % BITS_PER_UINT32 == 0) {
            tloge("%u-%u: %s\n", i + 1 - BITS_PER_UINT32, i, bitmap);
            (void)memset_s(bitmap, BITS_PER_UINT32 + 1, 0, BITS_PER_UINT32 + 1);
        }
    }
    if ((sp->poolSize % BITS_PER_UINT32) != 0) {
        tloge("%u-%u: %s\n", i - i % BITS_PER_UINT32, sp->poolSize, bitmap);
    }

    free(bitmap);
}

static void DumpSessionInfo(struct SessionPool *sessionPool)
{
    if (sessionPool->poolSize > SESSION_POOL_CAP_MAX || sessionPool->poolSize < SESSION_POOL_CAP_MIN) {
        return;
    }

    DumpSessionPool(sessionPool);
    DumpSessionState(sessionPool);
}

TEEC_Result TEEC_SessionPoolInvoke(struct SessionPool *sessionPool, uint32_t commandID,
    TEEC_Operation *operation, uint32_t *returnOrigin)
{
    TEEC_Session *session = NULL;
    TEEC_Result ret;
    int32_t index;

    if (sessionPool == NULL || sessionPool->sessionsInfo == NULL) {
        return TEEC_ERROR_BAD_PARAMETERS;
    }

    session = GetSessionFromPool(sessionPool, &index);
    if (session == NULL) {
        tloge("can't get session from pool\n");
        /* shouldn't happen, dump session pool status */
        DumpSessionInfo(sessionPool);
        return TEEC_ERROR_BAD_PARAMETERS;
    }

    ret = TEEC_InvokeCommand(session, commandID, operation, returnOrigin);
    if (ret == TEEC_ERROR_TARGET_DEAD) {
        /* Session is crash, and not put bitmap and sem keys. */
        tloge("this session is dead: index=%d\n", index);
        sessionPool->sessionsInfo[index].isDead = true;
        DumpSessionInfo(sessionPool);
    } else {
        PutSessionToPool(sessionPool, index);
    }

    return ret;
}

void TEEC_SessionPoolDestroy(struct SessionPool *sessionPool)
{
    uint32_t i;

    if (sessionPool == NULL || sessionPool->sessionsInfo == NULL) {
        return;
    }

    for (i = 0; i < sessionPool->opened; i++) {
        TEEC_CloseSession(&sessionPool->sessionsInfo[i].session);
    }

    (void)sem_destroy(&sessionPool->keys);
    FreeSessionPool(sessionPool);
}

void TEEC_SessionPoolQuery(struct SessionPool *sessionPool, uint32_t *size,
    uint32_t *opened, uint32_t *inuse, bool showBitmap)
{
    if (sessionPool == NULL) {
        return;
    }

    if (size != NULL) {
        *size = sessionPool->poolSize;
    }
    if (opened != NULL) {
        *opened = sessionPool->opened;
    }
    if (inuse != NULL) {
        *inuse = sessionPool->inuse;
    }
    if (showBitmap && sessionPool->sessionsInfo != NULL) {
        DumpSessionInfo(sessionPool);
    }
}
