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
#define _GNU_SOURCE
#include <posix_data_handler.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <string.h>
#include <securec.h>
#include "serialize.h"
#include "common.h"
#include "fd_list.h"

static long NetSocketWork(struct PosixProxyParam *param)
{
    uint64_t domain = 0;
    uint64_t type = 0;
    uint64_t protocol = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &domain,
                    INTEGERTYPE, &type, INTEGERTYPE, &protocol) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    return socket(domain, type, protocol);
}

static long NetConnectWork(struct PosixProxyParam *param)
{
    uint64_t fd = 0;
    uint64_t len = 0;
    uint8_t *addr = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd,
                    POINTTYPE, &addr, INTEGERTYPE, &len) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    return connect(fd, (struct sockaddr *)addr, len);
}

static long NetBindWork(struct PosixProxyParam *param)
{
    uint64_t fd = 0;
    uint64_t len = 0;
    uint8_t *addr = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd,
                    POINTTYPE, &addr, INTEGERTYPE, &len) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    return bind(fd, (struct sockaddr *)addr, len);
}

static long NetListenWork(struct PosixProxyParam *param)
{
    uint64_t fd = 0;
    uint64_t backlog = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, INTEGERTYPE, &backlog) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    return listen(fd, backlog);
}

static long DispatchCmd(int fd, struct sockaddr *addr, socklen_t *len, int func)
{
    int ret = -1;
    switch (func) {
        case NET_ACCEPT:
            ret = accept(fd, addr, len);
            break;
        case NET_GETSOCKNAME:
            ret = getsockname(fd, addr, len);
            break;
        case NET_GETPEERNAME:
            ret = getpeername(fd, addr, len);
            break;
        default:
            ERR("invalid command\n");
            break;
    }
    return ret;
}

static long CommonOps(struct PosixProxyParam *param, int func)
{
    int ret = -1;
    uint64_t fd = 0;
    socklen_t *len = 0;
    uint8_t *buf = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, POINTTYPE,
                    &buf, POINTTYPE, &len) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return ret;
    }

    return DispatchCmd((int)fd, (struct sockaddr *)buf, len, func);
}

static long NetAcceptWork(struct PosixProxyParam *param)
{
    return CommonOps(param, NET_ACCEPT);
}

static long NetAccept4Work(struct PosixProxyParam *param)
{
    uint64_t fd = 0;
    socklen_t *len = 0;
    uint64_t flg = 0;
    uint8_t *buf = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, POINTTYPE, &buf,
                    POINTTYPE, &len, INTEGERTYPE, &flg) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    return accept4(fd, (struct sockaddr *)buf, len, flg);
}

static long NetShutdownWork(struct PosixProxyParam *param)
{
    uint64_t fd = 0;
    uint64_t how = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd,
                    INTEGERTYPE, &how) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    return shutdown(fd, how);
}

static long NetGetsocknameWork(struct PosixProxyParam *param)
{
    return CommonOps(param, NET_GETSOCKNAME);
}

static long NetGetsockoptWork(struct PosixProxyParam *param)
{
    uint64_t fd = 0;
    uint64_t level = 0;
    uint64_t optname = 0;
    uint8_t *optval = NULL;
    socklen_t *optlen = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, INTEGERTYPE, &level,
                    INTEGERTYPE, &optname, POINTTYPE, &optval, POINTTYPE, &optlen) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    return getsockopt(fd, level, optname, optval, optlen);
}

static long NetSetsockoptWork(struct PosixProxyParam *param)
{
    uint64_t fd = 0;
    uint64_t level = 0;
    uint64_t optname = 0;
    uint8_t *optval = NULL;
    uint64_t optlen = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, INTEGERTYPE, &level,
                    INTEGERTYPE, &optname, POINTTYPE, &optval, INTEGERTYPE, &optlen) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    return setsockopt(fd, level, optname, optval, optlen);
}

static long NetGetpeernameWork(struct PosixProxyParam *param)
{
    return CommonOps(param, NET_GETPEERNAME);
}

static long NetSendtoWork(struct PosixProxyParam *param)
{
    ssize_t ret = -1;
    uint64_t fd = 0;
    uint8_t *buf = NULL;
    uint64_t len = 0;
    uint64_t flags = 0;
    uint8_t *addr = NULL;
    uint64_t alen = 0;
    uint64_t teeIndex = 0;
    struct PkgTmpBuf *pkg = NULL;
    struct FdList *fdList = (struct FdList *)param->ctx;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, POINTTYPE, &buf,
                    INTEGERTYPE, &len, INTEGERTYPE, &flags, POINTTYPE, &addr, INTEGERTYPE, &alen,
                    INTEGERTYPE, &teeIndex) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    if (teeIndex > 0) {
        ret = FdListGetPkg(fdList, fd, teeIndex, &pkg);
        if (ret != 0) {
            ERR("failed to get pkg from temp buf\n");
            errno = EIO;
            return -1;
        }
        if (len != pkg->buffLen) {
            ERR("write count is not the same as pkg buf Len\n");
            errno = EINVAL;
            ret = -1;
            goto end;
        }
        buf = pkg->buff;
    }

    ret = sendto(fd, buf, len, flags, (struct sockaddr *)addr, alen);
end:
    /* sub pkg's ref-cnt */
    (void)FdListReleasePkg(fdList, pkg);
    /* to ensure pkg memory release, as soon as the write is completed, regardless of whether successful or not */
    (void)FdListReleasePkg(fdList, pkg);
    return ret;
}

static long NetRecvfromWork(struct PosixProxyParam *param)
{
    ssize_t ret = -1;
    uint64_t fd = 0;
    uint8_t *buf = NULL;
    uint64_t len = 0;
    uint64_t flags = 0;
    uint8_t *addr = NULL;
    socklen_t *alen = 0;
    uint64_t teeIndex = 0;
    struct FdList *fdList = (struct FdList *)param->ctx;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, POINTTYPE, &buf,
                    INTEGERTYPE, &len, INTEGERTYPE, &flags, POINTTYPE, &addr, POINTTYPE, &alen,
                    INTEGERTYPE, &teeIndex) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    if (teeIndex > 0) {
        /* buf will be freed when clearing pkg */
        buf = (uint8_t *)malloc(len);
        if (buf == NULL) {
            ERR("failed to alloc mem for pkg buf\n");
            return -1;
        }
        /* Before the actual read ops, the purpose of putting pkg now is to avoid read's rollback due to put failure */
        if (FdListPutPkg(fdList, fd, teeIndex, buf, len) != 0) {
            ERR("put pkg to tmp buff failed\n");
            free(buf);
            errno = EIO;
            return -1;
        }
    }

    ret = recvfrom(fd, buf, len, flags, (struct sockaddr *)addr, alen);
    /*
     * Read failed, clear pkg inserted by fd and tee_index. Buf will be freed here.
     * if ret == 0 and teeIndex > 0, we also release pkg here.
     **/
    if (ret <= 0 && teeIndex > 0)
        (void)FdListReleasePkgByIndex(fdList, fd, teeIndex);

    return ret;
}

#define SENDMSG_CMD 0
#define RECVMSG_CMD 1

static bool CheckIovlenInvalid(uint64_t msgIovlen, uint32_t cmd)
{
    if (cmd != SENDMSG_CMD || msgIovlen == 0) {
        return false;
    }

    size_t len = sizeof(struct iovec);
    uint64_t result = msgIovlen * len;
    return (result / msgIovlen) != len || (result / len) != msgIovlen || result < msgIovlen || result < len;
}

/* Parse struct msghdr from buf */
static long ConstructMsg(uint8_t *buf, uint64_t buf_len, struct msghdr **msg, uint32_t cmd,
                         uint8_t **ioBuf, size_t *ioBufLen)
{
    uint8_t *msgName = NULL;
    uint64_t msgNamelen = 0;
    uint8_t *iovecBuf = NULL;
    uint64_t msgIovlen = 0;
    uint8_t *msgControl = NULL;
    uint64_t msgControllen = 0;
    uint64_t msgFlags = 0;

    if (DeSerialize(POSIX_CALL_ARG_COUNT_7, buf, buf_len, POINTTYPE, &msgName, INTEGERTYPE, &msgNamelen,
                    POINTTYPE, &iovecBuf, INTEGERTYPE, &msgIovlen, POINTTYPE, &msgControl, INTEGERTYPE,
                    &msgControllen, INTEGERTYPE, &msgFlags) != 0) {
        ERR("deserialize for msghdr failed\n");
        return -1;
    }

    /* check (sizeof(struct iovec) * msgIovlen)  */
    if (CheckIovlenInvalid(msgIovlen, cmd)) {
        ERR("check iov len failed\n");
        return -1;
    }

    struct msghdr *hdr = NULL;
    hdr = (struct msghdr *)malloc(sizeof(struct msghdr));
    if (hdr == NULL) {
        ERR("malloc buf for msghdr failed\n");
        return -1;
    }

    hdr->msg_iov = (struct iovec *)iovecBuf;
    size_t tempPos = 0;
    if (cmd == SENDMSG_CMD) {
        tempPos = sizeof(struct iovec) * msgIovlen;
        for (uint32_t i = 0; i < msgIovlen; i++) {
            hdr->msg_iov[i].iov_base = iovecBuf + tempPos;
            tempPos += hdr->msg_iov[i].iov_len;
        }
    } else {
        size_t totalLen = 0;
        for (uint32_t i = 0; i < msgIovlen; i++) {
            totalLen += hdr->msg_iov[i].iov_len;
        }
        uint8_t *newBuf = malloc(totalLen);
        if (newBuf == NULL) {
            ERR("malloc mem for iovec buf failed\n");
            free(hdr);
            return -1;
        }
        for (uint32_t i = 0; i < msgIovlen; i++) {
            hdr->msg_iov[i].iov_base = newBuf + tempPos;
            tempPos += hdr->msg_iov[i].iov_len;
        }
        *ioBuf = newBuf;
        *ioBufLen = totalLen;
    }

    hdr->msg_name = msgName;
    hdr->msg_namelen = msgNamelen;
    hdr->msg_iovlen = msgIovlen;
    hdr->msg_control = msgControl;
    hdr->msg_controllen = msgControllen;
    hdr->msg_flags = msgFlags;
    *msg = hdr;
    return 0;
}

static void DestroyMsg(struct msghdr *hdr)
{
    if (hdr != NULL) {
        free(hdr);
        hdr = NULL;
    }
}

static long NetSendmsgWork(struct PosixProxyParam *param)
{
    ssize_t ret = -1;
    uint64_t fd = 0;
    uint8_t *buf = NULL;
    struct msghdr *msg = NULL;
    uint64_t buf_len = 0;
    uint64_t flags = 0;
    uint64_t teeIndex = 0;
    struct PkgTmpBuf *pkg = NULL;
    struct FdList *fdList = (struct FdList *)param->ctx;

    ret = DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, POINTTYPE, &buf,
                    INTEGERTYPE, &buf_len, INTEGERTYPE, &flags, INTEGERTYPE, &teeIndex);
    if (ret != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    if (teeIndex > 0) {
        ret = FdListGetPkg(fdList, fd, teeIndex, &pkg);
        if (ret != 0) {
            ERR("failed to get pkg from temp buf\n");
            errno = EIO;
            return -1;
        }
        if (buf_len != pkg->buffLen) {
            ERR("sendmsg buf_len is not the same as pkg temp buf len\n");
            errno = EINVAL;
            ret = -1;
            goto end;
        }
        buf = pkg->buff;
    }

    ret = ConstructMsg(buf, buf_len, &msg, SENDMSG_CMD, NULL, NULL);
    if (ret != 0) {
        ERR("init msg failed\n");
        errno = EINVAL;
        ret = -1;
        goto end;
    }

    ret = sendmsg(fd, msg, flags);
    DestroyMsg(msg);
end:
    /* sub pkg's ref-cnt */
    (void)FdListReleasePkg(fdList, pkg);
    /* to ensure pkg memory release, as soon as the write is completed, regardless of whether successful or not */
    (void)FdListReleasePkg(fdList, pkg);
    return ret;
}

/* serialize the contents of msg to a piece of continuous memory */
static int SerializeMsgBuf(const struct msghdr *msg, void *buf, uint32_t bufLen)
{
    int ret = -1;
    uint32_t i = 0;
    uint32_t iovecSize = 0;
    iovecSize = msg->msg_iovlen * sizeof(struct iovec);

    uint8_t *iovecBuf = (uint8_t *)malloc(iovecSize);
    if (iovecBuf == NULL) {
        ERR("alloc iovec buf failed\n");
        return -1;
    }
    (void)memset_s(iovecBuf, iovecSize, 0, iovecSize);

    struct iovec *tmpBuf = (struct iovec *)iovecBuf;
    for (i = 0; i < msg->msg_iovlen; i++)
        tmpBuf[i].iov_len = msg->msg_iov[i].iov_len;

    size_t msgSize = 0;
    ret = CalculateBuffSize(&msgSize, POSIX_CALL_ARG_COUNT_7, POINTTYPE, msg->msg_name,
        (size_t)(msg->msg_namelen), INTEGERTYPE, (uint64_t)msg->msg_namelen, POINTTYPE, iovecBuf,
        (size_t)iovecSize, INTEGERTYPE, (uint64_t)msg->msg_iovlen, POINTTYPE, msg->msg_control,
        (size_t)(msg->msg_controllen), INTEGERTYPE, (uint64_t)msg->msg_controllen,
        INTEGERTYPE, (uint64_t)msg->msg_flags);
    if (ret != 0) {
        ERR("calculate serialize buf size failed\n");
        goto end;
    }

    uint8_t *msgBuf = (uint8_t *)malloc(msgSize);
    if (msgBuf == NULL) {
        ERR("alloc msg buf failed\n");
        ret = -1;
        goto end;
    }
    (void)memset_s(msgBuf, msgSize, 0, msgSize);

    ret = Serialize(POSIX_CALL_ARG_COUNT_7, msgBuf, msgSize, POINTTYPE, msg->msg_name, (size_t)(msg->msg_namelen),
        INTEGERTYPE, (uint64_t)msg->msg_namelen, POINTTYPE, iovecBuf, (size_t)(iovecSize), INTEGERTYPE,
        (uint64_t)msg->msg_iovlen, POINTTYPE, msg->msg_control, (size_t)(msg->msg_controllen), INTEGERTYPE,
        (uint64_t)msg->msg_controllen, INTEGERTYPE, (uint64_t)msg->msg_flags);
    if (ret < 0) {
        ERR("serialize msg buf failed\n");
        goto clear_msg;
    }

    if (memcpy_s(buf, bufLen, msgBuf, msgSize) != EOK) {
        ERR("memcpy msg buf failed\n");
        ret = -1;
    }

clear_msg:
    free(msgBuf);
end:
    free(iovecBuf);
    return ret;
}

static long NetRecvmsgWork(struct PosixProxyParam *param)
{
    ssize_t ret = -1;
    uint64_t fd = 0;
    uint8_t *buf = NULL;
    struct msghdr *msg = NULL;
    uint64_t bufLen = 0;
    uint64_t flags = 0;
    uint64_t teeIndex = 0;
    struct FdList *fdList = (struct FdList *)param->ctx;

    ret = DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, POINTTYPE, &buf,
                      INTEGERTYPE, &bufLen, INTEGERTYPE, &flags, INTEGERTYPE, &teeIndex);
    if (ret != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    
    /* ioBuf will be freed when clearing pkg */
    uint8_t *ioBuf = NULL;
    size_t ioBufLen = 0;
    ret = ConstructMsg(buf, bufLen, &msg, RECVMSG_CMD, &ioBuf, &ioBufLen);
    if (ret != 0) {
        ERR("init msg failed\n");
        errno = EINVAL;
        return -1;
    }

    /* Before the actual read ops, the purpose of putting pkg now is to avoid read's rollback due to put failure */
    if (FdListPutPkg(fdList, fd, teeIndex, ioBuf, ioBufLen) != 0) {
        ERR("put pkg to temp buff failed\n");
        errno = EIO;
        DestroyMsg(msg);
        free(ioBuf);
        return -1;
    }

    ret = recvmsg(fd, msg, flags);
    /*
     * Read failed, clear pkg inserted by fd and tee_index. Iobuf will be freed here.
     * If ret == 0, we also release package here.
     **/
    if (ret <= 0)
        (void)FdListReleasePkgByIndex(fdList, fd, teeIndex);

    if (ret >= 0 && SerializeMsgBuf(msg, buf, bufLen) != 0) {
        ERR("serialize msg failed\n");
        errno = EIO;
        DestroyMsg(msg);
        free(ioBuf);
        return -1;
    }
    DestroyMsg(msg);
    return ret;
}

static long NetResInit(struct PosixProxyParam *param)
{
    (void)param;
    return res_init();
}


static long NetGetAddrInfo(struct PosixProxyParam *param)
{
    int ret = -1;
    char *host = NULL;
    char *serv = NULL;
    struct addrinfo *hint = NULL;
    struct addrinfo *temp_result = NULL;
    uint64_t host_len = 0;
    uint64_t serv_len = 0;
    uint64_t hint_len = 0;

    ret = DeSerialize(param->argsCnt, param->args, param->argsSz,
                      POINTTYPE, &host, INTEGERTYPE, &host_len,
                      POINTTYPE, &serv, INTEGERTYPE, &serv_len,
                      POINTTYPE, &hint, INTEGERTYPE, &hint_len);
    if (ret != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    temp_result = NULL;
    ret = getaddrinfo(host_len == 0 ? NULL : host, serv_len == 0 ? NULL : serv,
                      hint_len == 0 ? NULL : hint, &temp_result);
    if (ret != 0) {
        ERR("getaddrinfo failed (%d)\n", ret);
        return ret;
    }

    *(uint64_t *)(param->args) = (uint64_t)(uintptr_t)temp_result;

    return 0;
}

static long NetGetAddrInfoDoFetch(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t *addr_info_handle = NULL;
    struct addrinfo *empty_addrinfo = NULL;
    struct sockaddr *empty_ai_addr = NULL;
    char *empty_canonname = NULL;

    /*
     * addr_info_handle, input for current ptr, output for next ptr
     * empty_addrinfo, output for current addrinfo
     * empty_ai_addr, output for current ai_addr
     * empty_canonname, output for current canonname
     */
    ret = DeSerialize(param->argsCnt, param->args, param->argsSz,
                       POINTTYPE, &addr_info_handle,
                       POINTTYPE, &empty_addrinfo,
                       POINTTYPE, &empty_ai_addr,
                       POINTTYPE, &empty_canonname);
    if (ret != 0 || addr_info_handle == NULL || empty_addrinfo == NULL ||
        empty_ai_addr == NULL || empty_canonname == NULL) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    if (*addr_info_handle == 0)
        return 0;
    struct addrinfo *next = ((struct addrinfo *)(uintptr_t)(*addr_info_handle))->ai_next;
    struct sockaddr *ai_addr = ((struct addrinfo *)(uintptr_t)(*addr_info_handle))->ai_addr;
    char *ai_canonname = ((struct addrinfo *)(uintptr_t)(*addr_info_handle))->ai_canonname;
    uint64_t ai_canonname_size = ai_canonname == NULL ? 0 : strnlen(ai_canonname, NI_MAXHOST) + 1;
    if (ai_canonname_size > NI_MAXHOST) {
        ERR("Canonname is Invalid\n");
        errno = EINVAL;
        return -1;
    }

    if (memcpy_s(empty_addrinfo, sizeof(struct addrinfo), (void *)(uintptr_t)(*addr_info_handle),
        sizeof(struct addrinfo)) != EOK) {
        ERR("Copy addrinfo failed\n");
        errno = EINVAL;
        return -1;
    }

    if (ai_addr != NULL &&
        memcpy_s(empty_ai_addr, sizeof(struct sockaddr), ai_addr, sizeof(struct sockaddr)) != EOK) {
        ERR("Copy ai_addr failed\n");
        errno = EINVAL;
        return -1;
    }

    if (ai_canonname != NULL &&
        memcpy_s(empty_canonname, NI_MAXHOST, ai_canonname, ai_canonname_size) != EOK) {
        ERR("Copy canonname failed\n");
        errno = EINVAL;
        return -1;
    }

    *addr_info_handle = (uint64_t)(uintptr_t)next;
    return 0;
}

static long NetFreeAddrInfo(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t addr_info_handle = 0;
    ret = DeSerialize(param->argsCnt, param->args, param->argsSz,
                      INTEGERTYPE, &addr_info_handle);
    if (ret != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    if (addr_info_handle != 0)
        freeaddrinfo((struct addrinfo *)(uintptr_t)addr_info_handle);

    return 0;
}

static struct PosixFunc g_funcs[] = {
    /* enum network-related posix calls here */
    POSIX_FUNC_ENUM(NET_SOCKET, NetSocketWork, 3),
    POSIX_FUNC_ENUM(NET_CONNECT, NetConnectWork, 3),
    POSIX_FUNC_ENUM(NET_BIND, NetBindWork, 3),
    POSIX_FUNC_ENUM(NET_LISTEN, NetListenWork, 2),
    POSIX_FUNC_ENUM(NET_ACCEPT, NetAcceptWork, 3),
    POSIX_FUNC_ENUM(NET_ACCEPT4, NetAccept4Work, 4),
    POSIX_FUNC_ENUM(NET_SHUTDOWN, NetShutdownWork, 2),
    POSIX_FUNC_ENUM(NET_GETSOCKNAME, NetGetsocknameWork, 3),
    POSIX_FUNC_ENUM(NET_GETSOCKOPT, NetGetsockoptWork, 5),
    POSIX_FUNC_ENUM(NET_SETSOCKOPT, NetSetsockoptWork, 5),
    POSIX_FUNC_ENUM(NET_GETPEERNAME, NetGetpeernameWork, 3),
    POSIX_FUNC_ENUM(NET_SENDTO, NetSendtoWork, 7),
    POSIX_FUNC_ENUM(NET_RECVFROM, NetRecvfromWork, 7),
    POSIX_FUNC_ENUM(NET_SENDMSG, NetSendmsgWork, 5),
    POSIX_FUNC_ENUM(NET_RECVMSG, NetRecvmsgWork, 5),
    POSIX_FUNC_ENUM(NET_GETADDRINFO, NetGetAddrInfo, 6),
    POSIX_FUNC_ENUM(NET_GETADDRINFO_DOFETCH, NetGetAddrInfoDoFetch, 4),
    POSIX_FUNC_ENUM(NET_FREEADDRINFO, NetFreeAddrInfo, 1),
    POSIX_FUNC_ENUM(NET_RES_INIT, NetResInit, 0),
};

POSIX_FUNCS_IMPL(POSIX_NETWORK, g_funcs)
