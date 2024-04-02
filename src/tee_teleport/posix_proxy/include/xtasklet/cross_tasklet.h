/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All ritghts reserved.
 * Description: A tasklet can be used between processes
 */
#ifndef CROSS_TASKLET_H
#define CROSS_TASKLET_H

#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>

#include "blocking_queue.h"
#include "../common.h"
#include "thread_pool.h"

#define XTASKLET_BUF_MAGIC 0x12345678
struct Xtask {
    uint32_t magic;
    uint64_t id;
    sem_t done;
    uint8_t *buf;
    size_t bufSz;
    long ret;
    volatile atomic_ulong refCnt;
#ifdef CONFIG_XTASKLET_STAT
    unsigned long enqueueTask;
    unsigned long dequeueTask;
    unsigned long enqueueResult;
    unsigned long dequeueResult;
#endif
};

typedef long (*TaskFn)(uint8_t *membuf, void *priv);

struct Xtasklet {
    void *priv;
    volatile atomic_bool terminated;
    struct BlockingQueue *taskQ;
    struct BlockingQueue *resQ;
    struct ThreadPool fetchThPool;
    TaskFn fn;
};

struct XtaskletCreateProps {
    void *shm;
    size_t shmSz;
    size_t concurrency;
    TaskFn fn;
    void *priv;
};

int XtaskletCreate(const struct XtaskletCreateProps *props, struct Xtasklet **tasklet);
void XtaskletDestroy(struct Xtasklet *tasklet);
#endif
