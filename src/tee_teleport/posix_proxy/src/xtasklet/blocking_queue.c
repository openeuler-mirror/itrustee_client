/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All ritghts reserved.
 * Description: A safe blocking queue can be used cross processes
 */
#include <blocking_queue.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>
#include <securec.h>

#include <common.h>

static void SetHead(struct Ringbuffer *buffer, uint32_t at, bool flip)
{
    __asm__ volatile("isb");
    __asm__ volatile("dmb ish");
    uint32_t val = flip ? 1 : 0;
    val |= (at << 1);
    buffer->pos.head.val = val;
}

static void SetTail(struct Ringbuffer *buffer, uint32_t at, bool flip)
{
    __asm__ volatile("isb");
    __asm__ volatile("dmb ish");
    uint32_t val = flip ? 1 : 0;
    val |= (at << 1);
    buffer->pos.tail.val = val;
}

#define FLIP_MASK       0x00000001U
#define POS_MASK        0xFFFFFFFEU
#define BITS_PER_BYTE   8
static inline void GetHeadTail(struct BlockingQueue *queue,
                               uint32_t *head, bool *headFlip,
                               uint32_t *tail, bool *tailFlip)
{
    uint64_t val = queue->buffer->posVal;
    *headFlip = val & FLIP_MASK;
    *head = (val & POS_MASK) >> 1;
    val >>= (sizeof(union Position) * BITS_PER_BYTE);
    *tailFlip = val & FLIP_MASK;
    *tail = (val & POS_MASK) >> 1;
}

#define MIN_ENTRY_CNT 2
static int InitRingbuffer(struct BlockingQueue *q, void *mem, size_t memSize, bool isProducer)
{
    int ret = 0;
    if (memSize < sizeof(struct Ringbuffer) + MIN_ENTRY_CNT * sizeof(struct BlockingQueueEntry)) {
        ret = 1;
        ERR("memory size is too small for initializing queue\n");
        goto end;
    }
    q->buffer = (struct Ringbuffer *)mem;
    size_t bufferSz = memSize - sizeof(struct Ringbuffer);
    q->entries = (struct BlockingQueueEntry *)((uintptr_t)mem + sizeof(struct Ringbuffer));
    q->entriesNr = bufferSz / sizeof(struct BlockingQueueEntry);
    if (isProducer) {
        (void)memset_s(q->entries, bufferSz, 0, bufferSz);
        SetHead(q->buffer, 0, false);
    } else {
        SetTail(q->buffer, 0, false);
    }
end:
    return ret;
}

int BlockingQueueCreate(void *mem, size_t memSize, struct BlockingQueue **queue, bool isProducer, bool concurrent)
{
    int ret = 0;
    if (mem == NULL || queue == NULL) {
        ret = 1;
        ERR("invalid null pointer\n");
        goto end;
    }
    struct BlockingQueue *q = (struct BlockingQueue *)malloc(sizeof(struct BlockingQueue));
    if (q == NULL) {
        ret = errno;
        ERR("allocate blocking queue failed, %s\n", strerror(ret));
        goto end;
    }
    q->isProducer = isProducer;
    q->concurrent = concurrent;
    atomic_init(&q->interrupt, false);
    atomic_init(&q->refCnt, 1);
    q->blockingAcc = CONFIG_BLOCKING_QUEUE_ACC_CNT;
    if (concurrent) {
        ret = pthread_mutex_init(&q->sync, NULL);
        if (ret != 0) {
            ret = errno;
            ERR("init sync lock failed\n");
            goto free_q;
        }
    }
    ret = InitRingbuffer(q, mem, memSize, isProducer);
    if (ret != 0) {
        ERR("init ringbuffer failed\n");
        goto destroy_sync;
    }
    *queue = q;
    DBG("blocking queue created, entry number: %u\n", q->entriesNr);
    goto end;

destroy_sync:
    if (concurrent) {
        pthread_mutex_destroy(&q->sync);
    }
free_q:
    free(q);
end:
    return ret;
}

void BlockingQueueInterrupt(struct BlockingQueue *queue)
{
    if (queue == NULL) {
        ERR("invalid null pointer\n");
        goto end;
    }
    atomic_store(&queue->interrupt, true);
end:
    return;
}

static void RefQueue(struct BlockingQueue *q)
{
    (void)atomic_fetch_add(&q->refCnt, 1);
}

static void DerefQueue(struct BlockingQueue *q)
{
    if (atomic_fetch_sub(&q->refCnt, 1) == 1) {
        if (q->concurrent) {
            (void)pthread_mutex_destroy(&q->sync);
        }
        free(q);
        DBG("blocking queue is destroyed\n");
    }
}

void BlockingQueueDestroy(struct BlockingQueue *queue)
{
    if (queue == NULL) {
        ERR("invalid null pointer\n");
        goto end;
    }
    if (!atomic_load(&queue->interrupt)) {
        ERR("blocking queue should be interrupt first\n");
        goto end;
    }
    DerefQueue(queue);
end:
    return;
}

static inline int LockQueue(struct BlockingQueue *queue)
{
    int ret = 0;
    if (queue->concurrent) {
        ret = pthread_mutex_lock(&queue->sync);
        if (ret != 0) {
            ret = errno;
            ERR("aquire lock failed, %s\n", strerror(ret));
        }
    }
    return ret;
}

static inline void UnlockQueue(struct BlockingQueue *queue)
{
    if (queue->concurrent) {
        (void)pthread_mutex_unlock(&queue->sync);
    }
}

static int Blocking(struct BlockingQueue *queue, long *timeoutUs)
{
    int ret = 0;
    if (*timeoutUs != -1 && *timeoutUs < CONFIG_BLOCKING_QUEUE_SLEEP_US) {
        ret = ETIMEDOUT;
        ERR("blocking timeout\n");
        goto end;
    }
    if (queue->blockingAcc == 0) {
        ret = usleep(CONFIG_BLOCKING_QUEUE_SLEEP_US);
        if (ret != 0) {
            ret = errno;
            ERR("sleep failed, %s\n", strerror(ret));
            goto end;
        }
        if (*timeoutUs != -1) {
            *timeoutUs = *timeoutUs - CONFIG_BLOCKING_QUEUE_SLEEP_US;
        }
    } else {
        queue->blockingAcc -= 1;
    }
end:
    return ret;
}

static void Unblocking(struct BlockingQueue *queue)
{
    queue->blockingAcc += CONFIG_BLOCKING_QUEUE_ACC_CNT;
}

static int WaitFull(struct BlockingQueue *queue, long *remainTimeout, uint32_t srcSize,
                    uint32_t *head, bool *headFlip)
{
    int ret = 0;
    uint32_t total = srcSize / CONFIG_BLOCKING_QUEUE_ENTRY_SIZE;
    total += srcSize % CONFIG_BLOCKING_QUEUE_ENTRY_SIZE == 0 ? 0 : 1;
    uint32_t len = queue->entriesNr;
    if (total > len) {
        ret = 1;
        ERR("data is larger than buffer, %u %u\n", total, len);
        goto end;
    }
    uint32_t nextHead;
    uint32_t tail;
    bool tailFlip;
    bool nextHeadFlip;
    bool isFull = true;
    while (isFull) {
        if (atomic_load(&queue->interrupt)) {
            ret = BLOCKING_QUEUE_INTERRUPTED;
            DBG("queue is interrupted\n");
            goto end;
        }
        GetHeadTail(queue, head, headFlip, &tail, &tailFlip);
        nextHead = (*head + total) % len;
        nextHeadFlip = (bool)((uint32_t)(((*head) + total) >= len) ^ (uint32_t)(*headFlip));
        isFull = (nextHeadFlip != tailFlip && nextHead >= tail) ||
                 (*headFlip != tailFlip && nextHeadFlip == tailFlip);
        if (isFull) {
            ret = Blocking(queue, remainTimeout);
            if (ret != 0) {
                ERR("blocking queue failed\n");
                goto end;
            }
        }
    }
    Unblocking(queue);
end:
    return ret;
}

static void SetMetadata(struct BlockingQueueEntry *entries, uint32_t pos, int32_t size, int32_t remain)
{
    entries[pos].meta.prop.size = size;
    entries[pos].meta.prop.remain = remain;
}

static void EnqueueBlocks(struct BlockingQueue *queue,
                          uint32_t head, bool headFlip,
                          void *src, int32_t srcSize)
{
    int32_t offset = 0;
    int32_t total = srcSize / CONFIG_BLOCKING_QUEUE_ENTRY_SIZE;
    total += srcSize % CONFIG_BLOCKING_QUEUE_ENTRY_SIZE == 0 ? 0 : 1;
    uint32_t len = queue->entriesNr;
    uint32_t nextHead = (head + total) % len;
    bool nextHeadFlip = (bool)((uint32_t)((head + total) >= len) ^ (uint32_t)headFlip);
    for (uint32_t i = head; i != nextHead; i = (i + 1) % len) {
        SetMetadata(queue->entries, i,
                    i != head ? BLOCKING_QUEUE_INVALID_META_VALUE : srcSize,
                    i != head ? BLOCKING_QUEUE_INVALID_META_VALUE : total - 1);
        struct BlockingQueueEntry *entry = &queue->entries[i];
        size_t cnt = offset + CONFIG_BLOCKING_QUEUE_ENTRY_SIZE < srcSize ?
                     CONFIG_BLOCKING_QUEUE_ENTRY_SIZE :
                     srcSize - offset;
        (void)memcpy_s(entry->data, cnt, src + offset, cnt);
        offset += cnt;
    }
    SetHead(queue->buffer, nextHead, nextHeadFlip);
}

int BlockingEnqueue(struct BlockingQueue *queue, void *src, uint32_t srcSize, long timeoutUs)
{
    int ret = 0;
    if (queue == NULL || src == NULL) {
        ret = 1;
        ERR("invalid null pointer\n");
        goto end;
    }
    if (!queue->isProducer) {
        ret = 1;
        ERR("consumer cannot dequeue\n");
        goto end;
    }
    if (srcSize > queue->entriesNr * CONFIG_BLOCKING_QUEUE_ENTRY_SIZE) {
        ret = 1;
        ERR("data size is too large\n");
        goto end;
    }
    RefQueue(queue);
    ret = LockQueue(queue);
    if (ret != 0) {
        ERR("lock queue for enqueue failed\n");
        goto deref;
    }
    uint32_t head;
    bool headFlip;
    ret = WaitFull(queue, &timeoutUs, srcSize, &head, &headFlip);
    if (ret != 0) {
        if (ret != BLOCKING_QUEUE_INTERRUPTED) {
            ERR("wait queue non full failed\n");
        }
        goto unlock;
    }
    EnqueueBlocks(queue, head, headFlip, src, srcSize);

unlock:
    UnlockQueue(queue);
deref:
    DerefQueue(queue);
end:
    return ret;
}

static int WaitEmpty(struct BlockingQueue *queue, long *remainTimeout, uint32_t *tail, bool *tailFlip)
{
    int ret = 0;
    uint32_t head;
    bool headFlip;
    bool isEmpty = true;
    while (isEmpty) {
        if (atomic_load(&queue->interrupt)) {
            ret = BLOCKING_QUEUE_INTERRUPTED;
            DBG("queue interrupted\n");
            goto end;
        }
        GetHeadTail(queue, &head, &headFlip, tail, tailFlip);
        isEmpty = headFlip == *tailFlip && head == *tail;
        if (isEmpty) {
            ret = Blocking(queue, remainTimeout);
            if (ret != 0) {
                ERR("blocking queue failed\n");
                goto end;
            }
        }
    }
    Unblocking(queue);
end:
    return ret;
}

static int GetMetadata(struct BlockingQueueEntry *entries, uint32_t pos, union BlockingQueueEntryMeta *res)
{
    int ret = 0;
    struct BlockingQueueEntry *entry = &entries[pos];
    res->val = entry->meta.val;
    if (res->prop.size == BLOCKING_QUEUE_INVALID_META_VALUE ||
        res->prop.remain == BLOCKING_QUEUE_INVALID_META_VALUE) {
        ret = 1;
        ERR("bad position, entry on position contains no metadata\n");
        goto end;
    }
    if (res->prop.size > (res->prop.remain + 1) * CONFIG_BLOCKING_QUEUE_ENTRY_SIZE) {
        ret = 1;
        ERR("bad size, too large\n");
    }
end:
    return ret;
}

static int AllocateDequeueBuffer(const union BlockingQueueEntryMeta *meta, void **buf)
{
    int ret = 0;
    void *ptr = calloc(meta->prop.size, 1);
    if (ptr == NULL) {
        ret = errno;
        ERR("allocate buffer failed, %s\n", strerror(ret));
        goto end;
    }
    *buf = ptr;
end:
    return ret;
}

static void DequeueBlocks(struct BlockingQueue *queue,
                          uint32_t tail, bool tailFlip,
                          const union BlockingQueueEntryMeta *meta, void *dst)
{
    uint32_t len = queue->entriesNr;
    uint32_t nextTail = (tail + meta->prop.remain + 1) % len;
    uint32_t offset = 0;
    bool nextTailFlip = (bool)((uint32_t)((tail + meta->prop.remain + 1) >= len) ^ (uint32_t)tailFlip);
    for (size_t i = tail; i != nextTail; i = (i + 1) % len) {
        struct BlockingQueueEntry *entry = &queue->entries[i];
        size_t cnt = meta->prop.size - offset < sizeof(entry->data) ?
                     meta->prop.size - offset :
                     sizeof(entry->data);
        (void)memcpy_s(dst + offset, cnt, entry->data, cnt);
        offset += cnt;
    }
    SetTail(queue->buffer, nextTail, nextTailFlip);
}

int BlockingDequeue(struct BlockingQueue *queue, void **dst, long timeoutUs)
{
    int ret = 0;
    if (queue == NULL || dst == NULL) {
        ret = 1;
        ERR("invalid null pointer\n");
        goto end;
    }
    if (queue->isProducer) {
        ret = 1;
        ERR("producer cannot dequeue\n");
        goto end;
    }
    RefQueue(queue);
    ret = LockQueue(queue);
    if (ret != 0) {
        ERR("lock queue for dequeue failed\n");
        goto deref;
    }
    uint32_t tail;
    bool tailFlip;
    ret = WaitEmpty(queue, &timeoutUs, &tail, &tailFlip);
    if (ret != 0) {
        if (ret != BLOCKING_QUEUE_INTERRUPTED) {
            ERR("wait queue none empty failed\n");
        }
        goto unlock;
    }
    union BlockingQueueEntryMeta meta;
    ret = GetMetadata(queue->entries, tail, &meta);
    if (ret != 0) {
        ERR("get metadata of blocking queue entries failed\n");
        goto unlock;
    }
    void *buf = NULL;
    ret = AllocateDequeueBuffer(&meta, &buf);
    if (ret != 0) {
        ERR("allocate dequeue buffer failed\n");
        goto unlock;
    }
    DequeueBlocks(queue, tail, tailFlip, &meta, buf);
    *dst = buf;

unlock:
    UnlockQueue(queue);
deref:
    DerefQueue(queue);
end:
    return ret;
}
