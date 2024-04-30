/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All ritghts reserved.
 * Description: A safe blocking queue can be used cross processes
 */
#ifndef BLOCKING_QUEUE
#define BLOCKING_QUEUE

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../common.h"

#define CONFIG_BLOCKING_QUEUE_ENTRY_SIZE 512
#define CONFIG_BLOCKING_QUEUE_SLEEP_US 100
#define CONFIG_BLOCKING_QUEUE_ACC_CNT 2000

union Position {
    struct {
        uint32_t flip   : 1;
        uint32_t at     : 31;
    } pos;
    uint32_t val;
};

#define BLOCKING_QUEUE_INVALID_META_VALUE (-1)
union BlockingQueueEntryMeta {
    struct {
        int32_t size;
        int32_t remain;
    } prop;
    uint64_t val;
};

struct BlockingQueueEntry {
    union BlockingQueueEntryMeta meta;
    uint8_t data[CONFIG_BLOCKING_QUEUE_ENTRY_SIZE];
};

struct Ringbuffer {
    union {
        struct {
            union Position head;
            union Position tail;
        } pos;
        uint64_t posVal;
    };
};

struct BlockingQueue {
    bool isProducer;
    volatile atomic_bool interrupt;
    bool concurrent;
    unsigned long blockingAcc;
    volatile atomic_ulong refCnt;
    pthread_mutex_t sync;
    struct Ringbuffer *buffer;
    struct BlockingQueueEntry *entries;
    uint32_t entriesNr;
};

int BlockingQueueCreate(void *mem, size_t memSize, struct BlockingQueue **queue, bool producer, bool concurrent);
void BlockingQueueDestroy(struct BlockingQueue *queue);
void BlockingQueueInterrupt(struct BlockingQueue *queue);
#define BLOCKING_QUEUE_INTERRUPTED 0xFF8
#define BLOCKING_QUEUE_LARGER_ENTRY 0xFF7
int BlockingEnqueue(struct BlockingQueue *queue, void *src, uint32_t srcSize, long timeoutUs);
int BlockingDequeue(struct BlockingQueue *queue, void **dst, long timeoutUs);

#endif
