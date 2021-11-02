/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
 * iTrustee licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef _TEE_CLIENT_INNER_H_
#define _TEE_CLIENT_INNER_H_

#include <unistd.h>
#include <semaphore.h>
#include "tee_client_constants.h"
#include "tee_client_type.h"

#define IS_TEMP_MEM(paramType)                                                              \
    (((paramType) == TEEC_MEMREF_TEMP_INPUT) || ((paramType) == TEEC_MEMREF_TEMP_OUTPUT) || \
     ((paramType) == TEEC_MEMREF_TEMP_INOUT))

#define IS_PARTIAL_MEM(paramType)                                                        \
    (((paramType) == TEEC_MEMREF_WHOLE) || ((paramType) == TEEC_MEMREF_PARTIAL_INPUT) || \
     ((paramType) == TEEC_MEMREF_PARTIAL_OUTPUT) || ((paramType) == TEEC_MEMREF_PARTIAL_INOUT))

#define IS_VALUE_MEM(paramType) \
    (((paramType) == TEEC_VALUE_INPUT) || ((paramType) == TEEC_VALUE_OUTPUT) || ((paramType) == TEEC_VALUE_INOUT))


#define NUM_OF_SHAREMEM_BITMAP 8

#ifndef PAGE_SIZE
#define PAGE_SIZE getpagesize()
#endif

/* TEE GLOBAL CMD */
enum SVC_GLOBAL_CMD_ID {
    GLOBAL_CMD_ID_INVALID                   = 0x0,  /* Global Task invalid cmd ID */
    GLOBAL_CMD_ID_BOOT_ACK                  = 0x1,  /* Global Task boot ack */
    GLOBAL_CMD_ID_OPEN_SESSION              = 0x2,  /* Global Task open Session */
    GLOBAL_CMD_ID_CLOSE_SESSION             = 0x3,  /* Global Task close Session */
    GLOBAL_CMD_ID_LOAD_SECURE_APP           = 0x4,  /* Global Task load dyn ta */
    GLOBAL_CMD_ID_NEED_LOAD_APP             = 0x5,  /* Global Task judge if need load ta */
    GLOBAL_CMD_ID_REGISTER_AGENT            = 0x6,  /* Global Task register agent */
    GLOBAL_CMD_ID_UNREGISTER_AGENT          = 0x7,  /* Global Task unregister agent */
    GLOBAL_CMD_ID_REGISTER_NOTIFY_MEMORY    = 0x8,  /* Global Task register notify memory */
    GLOBAL_CMD_ID_UNREGISTER_NOTIFY_MEMORY  = 0x9,  /* Global Task unregister notify memory */
    GLOBAL_CMD_ID_INIT_CONTENT_PATH         = 0xa,  /* Global Task init content path */
    GLOBAL_CMD_ID_TERMINATE_CONTENT_PATH    = 0xb,  /* Global Task terminate content path */
    GLOBAL_CMD_ID_ALLOC_EXCEPTION_MEM       = 0xc,  /* Global Task alloc exception memory */
    GLOBAL_CMD_ID_TEE_TIME                  = 0xd,  /* Global Task get tee secure time */
    GLOBAL_CMD_ID_TEE_INFO                  = 0xe,  /* Global Task tlogcat get tee info */
    GLOBAL_CMD_ID_MAX,
};

typedef struct {
    int32_t fd;                    /* file descriptor */
    struct ListNode session_list;  /* session list  */
    struct ListNode shrd_mem_list; /* share memory list */
    struct {
        void *buffer;
        sem_t buffer_barrier;
    } share_buffer;
    uint8_t shm_bitmap[NUM_OF_SHAREMEM_BITMAP];
    struct ListNode c_node; /* context list node  */
    uint32_t ops_cnt;
    pthread_mutex_t sessionLock;
    pthread_mutex_t shrMemLock;
    pthread_mutex_t shrMemBitMapLock;
    bool callFromHidl; /* true:from hidl,false:from vendor */
} TEEC_ContextHidl;

typedef struct {
    void *buffer;              /* memory pointer */
    uint32_t size;             /* memory size */
    uint32_t flags;            /* memory flag, distinguish between input and output, range in #TEEC_SharedMemCtl */
    uint32_t ops_cnt;          /* memoty operation cnt */
    bool is_allocated;         /* memory allocated flag, distinguish between registered or distributed */
    struct ListNode head;      /* head of shared memory list */
    TEEC_ContextHidl *context; /* point to its own TEE environment */
    uint32_t offset;
} TEEC_SharedMemoryHidl;

typedef struct {
    const uint8_t *taPath;
    FILE *taFp;
} TaFileInfo;

void SetBit(uint32_t i, uint32_t byteMax, uint8_t *bitMap);
void ClearBit(uint32_t i, uint32_t byteMax, uint8_t *bitMap);
int32_t GetAndSetBit(uint8_t *bitMap, uint32_t byteMax);
int32_t GetAndCleartBit(uint8_t *bitMap, uint32_t byteMax);

#endif
