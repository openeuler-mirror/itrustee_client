/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All ritghts reserved.
 * Description: A thread pool which can initialize/finalize mutiple threads
 */
#include <thread_pool.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#include <common.h>

void ThreadPoolFinalize(struct ThreadPool *pool)
{
    if (pool == NULL) {
        ERR("invalid null pointer\n");
        goto end;
    }
    for (size_t i = 0; i < pool->size; ++i) {
        struct ThreadInfo *th = &pool->ths[i];
        if (th->initiated) {
            if (th->interruptHandler != NULL) {
                (void)th->interruptHandler(th->handlerArg);
            }
        }
    }
    for (size_t i = 0; i < pool->size; ++i) {
        struct ThreadInfo *th = &pool->ths[i];
        if (th->initiated) {
            (void)pthread_join(th->tid, NULL);
        }
    }
    free(pool->ths);
end:
    return;
}

static void DumpThread(struct ThreadInfo *th)
{
#ifdef CONFIG_DEBUG_BUILD
    if (th != NULL) {
        DBG("starting thread: id %d, tid 0x%lx, initiated %s, taskHandler: 0x%lx, handlerArg 0x%lx\n",
            th->id,
            (unsigned long)th->tid,
            th->initiated ? "true" : "false",
            (unsigned long)th->taskHandler,
            (unsigned long)th->handlerArg);
    } else {
        DBG("null thread info\n");
    }
#else
    (void)th;
#endif
}

static void *ThreadExecuteTask(void *data)
{
    struct ThreadInfo *th = (struct ThreadInfo *)data;
    DumpThread(th);
    return th->taskHandler(th->handlerArg);
}

int ThreadPoolInit(struct ThreadPool *pool, size_t threadCnt,
                   ThTask taskHandler, ThTask interruptHandler, void *handlerArg)
{
    int ret = 0;
    if (pool == NULL) {
        ret = 1;
        ERR("invalid null pointer\n");
        goto end;
    }
    pool->size = threadCnt;
    pool->ths = calloc(pool->size, sizeof(struct ThreadInfo));
    if (pool->ths == NULL) {
        ret = errno;
        ERR("allocate threads failed: %s\n", strerror(ret));
        goto end;
    }
    for (size_t i = 0; i < threadCnt; ++i) {
        struct ThreadInfo *th = &pool->ths[i];
        th->id = i;
        th->taskHandler = taskHandler;
        th->interruptHandler = interruptHandler;
        th->handlerArg = handlerArg;
        th->initiated = false;
        ret = pthread_create(&th->tid, NULL, ThreadExecuteTask, th);
        if (ret != 0) {
            ERR("create pthread failed, %s\n", strerror(ret));
            goto finalize_pool;
        }
        th->initiated = true;
    }
    DBG("thread pool initiated success\n");
    goto end;
finalize_pool:
    ThreadPoolFinalize(pool);
end:
    return ret;
}
