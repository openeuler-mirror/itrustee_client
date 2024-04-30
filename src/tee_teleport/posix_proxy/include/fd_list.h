/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef TELEPORT_FD_LIST_H
#define TELEPORT_FD_LIST_H

#include <stdio.h>
#include <stdint.h>
#include <pthread.h> /* pthread_mutex_t */
#include <stdatomic.h> /* atomic_ulong */
#include <tee_client_list.h> /* struct ListNode */
#include <tee_client_inner.h> /* bitmap */

#define BITS_PER_BYTE 8

struct PkgTmpBuf {
    struct ListNode list;
    int fd;
    unsigned long teeIndex;
    void *buff;
    size_t buffLen;
    volatile atomic_ulong refCnt;
    unsigned long creatTimeUs;
};

struct FdList {
    uint8_t *bitmap; /* a bitmap mark fd that cannot be closed */
    uint32_t byteMax; /* max fd divide by BITS_PER_BYTE */
    pthread_mutex_t bitmapLock;
    struct ListNode list; /* fd list */
    pthread_mutex_t fdListLock;
};

int FdListPutFD(struct FdList *fdList, int fd);
int FdListCheckFD(struct FdList *fdList, int fd, bool *res);
int FdListRemoveFD(struct FdList *fdList, int fd);

/* success: 0, error: -1 */
int FdListPutPkg(struct FdList *fdList, int fd, unsigned long teeIndex, void *buff, size_t buffLen);

/* 
 * success: 0, error: -1 
 * get pkg successfully, add pkg's refcnt
 */
int FdListGetPkg(struct FdList *fdList, int fd, unsigned long teeIndex, struct PkgTmpBuf **retPkg);

/* 
 * success: 0, error: -1 
 * search pkg by index, and sub pkg's refcnt, and try to remove pkg from list
 * in order ro sub pkg's refcnt to 1 to free pkg memory, please call FdListReleasePkgByIndex twice
 */
int FdListReleasePkgByIndex(struct FdList *fdList, int fd, unsigned long teeIndex);

/* 
 * success: 0, error: -1 
 * direcrly sub pkg's refcnt, and try to remove pkg from list
 * in order ro sub pkg's refcnt to 1 to free pkg memory, please call FdListReleasePkgByIndex twice
 */
int FdListReleasePkg(struct FdList *fdList, struct PkgTmpBuf *pkg);

int FdListDelTimeoutPkg(struct FdList *fdList);

int FdListInit(struct FdList **retFdList);

int FdListDestroy(struct FdList *fdList);

#endif