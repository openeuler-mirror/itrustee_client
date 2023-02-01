/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2012-2022. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "tee_ca_daemon.h"
#include <unistd.h>
#include <errno.h> /* for errno */
#include <fcntl.h>
#include <sys/ioctl.h> /* for ioctl */
#include <sys/mman.h>  /* for mmap */
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include "securec.h"
#include "tc_ns_client.h"
#include "tee_client_type.h"
#include "tee_client_socket.h"
#include "tee_agent.h"
#include "tee_log.h"
#include "tee_auth_common.h"
#include "tee_ca_auth.h"
#include "system_ca_auth.h"

/* debug switch */
#ifdef LOG_NDEBUG
#undef LOG_NDEBUG
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "teecd"

#define IOV_LEN 1

static unsigned int g_version = 0;

static int InitMsg(struct msghdr *hmsg, struct iovec *iov, size_t iovLen,
                   char *ctrlBuf, size_t ctrlBufLen)
{
    if (hmsg == NULL || iov == NULL || ctrlBuf == NULL) {
        return EINVAL;
    }
    hmsg->msg_name       = NULL;
    hmsg->msg_namelen    = 0;
    hmsg->msg_iov        = iov;
    hmsg->msg_iovlen     = iovLen;
    hmsg->msg_control    = ctrlBuf;
    hmsg->msg_controllen = ctrlBufLen;
    return EOK;
}

static int SendFileDescriptor(int cmd, int socket, int fd)
{
    struct msghdr hmsg;
    struct iovec iov[IOV_LEN];
    char ctrlBuf[CMSG_SPACE(sizeof(int))];
    char base[IOV_LEN];
    int *cmdata = NULL;

    errno_t ret = memset_s(&hmsg, sizeof(hmsg), 0, sizeof(hmsg));
    if (ret != EOK) {
        tloge("memset failed!\n");
        return ret;
    }

    ret = memset_s(ctrlBuf, CMSG_SPACE(sizeof(int)), 0, CMSG_SPACE(sizeof(int)));
    if (ret != EOK) {
        tloge("memset failed!\n");
        return ret;
    }
    /* Pass at least one byte data, recvmsg() will not return 0 */
    base[0]         = ' ';
    iov[0].iov_base = base;
    iov[0].iov_len  = sizeof(base);

    ret = InitMsg(&hmsg, iov, IOV_LEN, ctrlBuf, CMSG_SPACE(sizeof(int)));
    if (ret != EOK) {
        tloge("init msg failed!\n");
        return ret;
    }

    struct cmsghdr *controlMsg = CMSG_FIRSTHDR(&hmsg);
    if (controlMsg != NULL) {
        controlMsg->cmsg_level          = SOL_SOCKET;
        controlMsg->cmsg_type           = SCM_RIGHTS;
        controlMsg->cmsg_len            = CMSG_LEN(sizeof(int));
        cmdata = (int *)(uintptr_t)CMSG_DATA(controlMsg);
        *cmdata = fd;
    }

    if (cmd == GET_TEEVERSION) {
        iov[0].iov_base        = &fd;
        iov[0].iov_len         = sizeof(int);
        hmsg.msg_control       = NULL;
        hmsg.msg_controllen    = 0;
    }

    ret = (int)sendmsg(socket, &hmsg, 0);
    if (ret <= 0) {
        tloge("sendmsg failed ret=0x%x:%d.\n", ret, errno);
        return -1;
    }
    return 0;
}

static int ProcessCaMsg(const struct ucred *cr, const CaRevMsg *caInfo, int socket)
{
    int ret;

    if (caInfo->cmd == GET_TEEVERSION) {
        ret = SendFileDescriptor(caInfo->cmd, socket, (int)g_version);
        if (ret != 0) {
            tloge("Failed to send version back. ret = %d\n", ret);
            return -1;
        }
        return 0;
    }

    int fd = open(TC_NS_CLIENT_DEV_NAME, O_RDWR);
    if (fd == -1) {
        tloge("Failed to open %s: %d\n", TC_NS_CLIENT_DEV_NAME, errno);
        return -1;
    }

    ret = SendLoginInfo(cr, caInfo, fd);
    if (ret != EOK) {
        tloge("Failed to send login info. ret=%d\n", ret);
        close(fd);
        return -1;
    }

    ret = SendFileDescriptor(caInfo->cmd, socket, fd);
    if (ret != EOK) {
        tloge("Failed to send fd. ret=%d\n", ret);
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static void ProcessAccept(int s, CaRevMsg *caInfo)
{
    struct ucred cr;
    struct sockaddr_un remote;
    int ret;

    while (1) {
        tlogd("Waiting for a connection...target daemon\n");
        size_t t = sizeof(remote);
        int s2   = accept(s, (struct sockaddr *)&remote, (socklen_t *)&t);
        if (s2 == -1) {
            tloge("accept() to server socket failed, errno=%d", errno);
            continue;
        }

        socklen_t len = sizeof(struct ucred);
        if (getsockopt(s2, SOL_SOCKET, SO_PEERCRED, &cr, &len) < 0) {
            tloge("peercred failed: %d", errno);
            close(s2);
            continue;
        }

        tlogd("uid %d pid %d\n", cr.uid, cr.pid);

        ret = RecvCaMsg(s2, caInfo);
        if (ret != 0) {
            tloge("tee ca daemon recvmsg failed\n");
            goto CLOSE_SOCKET;
        }

        TrySyncSysTimeToSecure();

        ret = ProcessCaMsg(&cr, caInfo, s2);
        if (ret != 0) {
            tloge("Failed to process ca msg. ret=%d\n", ret);
            goto CLOSE_SOCKET;
        }

    CLOSE_SOCKET:
        tlogd("close_socket and curret ret=%u\n", ret);
        close(s2);
        errno_t rc = memset_s(caInfo, sizeof(CaRevMsg), 0, sizeof(CaRevMsg));
        if (rc != EOK) {
            tloge("ca_info memset_s failed\n");
        }
    }
}

static int FormatSockAddr(struct sockaddr_un *local, socklen_t *len)
{
    int ret = strncpy_s(local->sun_path, sizeof(local->sun_path), TC_NS_SOCKET_NAME, sizeof(TC_NS_SOCKET_NAME));
    if (ret != EOK) {
        tloge("strncpy_s failed\n");
        return ret;
    }

    local->sun_family = AF_UNIX;
    *len              = (socklen_t)(strlen(local->sun_path) + sizeof(local->sun_family));
    /* Make the socket in the Abstract Domain(no path but everyone can connect) */
    local->sun_path[0] = 0;

    return 0;
}

int GetTEEVersion(void)
{
    int ret;

    int fd = open(TC_TEECD_PRIVATE_DEV_NAME, O_RDWR);
    if (fd == -1) {
        tloge("Failed to open %s: %d\n", TC_TEECD_PRIVATE_DEV_NAME, errno);
        return -1;
    }

    ret = ioctl(fd, TC_NS_CLIENT_IOCTL_GET_TEE_VERSION, &g_version);
    close(fd);
    if (ret != 0) {
        tloge("Failed to get tee version, err=%d\n", ret);
        return -1;
    }

    return ret;
}

static int32_t CreateSocket(void)
{
    int32_t ret;

    /* Open a socket (a UNIX domain stream socket) */
    int32_t s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) {
        tloge("can't open stream socket, errno=%d\n", errno);
        return -1;
    }

    /* Fill in address structure and bind to socket */
    struct sockaddr_un local;
    socklen_t len;
    ret = memset_s(&local, sizeof(local), 0, sizeof(local));
    if (ret != EOK) {
        tloge("memset_s sockaddr_un local failed!\n");
        close(s);
        return -1;
    }

    ret = FormatSockAddr(&local, &len);
    if (ret != EOK) {
        tloge("format sock addr failed\n");
        close(s);
        return -1;
    }

    if (bind(s, (struct sockaddr *)&local, len) < 0) {
        tloge("bind() to server socket failed, errno=%d\n", errno);
        close(s);
        return -1;
    }

    return s;
}

void *CaServerWorkThread(void *dummy)
{
    (void)dummy;
    CaRevMsg *caInfo = NULL;

    int32_t s = CreateSocket();
    if (s < 0) {
        return NULL;
    }

    /* Start listening on the socket */
    if (listen(s, BACKLOG_LEN) < 0) {
        tloge("listen() failed, errno=%d\n", errno);
        goto CLOSE_EXIT;
    }

    tlogv("\n********* deamon successfully initialized!***\n");

    caInfo = (CaRevMsg *)malloc(sizeof(CaRevMsg));
    if (caInfo == NULL) {
        tloge("ca server: Failed to malloc caInfo\n");
        goto CLOSE_EXIT;
    }

    if (memset_s(caInfo, sizeof(CaRevMsg), 0, sizeof(CaRevMsg)) != EOK) {
        tloge("ca_info memset_s failed\n");
        free(caInfo);
        goto CLOSE_EXIT;
    }

    ProcessAccept(s, caInfo);
    free(caInfo);

    tlogv("\n********* deamon process_accept over!***\n");

CLOSE_EXIT:
    close(s);
    return NULL;
}
