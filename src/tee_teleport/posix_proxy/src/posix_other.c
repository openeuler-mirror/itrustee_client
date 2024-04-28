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
#include <errno.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <posix_data_handler.h>
#include <securec.h>
#include <net/if.h>
#include <serialize.h>
#include <common.h>
#include <fd_list.h>

static long OtherPlaceHolder(struct PosixProxyParam *param)
{
    (void)param;
    return 0;
}

static long EventfdWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t count, flags;
    
    if (DeSerialize(param->argsCnt, param->args, param->argsSz,
        INTEGERTYPE, &count, INTEGERTYPE, &flags) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        goto end;
    }

    ret = eventfd(count, flags);
    if (ret == -1) {
        DBG("failed to call eventfd, ret: %d, err: %s\n", ret, strerror(errno));
    }

end:
    return ret;
}

static long EpollCreate1Work(struct PosixProxyParam *param)
{
    uint64_t flags;
    int ret = -1;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz,
        INTEGERTYPE, &flags) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        goto end;
    }

    ret = epoll_create1(flags);
    if (ret == -1) {
        DBG("failed to call epoll_create1, ret: %d, err: %s\n", ret, strerror(errno));
    }
end:
    return ret;
}

static long EpollCtlWork(struct PosixProxyParam *param)
{
    uint64_t fd, op, fd2;
    struct epoll_event *ev = NULL;
    int ret = -1;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz,
        INTEGERTYPE, &fd, INTEGERTYPE, &op, INTEGERTYPE, &fd2, POINTTYPE, &ev) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        goto end;
    }

    ret = epoll_ctl(fd, op, fd2, ev);
    if (ret != 0) {
        DBG("failed to call epoll_ctl, ret: %d, err: %s\n", ret, strerror(errno));
    }
end:
    return ret;
}

static long EpollPwaitWork(struct PosixProxyParam *param)
{
    uint64_t epollFd, eventCnt, to, sigsIsNull;
    struct epoll_event *ev = NULL;
    sigset_t *sigs = NULL;
    int ret = -1;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz,
        INTEGERTYPE, &epollFd, POINTTYPE, &ev, INTEGERTYPE, &eventCnt,
        INTEGERTYPE, &to, POINTTYPE, &sigs, INTEGERTYPE, &sigsIsNull) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        goto end;
    }

    ret = sigsIsNull == 1 ?
        epoll_pwait(epollFd, param->args, eventCnt, to, NULL) :
        epoll_pwait(epollFd, param->args, eventCnt, to, sigs);
    if (ret == -1) {
        DBG("failed to call epoll_pwait, ret: %d, err: %s\n", ret, strerror(errno));
    }
end:
    return ret;
}

static long SelectWork(struct PosixProxyParam *param)
{
    int ret = -1;
    fd_set *rfds = NULL;
    fd_set *wfds = NULL;
    fd_set *efds = NULL;
    struct timeval *tv = NULL;
    uint64_t n = 0;
    uint64_t tvSet = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz,
        INTEGERTYPE, &n, POINTTYPE, &rfds, POINTTYPE, &wfds,
        POINTTYPE, &efds, POINTTYPE, &tv, INTEGERTYPE, &tvSet) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    if (n >= UINT32_MAX) {
        ERR("select invalid n: %lu\n", n);
        errno = EINVAL;
        return -1;
    }
    if (tvSet == 0) {
        tv = NULL;
    }

    ret = select((int)n, rfds, wfds, efds, tv);
    if (ret < 0) {
        DBG("failed to call select, ret: %d, err: %s\n", ret, strerror(errno));
    }

    return ret;
}

static int InsertFirstPkg(struct FdList *fdList, int fd, atomic_ulong teeIndex,
                          void *data, size_t blkSz, size_t totalLen)
{
    int ret = 0;
    if (fdList == NULL || data == NULL || totalLen < blkSz) {
        ERR("bad parameters\n");
        return -EINVAL;
    }

    uint8_t *pkgTmpBuf = (uint8_t *)calloc(totalLen, sizeof(uint8_t));
    if (pkgTmpBuf == NULL) {
        ERR("has no enough memory for pkg\n");
        return -errno;
    }

    if (blkSz > 0) {
        ret = memcpy_s(pkgTmpBuf, blkSz, data, blkSz);
        if (ret != 0) {
            ERR("memcpy failed ret: %d\n", ret);
            free(pkgTmpBuf);
            return -EIO;
        }
    }

    ret = FdListPutPkg(fdList, fd, teeIndex, pkgTmpBuf, totalLen);
    if (ret != 0) {
        ERR("put pkg tmpBuf to list failed\n");
        free(pkgTmpBuf);
        return -EIO;
    }
    return 0;
}

static long PkgSendWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = 0, teeIndex = 0, totalLen = 0, offset = 0, blkSz = 0;
    uint8_t *data = NULL;
    struct FdList *fdList = (struct FdList *)param->ctx;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz,
        INTEGERTYPE, &fd, INTEGERTYPE, &teeIndex, INTEGERTYPE, &totalLen,
        POINTTYPE, &data, INTEGERTYPE, &offset, INTEGERTYPE, &blkSz) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    if (offset == 0) {
        ret = InsertFirstPkg(fdList, fd, teeIndex, data, blkSz, totalLen);
        if (ret != 0) {
            ERR("PkgSendWork first pkg put failed\n");
            errno = EIO;
            return -1;
        }
        return blkSz;
    }

    struct PkgTmpBuf *pkg = NULL;
    ret = FdListGetPkg(fdList, fd, teeIndex, &pkg);
    if (ret != 0) {
        ERR("failed to get pkg\n");
        errno = ENOENT;
        return -1;
    }

    if (totalLen != pkg->buffLen || offset > pkg->buffLen) {
        ERR("totalLen is not same as put or data expected exceed buf temp buf\n");
        errno = EINVAL;
        ret = -1;
        goto end;
    }

    if (pkg->buffLen - offset < blkSz) {
        ERR("pkg exceeded temp buff len\n");
        errno = ERANGE;
        ret = -1;
        goto end;
    }

    if (blkSz > 0) {
        (void)memcpy_s(pkg->buff + offset, blkSz, data, blkSz);
    }
    ret = blkSz;

end:
    FdListReleasePkg(fdList, pkg);
    return ret;
}

static long PkgRecvWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = 0, teeIndex = 0, totalLen = 0, offset = 0, blkSz = 0;
    uint8_t *retBuf = param->args;
    uint8_t *tmpBuf = NULL;
    struct FdList *fdList = (struct FdList *)param->ctx;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz,
        INTEGERTYPE, &fd, INTEGERTYPE, &teeIndex, INTEGERTYPE, &totalLen,
        POINTTYPE, &tmpBuf, INTEGERTYPE, &offset, INTEGERTYPE, &blkSz) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    struct PkgTmpBuf *pkg = NULL;
    ret = FdListGetPkg(fdList, fd, teeIndex, &pkg);
    if (ret != 0) {
        ERR("failed to get pkg\n");
        errno = ENOENT;
        return -1;
    }

    if (totalLen > pkg->buffLen || offset > UINT64_MAX - blkSz || offset + blkSz > pkg->buffLen) {
        ERR("totalLen is not same as put or data expected exceed buf temp buf\n");
        errno = EINVAL;
        ret = -1;
        goto end;
    }

    if (memcpy_s(retBuf, param->argsSz, pkg->buff + offset, blkSz) != 0) {
        ERR("memcpy_s failed for expected results\n");
        errno = ERANGE;
        ret = -1;
        goto end;
    }
    ret = blkSz;

end:
    FdListReleasePkg(fdList, pkg);
    return ret;
}

static long PkgTerminateWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = 0, teeIndex = 0;
    struct FdList *fdList = (struct FdList *)param->ctx;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz,
        INTEGERTYPE, &fd, INTEGERTYPE, &teeIndex) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    ret = FdListReleasePkgByIndex(fdList, fd, teeIndex);
    if (ret != 0) {
        ERR("remove pkg failed\n");
        errno = EIO;
        return -1;
    }

    return ret;
}

static long GetInterfaceList(int fd, unsigned long req, uint8_t *buf, uint64_t bufLen)
{
    long ret = -1;
    struct ifconf ifc = {0};
    int *len = NULL;

    if (DeSerialize(POSIX_CALL_ARG_COUNT_2, buf, bufLen,
        POINTTYPE, &len, POINTTYPE, &(ifc.ifc_buf)) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    if (len != NULL)
        ifc.ifc_len = *len;

    ret = ioctl(fd, req, &ifc);
    *len = ifc.ifc_len;
    return ret;
}

static long HandleInterfaceOpt(int fd, unsigned long req, struct ifreq *ifr)
{
    return ioctl(fd, req, ifr);
}

static long IoctlWork(struct PosixProxyParam *param)
{
    long ret = -1;
    uint64_t fd = 0, req = 0, bufLen = 0;
    uint8_t *buf = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz,
        INTEGERTYPE, &fd, INTEGERTYPE, &req, POINTTYPE, &buf, INTEGERTYPE, &bufLen) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    DBG("start to call ioctl, fd: %lu, req: %lu\n", fd, req);

    switch (req) {
        case SIOCGIFCONF:
            ret = GetInterfaceList((int)fd, (unsigned long)req, buf, bufLen);
            break;
        case SIOCGIFADDR:
        case SIOCGIFFLAGS:
        case SIOCGIFBRDADDR:
        case SIOCGIFNETMASK:
        case SIOCGIFINDEX:
        case SIOCGIFHWADDR:
            ret = HandleInterfaceOpt((int)fd, (unsigned long)req, (struct ifreq *)buf);
            break;
        case FIONBIO:
        case FIONREAD:
            ret = ioctl((int)fd, (unsigned long)req, (int *)buf);
            if (buf != NULL) {
                *(int *)param->args = *(int *)buf;
            }
            break;
        default:
            ERR("req %lu is invalid\n", req);
            ret = -1;
            errno = EINVAL;
            break;
    }

    DBG("end to call ioctl, fd: %lu, req: %lu, ret: %ld\n", fd, req, ret);

    return ret;
}

static long Poll(struct PosixProxyParam *param)
{
    int ret = 0;
    uint64_t n = 0, timeout = 0;
    struct pollfd *fds = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz,
        INTEGERTYPE, &n, INTEGERTYPE, &timeout, POINTTYPE, &fds) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    ret = poll(fds, (nfds_t)n, (int)timeout);
    if (ret >= 0 && (memmove_s((void *)param->args, param->argsSz, fds, sizeof(struct pollfd) * n) != EOK)) {
        ERR("memmove in poll failed\n");
        errno = EINVAL;
        return -1;
    }
    return ret;
}

static long GetrlimitWork(struct PosixProxyParam *param)
{
    int ret = 0;
    uint64_t resource = 0;
    uint8_t *rlim = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz,
        INTEGERTYPE, &resource, POINTTYPE, &rlim) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    ret = getrlimit(resource, (struct rlimit *)rlim);
    *(struct rlimit *)param->args = *(struct rlimit *)rlim;

    return ret;
}

static struct PosixFunc g_funcs[] = {
    POSIX_FUNC_ENUM(OTHER_PLACE_HOLDER, OtherPlaceHolder, 0),    
    POSIX_FUNC_ENUM(OTHER_EPOLL_CREATE1, EpollCreate1Work, 1),
    POSIX_FUNC_ENUM(OTHER_EPOLL_CTL, EpollCtlWork, 4),
    POSIX_FUNC_ENUM(OTHER_EPOLL_PWAIT, EpollPwaitWork, 6),
    POSIX_FUNC_ENUM(OTHER_EVENTFD, EventfdWork, 2),
    POSIX_FUNC_ENUM(OTHER_SELECT, SelectWork, 6),
    POSIX_FUNC_ENUM(OTHER_PKG_SEND, PkgSendWork, 6),
    POSIX_FUNC_ENUM(OTHER_PKG_RECV, PkgRecvWork, 6),
    POSIX_FUNC_ENUM(OTHER_PKG_TERMINATE, PkgTerminateWork, 2),
    POSIX_FUNC_ENUM(OTHER_IOCTL, IoctlWork, 4),
    POSIX_FUNC_ENUM(OTHER_POLL, Poll, 3),
    POSIX_FUNC_ENUM(OTHER_GETRLIMIT, GetrlimitWork, 2),
};

POSIX_FUNCS_IMPL(POSIX_OTHER, g_funcs)