/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All ritghts reserved.
 * Description: A tasklet can be used between processes
 */
#include <cross_tasklet.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <securec.h>

#include <thread_pool.h>
#include <blocking_queue.h>

enum TaskState {
    STATE_ENQUEUE_TASK,
    STATE_DEQUEUE_TASK,
    STATE_ENQUEUE_RESULT,
    STATE_DEQUEUE_RESULT,
};

static void RecordTimestamp(struct Xtask *task, enum TaskState state)
{
#ifdef CONFIG_XTASKLET_STAT
    switch (state) {
        case STATE_ENQUEUE_TASK:
            (void)GetTimestampUs(&task->enqueueTask);
            break;
        case STATE_DEQUEUE_TASK:
            (void)GetTimestampUs(&task->dequeueTask);
            break;
        case STATE_ENQUEUE_RESULT:
            (void)GetTimestampUs(&task->enqueueResult);
            break;
        case STATE_DEQUEUE_RESULT:
            (void)GetTimestampUs(&task->dequeueResult);
            break;
        default:
            break;
    }
#else
    (void)task;
    (void)state;
#endif
}

static int DequeuedTaskCreate(struct BlockingQueue *q, struct Xtask **t)
{
    int ret = 0;
    void *buf = NULL;
    ret = BlockingDequeue(q, &buf, -1);
    if (ret != 0) {
        if (ret != BLOCKING_QUEUE_INTERRUPTED) {
            ERR("dequeue failed\n");
        }
        goto end;
    }
    struct Xtask *task = (struct Xtask *)buf;
    task->buf = (uint8_t *)((uintptr_t)task + sizeof(struct Xtask));
    if (task->magic != XTASKLET_BUF_MAGIC) {
        ret = -EINVAL;
        ERR("task magic is not match\n");
        goto end;
    }
    *t = task;
end:
    return ret;
}

static void DequeuedTaskDestroy(struct Xtask *task)
{
    free(task);
}

static void *ExecutorFetch(void *data)
{
    struct Xtasklet *tl = (struct Xtasklet *)data;
    while (!atomic_load(&tl->terminated)) {
        int ret = 0;
        struct Xtask *task = NULL;
        ret = DequeuedTaskCreate(tl->taskQ, &task);
        if (ret != 0) {
            if (atomic_load(&tl->terminated)) {
                DBG("tasklet is terminated\n");
            } else {
                ERR("create task obj from task queue failed\n");
            }
            goto loop;
        }
        RecordTimestamp(task, STATE_DEQUEUE_TASK);
        task->ret = tl->fn(task->buf, tl->priv);
        RecordTimestamp(task, STATE_ENQUEUE_RESULT);
        ret = BlockingEnqueue(tl->resQ, (void *)task, sizeof(struct Xtask) + task->bufSz, -1);
        if (ret != 0 && ret != BLOCKING_QUEUE_INTERRUPTED) {
            ERR("enqueue task result failed\n");
        }
        DequeuedTaskDestroy(task);
        goto loop;

loop:
        continue;
    }
    return NULL;
}

int XtaskletCreate(const struct XtaskletCreateProps *props, struct Xtasklet **tasklet)
{
    int ret = 0;
    if (props == NULL || props->shm == NULL || tasklet == NULL) {
        ret = 1;
        ERR("invalid null pointer\n");
        goto end;
    }
    struct Xtasklet *tl = (struct Xtasklet *)malloc(sizeof(struct Xtasklet));
    if (tl == NULL) {
        ret = errno;
        ERR("allocate xtasklet failed, %s\n", strerror(ret));
        goto end;
    }
    atomic_init(&tl->terminated, false);
    ret = BlockingQueueCreate(props->shm, props->shmSz / 2,
                              &tl->taskQ, false, props->concurrency > 1);
    if (ret != 0) {
        ERR("create task queue failed\n");
        goto free_tl;
    }
    ret = BlockingQueueCreate(props->shm + props->shmSz / 2, props->shmSz / 2,
                              &tl->resQ, true, props->concurrency > 1);
    if (ret != 0) {
        ERR("create result queue failed\n");
        goto destroy_taskQ;
    }
    tl->fn = props->fn;
    ret = ThreadPoolInit(&tl->fetchThPool, props->concurrency, ExecutorFetch, NULL, tl);
    if (ret != 0) {
        ERR("init thread pool failed\n");
        goto destroy_resQ;
    }
    tl->priv = props->priv;
    *tasklet = tl;
    goto end;

destroy_resQ:
    BlockingQueueInterrupt(tl->resQ);
    BlockingQueueDestroy(tl->resQ);
destroy_taskQ:
    BlockingQueueInterrupt(tl->taskQ);
    BlockingQueueDestroy(tl->taskQ);
free_tl:
    free(tl);
end:
    return ret;
}

void XtaskletDestroy(struct Xtasklet *tl)
{
    if (tl == NULL) {
        ERR("invalid null pointer\n");
        return;
    }
    atomic_store(&tl->terminated, true);
    BlockingQueueInterrupt(tl->resQ);
    BlockingQueueInterrupt(tl->taskQ);
    ThreadPoolFinalize(&tl->fetchThPool);
    BlockingQueueDestroy(tl->resQ);
    BlockingQueueDestroy(tl->taskQ);
    free(tl);
    DBG("xtasklet is destroyed\n");
}
