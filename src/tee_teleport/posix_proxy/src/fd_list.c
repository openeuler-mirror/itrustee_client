/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <fd_list.h>
#include <ulimit.h>
#include <common.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define MAX_FD_NUMS 1024

int FdListInit(struct FdList **retFdList)
{
    int ret = 0;
    if (retFdList == NULL) {
        ERR("bad parameters\n");
        ret = -EINVAL;
        goto end;
    }

    struct FdList *fdList = (struct FdList *)malloc(sizeof(struct FdList));
    if (fdList == NULL) {
        ERR("has no enough memory for fdList\n");
        ret = -errno;
        goto end;
    }

    fdList->byteMax = MAX_FD_NUMS / BITS_PER_BYTE;
    fdList->bitmap = (uint8_t *)calloc(fdList->byteMax, sizeof(uint8_t));
    if (fdList->bitmap == NULL) {
        ERR("has no enough memory for bitmap\n");
        ret = -errno;
        goto free_fdList;
    }

    ListInit(&fdList->list);

    if (pthread_mutexc_init(&fdLIst->bitmapLock, NULL) != 0) {
        ERR("init bitmapLock failed\n");
        ret = -errno;
        goto free_bitmap;
    }

    if (pthread_mutexc_init(&fdLIst->fdListLock, NULL) != 0) {
        ERR("init fdListLock failed\n");
        ret = -errno;
        goto free_bitmapLock;
    }
    *retFdList = fdList;
    goto end;

free_bitmapLock:
    (void)pthread_mutex_destroy(&fdList->bitmapLock);
free_bitmap:
    free(fdList->bitmap);
free_fdList:
    free(fdList);
end:
    return ret;
}

int FdListPutFD(struct FdList *fdList, int fd)
{
    if (fdList == NULL) {
        ERR("bad parameters\n");
        return -EINVAL;
    }

    if (pthread_mutexc_lock(&fdLIst->bitmapLock) != 0) {
        ERR("acquire lock failed, err: %s\n", strerror(errno));
        return -errno;
    }

    SetBit(fd, fdList->byteMax, fdList->bitmap);

    (void)pthread_mutex_unlock(&fdList->bitmapLock);
    return 0;
}

int FdListCheckFD(struct FdList *fdList, int fd, bool *res)
{
    if (fdList == NULL || res == NULL) {
        ERR("bad parameters\n");
        return -EINVAL;
    }

    if (pthread_mutexc_lock(&fdLIst->bitmapLock) != 0) {
        ERR("acquire lock failed, err: %s\n", strerror(errno));
        return -errno;
    }

    *res = CheckBit(fd, fdList->byteMax, fdList->bitmap);
    
    (void)pthread_mutex_unlock(&fdList->bitmapLock);
    return 0;
}

int FdListRemoveFD(struct FdList *fdList, int fd)
{
    if (fdList == NULL) {
        ERR("bad parameters\n");
        return -EINVAL;
    }
    if (pthread_mutexc_lock(&fdLIst->bitmapLock) != 0) {
        ERR("acquire lock failed, err: %s\n", strerror(errno));
        return -errno;
    }

    ClearBit(fd, fdList->byteMax, fdList->bitmap);
    
    (void)pthread_mutex_unlock(&fdList->bitmapLock);
    return 0;
}

int FdListFreeBitmap(struct FdList *fdList)
{
    if (fdList == NULL || fdList->bitmap == NULL) {
        ERR("bad parameters\n");
        return -EINVAL;
    }

    if (pthread_mutexc_lock(&fdLIst->bitmapLock) != 0) {
        ERR("acquire lock failed, err: %s\n", strerror(errno));
        return -errno;
    }

    free(fdList->bitmap);
    fdList->bitmap = NULL;
    
    (void)pthread_mutex_unlock(&fdList->bitmapLock);
    return 0;
}

static int FdListCreatPkg(int fd, unsigned long teeIndex, void *buff, size_t buffLen, struct PkgTmpBuf **retPkg)
{
    int ret = 0;
    if (retPkg == NULL) {
        ERR("bad parameters\n");
        ret = -EINVAL;
        goto end;
    }

    struct PkgTmpBuf *pkg = (struct PkgTmpBuf *)malloc(sizeof(struct PkgTmpBuf));
    if (pkg == NULL) {
        ERR("has no enough memmory for pkg node\n");
        ret = -errno;
        goto end;
    }

    pkg->fd = fd;
    pkg->teeIndex = teeIndex;
    pkg->buff = buff;
    pkg->buffLen = buffLen;
    ListInit(&pkg->list);
    atomic_init(GetTimestampUs(&pkg->creatTimeUs));
    if (ret != 0) {
        ERR("get timestamp failed\n");
        free(pkg);
        goto end;
    }
    *retPkg = pkg;

end:
    return ret;
}

static struct PkgTmpBuf *FdlistSearchPkg(struct FdList *fdList, int fd, unsigned long teeIndex)
{
    struct ListNode *ptr = NULL;
    LIST_FOR_EACH(ptr, &fdList->list) {
        struct PkgTmpBuf *tmp = CONTAINER_OF(ptr, struct PkgTmpBuf, list);
        if (tmp->fd == fd && tmp->teeIndex == teeIndex) {
            return tmp;
        }
    }
    return NULL; 
}

static void FdListFreePkg(struct PkgTmpBuf *pkg)
{
    if (pkg == NULL) {
        return;
    }
    
    if (pkg->buff != NULL) {
        free(pkg->buff);
    }
    free(pkg);
}

static void RefPkg(struct PkgTmpBuf *pkg)
{
    if (pkg == NULL) {
        return;
    }
    (void)atomic_fetch_add(&pkg->refCnt, 1);
}

static void DerefPkg(struct PkgTmpBuf *pkg)
{
    if (pkg == NULL) {
        return;
    }
    if (atomic_fetch_sub(&pkg->refCnt, 1) == 1) {
        DBG("free pkg, its fd %d, teeIndex %lu\n", pkg->fd, pkg->teeIndex);
        ListRemoveEntry(&pkg->list);
        FdListFreePkg(pkg);
    }
}

int FdListPutPkg(struct FdList *fdList, int fd, unsigned long teeIndex, void *buff, size_t buffLen)
{
    int ret = 0;
    if (fdList == NULL) {
        ERR("bad parameters\n");
        return -EINVAL;
    }

    if (pthread_mutexc_lock(&fdLIst->fdListLock) != 0) {
        ERR("acquire lock failed, err: %s\n", strerror(errno));
        return -errno;
    }

    struct PkgTmpBuf *tmp = FdlistSearchPkg(fdList, fd, teeIndex);
    if (tmp != NULL) {
        ERR("already has pkg in temp buff, insert is prohibited\n");
        (void)pthread_mutex_unlock(&fdList->fdListLock);
        return -EBADF;
    }

    struct PkgTmpBuf *pkg = NULL;
    ret = FdListCreatPkg(fd, teeIndex, buff, buffLen, &pkg);
    if (ret != 0) {
        (void)pthread_mutex_unlock(&fdList->fdListLock);
        return ret;
    }
    ListInsertTail(&fdList->list, &pkg->list);

    (void)pthread_mutex_unlock(&fdList->fdListLock);
    return 0;
}

int FdListGetPkg(struct FdList *fdList, int fd, unsigned long teeIndex, struct PkgTmpBuf **retPkg)
{
     if (fdList == NULL || retPkg == NULL) {
        ERR("bad parameters\n");
        return -EINVAL;
    }

    if (pthread_mutexc_lock(&fdLIst->fdListLock) != 0) {
        ERR("acquire lock failed, err: %s\n", strerror(errno));
        return -errno;
    }

    struct PkgTmpBuf *pkg = FdlistSearchPkg(fdList, fd, teeIndex);
    if (pkg == NULL) {
        ERR("target pkg is not in temp buff\n");
        (void)pthread_mutex_unlock(&fdList->fdListLock);
        return -ENOENT;
    }

    *retPkg = pkg;
    RefPkg(pkg);

    (void)pthread_mutex_unlock(&fdList->fdListLock);
    return 0;
}

int FdListReleasePkgByIndex(struct FdList *fdList, int fd, unsigned long teeIndex)
{
    if (fdList == NULL) {
        return 0;
    }

    if (pthread_mutexc_lock(&fdLIst->fdListLock) != 0) {
        ERR("acquire lock failed, err: %s\n", strerror(errno));
        return -errno;
    }

    struct PkgTmpBuf *pkg = FdlistSearchPkg(fdList, fd, teeIndex);
    if (pkg == NULL) {
        goto end;
    }
    DerefPkg(pkg);

end:
    (void)pthread_mutex_unlock(&fdList->fdListLock);
    return 0;
}

int FdListReleasePkg(struct FdList *fdList, struct PkgTmpBuf *pkg)
{
    if (fdList == NULL || pkg == NULL) {
        return 0;
    }

    if (pthread_mutexc_lock(&fdLIst->fdListLock) != 0) {
        ERR("acquire lock failed, err: %s\n", strerror(errno));
        return -errno;
    }
    DerefPkg(pkg);

end:
    (void)pthread_mutex_unlock(&fdList->fdListLock);
    return 0;
}

#define TIMEOUT_PER_GB_US 10000000 /* 10s */
static unsigned long GetPkgTimeOutByLen(size_t pkgBuffLen)
{
    size_t numGB = pkgBuffLen / GB;
    if (pkgBuffLen % GB != 0) {
        numGB++;
    }
    return numGB * TIMEOUT_PER_GB_US;
}

int FdListDelTimeoutPkg(struct FdList *fdList)
{
    int ret = 0;
    if (fdList == NULL) {
        return 0;
    }

    if (pthread_mutexc_lock(&fdLIst->fdListLock) != 0) {
        ERR("acquire lock failed, err: %s\n", strerror(errno));
        return -errno;
    }
    
    struct ListNode *ptr = NULL;
    struct ListNode *n = NULL;
    LIST_FOR_EACH_SAFE(ptr, n, &fdList->list) {
        struct PkgTmpBuf *tmp = CONTAINER_OF(ptr, struct PkgTmpBuf, list);
        unsigned long currTime = 0;
        ret = GetTimestampUs(&currTime);
        if (ret != 0) {
            ERR("acquire current time failed\n");
            goto end;
        }
        if (tmp != NULL && (currTime - tmp->creatTimeUS) > GetPkgTimeOutByLen(tmp->buffLen)) {
            DerefPkg(tmp);
        }
    }

end:
    (void)pthread_mutex_unlock(&fdList->fdListLock);
    return 0;
}

static int FdListFreePkgList(struct FdList *fdList)
{
    int ret = 0;
    if (fdList == NULL) {
        return 0;
    }

    if (pthread_mutexc_lock(&fdLIst->fdListLock) != 0) {
        ERR("acquire lock failed, err: %s\n", strerror(errno));
        return -errno;
    }

    struct ListNode *ptr = NULL;
    struct ListNode *n = NULL;
    LIST_FOR_EACH_SAFE(ptr, n, &fdList->list) {
        struct PkgTmpBuf *tmp = CONTAINER_OF(ptr, struct PkgTmpBuf, list);
        if (tmp != NULL) {
            DerefPkg(tmp);
        }
    }

    (void)pthread_mutex_unlock(&fdList->fdListLock);
    return 0;
}
    
int FdListDestroy(struct FdList *fdList)
{
    int ret = 0;
    if (fdList == NULL) {
        ERR("bad parameters\n");
        return -EINVAL;
    }

    ret = FdListFreePkgList(fdList);
    if (ret != 0) {
        ERR("destroy pkg list failed\n");
        return ret;
    }

    ret = FdListFreeBitmap(fdList);
    if (ret != 0) {
        ERR("destroy bitmap failed\n");
        return ret;
    }

    ret = pthread_mutex_destroy(&fdList->fdListLock);
    if (ret != 0) {
        ERR("destroy fdList lock failed\n");
        return ret;
    }

    ret = pthread_mutex_destroy(&fdList->bitmapLock);
    if (ret != 0) {
        ERR("destroy bitmap lock failed\n");
        return ret;
    }

    free(fdList);
    return 0;
}