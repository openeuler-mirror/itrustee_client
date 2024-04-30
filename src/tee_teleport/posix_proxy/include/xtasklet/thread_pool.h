/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All ritghts reserved.
 * Description: A thread pool which can initialize/finalize mutiple threads
 */
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stdbool.h>
#include <pthread.h>

#include "../common.h"

typedef void *(*ThTask)(void *arg);

struct ThreadInfo {
    pthread_t tid;
    int id;
    bool initiated;
    ThTask taskHandler;
    ThTask interruptHandler;
    void *handlerArg;
};

struct ThreadPool {
    struct ThreadInfo *ths;
    size_t size;
};

int ThreadPoolInit(struct ThreadPool *pool, size_t threadCnt,
                   ThTask taskHandler, ThTask interruptHandler, void *handlerArg);
void ThreadPoolFinalize(struct ThreadPool *pool);

#endif
