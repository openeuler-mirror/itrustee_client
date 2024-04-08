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

#include "scp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <libgen.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <securec.h>
#include <stdint.h>

#include "tee_client_type.h"
#include "tee_client_api.h"
#include "tee_sys_log.h"
#include "tc_ns_client.h"
#include "portal.h"
#include "dir.h"

#ifdef OPENSSL_SUPPORT
#include <openssl/sha.h>
#endif

#define DECIMAL 10
static int SendContainerMsg(const struct TeePortalContainerType *config, struct TeePortalType *portal, uint32_t mode)
{
    int ret = -1;
    struct TeePortalType *scp = (struct TeePortalType *)portal;

    scp->type = SEND_CONTAINER_MSG;
    scp->reeUID = getuid();
    scp->sessionID = 0;
    scp->args.containerMsg.mode = mode;
    scp->nsId = config->nsid;

    if (memcpy_s(scp->args.containerMsg.containerId, sizeof(scp->args.containerMsg.containerId),
                 config->containerid, CONTAINER_ID_LEN) != 0) {
        printf("strncpy_s failed\n");
        return -EFAULT;
    }
    ret = TriggerPortal();
    if (ret != 0)
        printf("trigger portal is not successful!\n");

    return ret;
}

static int CopyFileByBlk(const char *srcPath, unsigned char *content, struct TeePortalType *scp)
{
    int ret = -EFAULT;
    uint64_t fileSize = scp->args.transport.fileSize;
    uint64_t blks = scp->args.transport.blks;
    uint32_t bs = scp->args.transport.bs;
    FILE *fp = fopen(srcPath, "r");
    if (fp == NULL) {
        printf("open file %s failed: %s\n", srcPath, strerror(errno));
        return -EACCES;
    }

    /* read data to TEE */
    for (uint64_t i = 0; i < blks; i++) {
        uint32_t expectedReadSize;
        if (i < (blks - 1))
            expectedReadSize = bs;
        else
            expectedReadSize = (uint32_t)(fileSize - i * bs);

        (void)memset_s(content, bs, 0, bs);
        uint32_t readSize = (uint32_t)fread((void *)content, 1, expectedReadSize, fp);
        if (readSize != expectedReadSize) {
            printf("read file failed, read size/total size=%u/%u: %s\n", readSize, expectedReadSize, strerror(errno));
            goto out;
        }
        if (memcpy_s((void *)((char*)scp + sizeof(*scp)), bs, content, readSize) != EOK) {
            printf("memcpy_s failed\n");
            goto out;
        }
        scp->args.transport.nbr = i;
        scp->args.transport.srcDataSize = readSize;

        ret = TriggerPortal();
        if (ret != 0) {
            printf("trigger portal failed\n");
            goto out;
        }
        ret = scp->ret;
        if (ret != 0) {
            printf("push data to tee failed\n");
            goto out;
        }
    }
out:
    (void)fclose(fp);
    return ret;
}

static int SetCopyFileArgs(struct TeePortalType *scp, const char *filename, const char *dstPath)
{
    (void)memset_s(scp->args.transport.basename, sizeof(scp->args.transport.basename), 0,
        sizeof(scp->args.transport.basename));
    if (memcpy_s(scp->args.transport.basename, sizeof(scp->args.transport.basename), filename,
        strlen(filename)) != EOK) {
        printf("memcpy_s failed\n");
        return -EFAULT;
    }
    (void)memset_s(scp->args.transport.dstPath, sizeof(scp->args.transport.dstPath), 0,
        sizeof(scp->args.transport.dstPath));
    if (memcpy_s(scp->args.transport.dstPath, sizeof(scp->args.transport.dstPath), dstPath, strlen(dstPath)) != EOK) {
        printf("memcpy_s failed\n");
        return -EFAULT;
    }
    return 0;
}

static int CopyFile(const char *srcPath, const char *dstPath, const char *filename, void *portal, uint32_t portalSize)
{
    struct TeePortalType *scp = (struct TeePortalType *)portal;
    int ret = -EFAULT;
    uint32_t sessionID = scp->sessionID;
    /* get the file size */
    struct stat ss;
    if (lstat(srcPath, &ss) == -1) {
        printf("lstat %s failed: %s\n", srcPath, strerror(errno));
        return -EFAULT;
    }
    uint64_t fileSize = (uint64_t)ss.st_size;
    uint32_t bs = portalSize - (uint32_t)sizeof(struct TeePortalType);
    uint64_t blks = fileSize / bs;

    unsigned char *content = (unsigned char *)malloc(bs);
    if (content == NULL) {
        printf("alloc buffer for file failed, %u\n", bs);
        return -ENOMEM;
    }

    if (fileSize % bs != 0)
        blks++;

    (void)memset_s(scp, portalSize, 0, sizeof(struct TeePortalType));
    scp->type = INPUT;
    scp->reeUID = getuid();
    scp->sessionID = sessionID;
    scp->args.transport.fileSize = fileSize;
    scp->args.transport.blks = blks;
    scp->args.transport.bs = bs;
    if (SetCopyFileArgs(scp, filename, dstPath) != 0) {
        tloge("set args failed\n");
        goto out;
    }

    if (CopyFileByBlk(srcPath, content, scp) != 0) {
        tloge("copy file by blk failed\n");
        goto out;
    }
    ret = 0;
out:
    free(content);
    return ret;
}

static int PushFile(const char *srcPath, const char *dstPath, const char *filename, void *portal, uint32_t portalSize)
{
    struct stat ss;
    if (lstat(srcPath, &ss) == -1) {
        printf("lstat %s failed: %s\n", srcPath, strerror(errno));
        return -1;
    }
    if (S_ISREG(ss.st_mode)) {
        return CopyFile(srcPath, dstPath, filename, portal, portalSize);
    } else {
        printf("the source type %d is not supported\n", ss.st_mode);
        return -1;
    }
}

static int SetDstPath(const char *dstPath)
{
    char *dirc, *dname;
    int ret;
    char realPath[PATH_MAX] = { 0 };

    if (realpath(dstPath, realPath) != NULL) {
        printf("file %s exists, please clean\n", dstPath);
        return -EFAULT;
    }

    dirc = strdup(dstPath);
    if (dirc == NULL) {
        printf("failed to get dirc\n");
        return -1;
    }

    dname = dirname(dirc);

    ret = MkdirIteration(dname);
    if (ret != 0) {
        printf("failed to mkdir!\n");
        free(dirc);
        return -EFAULT;
    }

    free(dirc);
    return 0;
}

static int CopyFileFromTEEByBlk(struct TeePortalType *scp, char *srcPath, unsigned char *content,
                                uint32_t bs)
{
    uint64_t fileSize = scp->args.transport.fileSize;
    uint64_t blks = scp->args.transport.blks;
    int ret;
    int fd = open(srcPath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        printf("open file %s failed: %s\n", srcPath, strerror(errno));
        return -EACCES;
    }

    for (uint64_t i = 0; i < blks; i++) {
        uint32_t expectedWriteSize;
        if (i < (blks - 1))
            expectedWriteSize = bs;
        else
            expectedWriteSize = (uint32_t)(fileSize - i * bs);

        scp->type = GET;
        scp->args.transport.srcDataSize = expectedWriteSize;

        ret = TriggerPortal();
        if (ret != 0) {
            printf("trigger portal failed when copying the file\n");
            goto clear;
        }

        (void)memset_s(content, bs, 0, bs);
        if (memcpy_s(content, bs, (void *)((char*)scp + sizeof(*scp)), expectedWriteSize) != EOK) {
            printf("memcpy_s failed\n");
            ret = -EFAULT;
            goto clear;
        }

        ssize_t writtenSize = write(fd, content, expectedWriteSize);
        if (writtenSize != (ssize_t)expectedWriteSize) {
            printf("write file failed, size/total =%ld/%u: %s\n", (long)writtenSize, expectedWriteSize,
                strerror(errno));
            ret = -EFAULT;
            goto clear;
        }
        scp->args.transport.nbr++;
        printf("copy file block %llu\n", (unsigned long long)scp->args.transport.nbr);
    }
clear:
    close(fd);
    return ret;
}

static int CopyFileFromTEE(char *srcPath, void *portal, uint32_t portalSize)
{
    struct TeePortalType *scp = (struct TeePortalType *)portal;
    uint32_t sessionID = scp->sessionID;
    (void)memset_s(scp, portalSize, 0, sizeof(struct TeePortalType));

    scp->type = OUTPUT;
    scp->reeUID = getuid();
    scp->sessionID = sessionID;
    uint32_t bs = portalSize - (uint32_t)sizeof(struct TeePortalType);

    /* tell TEE the file to be pulled */
    int ret = -EFAULT;
    ret = TriggerPortal();
    if (ret != 0) {
        printf("trigger portal failed\n");
        return ret;
    }

    unsigned char *content = (unsigned char *)malloc(bs);
    if (content == NULL) {
        printf("alloc buffer for file failed, %u\n", bs);
        return -ENOMEM;
    }
    if (SetDstPath(srcPath) != 0) {
        free(content);
        return -EFAULT;
    }
    if (CopyFileFromTEEByBlk(scp, srcPath, content, bs) != 0) {
        free(content);
        return -EFAULT;
    }
    ret = scp->ret;
    if (ret != 0)
        printf("pull data from tee failed\n");
    free(content);
    return ret;
}

// srcPath is the path in ree, dstPath is the path in tee
static int PullFile(const char *srcPath, void *portal, uint32_t portalSize)
{
    int ret = 0;
    char fullReePath[PATH_MAX] = { 0 };

    if (srcPath[0] != '/') {
        if (getcwd(fullReePath, PATH_MAX) == NULL) {
            printf("cannot get current dir\n");
            return -EFAULT;
        }

        if (snprintf_s(fullReePath, PATH_MAX, PATH_MAX - 1, "%s/%s", fullReePath, srcPath) < 0) {
            printf("Copy file path failed\n");
            return -EFAULT;
        }
    } else {
        if (strcpy_s(fullReePath, PATH_MAX, srcPath) != EOK) {
            printf("failed to copy full ree path\n");
            return -EFAULT;
        }
    }

    ret = CopyFileFromTEE(fullReePath, portal, portalSize);
    if (ret != 0)
        printf("copy files from TEE failed\n");
    return ret;
}

// here srcPath is a file path or a dir path, dstPath is a dir path
int TeeScp(int mode, const char *srcPath, const char *dstPath, const char *filename, uint32_t sessionID)
{
    int ret = -1;
    uint32_t portalSize;
    struct TeePortalType *portal = NULL;

    if (srcPath == NULL || dstPath == NULL || filename == NULL)
        return -1;
    if (mode != INPUT && mode != OUTPUT)
        return -1;

    if (GetPortal((void**)&portal, &portalSize) != 0) {
        printf("get portal failed\n");
        return -EFAULT;
    }

    portal->reeUID = getuid();
    portal->sessionID = sessionID;
    if (mode == INPUT)
        ret = PushFile(srcPath, dstPath, filename, portal, portalSize);
    else if (mode == OUTPUT)
        ret = PullFile(srcPath, portal, portalSize);
    return ret;
}

int TeeInstall(int mode, const char *filename, uint32_t *sessionID)
{
    int ret = -1;
    uint32_t portalSize;
    struct TeePortalType *portal = NULL;

    if (mode < 0 || mode >= INV_INSTALL_UNINSTALL_TYPE || filename == NULL || sessionID == NULL)
        return -EINVAL;

    if (GetPortal((void**)&portal, &portalSize) != 0) {
        printf("get portal failed\n");
        return -EFAULT;
    }

    portal->type = INSTALL;
    portal->reeUID = getuid();
    portal->sessionID = *sessionID;
    portal->args.install.type = mode;
    (void)memset_s(portal->args.install.file, sizeof(portal->args.install.file), 0, sizeof(portal->args.install.file));
    if (memcpy_s(portal->args.install.file, sizeof(portal->args.install.file), filename, strlen(filename)) != EOK) {
        printf("memcpy_s failed\n");
        return -EFAULT;
    }
    ret = TriggerPortal();
    if (ret != 0)
        printf("trigger portal failed\n");

    *sessionID = portal->sessionID;

    if (portal->ret != 0)
        ret = portal->ret;
    return ret;
}

int TeeUninstall(int mode)
{
    int ret = -1;
    uint32_t portalSize;
    struct TeePortalType *portal = NULL;

    if (mode != PYTHON_INTERPRETER && mode != JAVA_RUNTIME)
        return -EINVAL;

    if (GetPortal((void**)&portal, &portalSize) != 0) {
        printf("get portal failed\n");
        return -EFAULT;
    }

    portal->type = UNINSTALL;
    portal->reeUID = getuid();
    portal->sessionID = 0;
    portal->args.install.type = mode;
    ret = TriggerPortal();
    if (ret != 0)
        printf("trigger portal failed\n");
    if (portal->ret != 0)
        ret = portal->ret;

    return ret;
}

static bool CheckThirdLibParams(uint32_t offset, uint32_t portalSize, uint32_t numOfFiles, char *libNameStart)
{
    if (offset > (portalSize - (uint32_t)sizeof(struct TeePortalType))) {
        return false;
    }
    
    uint32_t cnt = 0;
    char *p = libNameStart;
    while (p) {
        if (*p == '\0' && *(p + 1) != '\0') {
            cnt++;
        } else if (*p == '\0' && *(p + 1) == '\0') {
            cnt++;
            break;
        }
        p++;
    }

    if (cnt != numOfFiles)
        return false;
    
    return true;
}

static int32_t ShowThirdLib(struct TeePortalType *portal, uint32_t portalSize)
{
    uint32_t numOfFiles = portal->args.list.numOfFiles;
    if (portal->args.list.offset == 0 || numOfFiles == 0) {
        printf("No any third-party lib found!\n");
        return 0;
    }

    uint32_t offset = 0;
    char *libNameStart = (char *)portal + sizeof(struct TeePortalType);
    if (!CheckThirdLibParams(portal->args.list.offset, portalSize, numOfFiles, libNameStart)) {
        printf("bad params!\n");
        return -EFAULT;
    }

    char *allLibNames = (char *)malloc(portal->args.list.offset);
    if (allLibNames == NULL) {
        return -ENOMEM;
    }

    if (memcpy_s(allLibNames, portal->args.list.offset, libNameStart, portal->args.list.offset) != EOK) {
        printf("cannot get all lib name path\n");
        free(allLibNames);
        return -EFAULT;
    }

    for (uint32_t i = 0; i < numOfFiles; i++) {
        char *name = strdup(allLibNames + offset);
        if (name == NULL) {
            printf("failed to copy lib name!\n");
            free(allLibNames);
            return -EFAULT;
        }
        offset += (uint32_t)(strlen(name) + 1);

        printf("%s\n", name);
        free(name);
    }

    free(allLibNames);
    return 0;
}

int TeeList(int mode)
{
    int ret = -1;
    uint32_t portalSize;
    struct TeePortalType *portal = NULL;

    if (mode != PYTHON_INTERPRETER && mode != JAVA_RUNTIME) {
        return -EINVAL;
    }

    if (GetPortal((void**)&portal, &portalSize) != 0) {
        printf("get portal failed\n");
        return -EFAULT;
    }

    portal->type = LIST;
    portal->reeUID = getuid();
    portal->sessionID = 0;
    portal->args.install.type = mode;
    portal->args.list.offset = 0;
    portal->args.list.numOfFiles = 0;
    ret = TriggerPortal();
    if (ret != 0)
        printf("trigger portal failed\n");
    if (portal->ret != 0) {
        ret = portal->ret;
        return ret;
    }

    ret = ShowThirdLib(portal, portalSize);
    return ret;
}

int TeeClean(int grpId)
{
    int ret = -1;
    uint32_t portalSize;
    struct TeePortalType *portal = NULL;

    if (grpId < 0) {
        printf("illegal group id!\n");
        return ret;
    }

    if (GetPortal((void**)&portal, &portalSize) != 0) {
        printf("get portal failed\n");
        return -EFAULT;
    }

    portal->type = CLEAN;
    portal->reeUID = getuid();
    portal->sessionID = 0;
    portal->args.clean.grpId = grpId;
    printf("In TeeClean, grpid is %d\n", grpId);
    ret = TriggerPortal();
    if (ret != 0) {
        printf("trigger portal failed\n");
        return ret;
    }

    if (portal->ret != 0) {
        ret = portal->ret;
    }

    return ret;
}

int TeeRconfig(struct TeePortalRConfigType *config, long nsid)
{
    int ret = -1;
    if (config == NULL) {
        printf("Unexpected null pointer for resource limit!\n");
        return ret;
    }

    uint32_t portalSize;
    struct TeePortalType *portal = NULL;

    if (GetPortal((void**)&portal, &portalSize) != 0) {
        printf("get portal failed\n");
        return -EFAULT;
    }

    portal->type = RCONFIG;
    portal->reeUID = getuid();
    portal->sessionID = 0;
    portal->nsId = (uint32_t)nsid;
    portal->args.rconfig.vmid = config->vmid;

    if (strcpy_s(portal->args.rconfig.cpus, PARAM_LEN, config->cpus) != 0) {
        printf("failed to copy mem size!\n");
        return -EFAULT;
    }

    if (strcpy_s(portal->args.rconfig.memSize, PARAM_LEN, config->memSize) != 0) {
        printf("failed to copy mem size!\n");
        return -EFAULT;
    }

    if (strcpy_s(portal->args.rconfig.diskSize, PARAM_LEN, config->diskSize) != 0) {
        printf("failed to copy disk mem size!\n");
        return -EFAULT;
    }

    if (strcpy_s(portal->args.rconfig.cpuset, PARAM_LEN, config->cpuset) != 0) {
        printf("failed to copy cpuset size!\n");
        return -EFAULT;
    }

    ret = TriggerPortal();
    if (ret != 0) {
        printf("trigger portal failed\n");
        return -1;
    }
    printf("online cpuset: %s\n", portal->args.rconfig.onlineCpus);

    /* portal->ret is group id for ree container */
    return portal->ret;
}


int TeeSendContainerMsg(const struct TeePortalContainerType *config, uint32_t mode)
{
    if (config == NULL) {
        printf("send container msg failed\n");
        return -EINVAL;
    }
    int ret = 0;
    uint32_t portalSize;
    struct TeePortalType *portal = NULL;

    if (GetPortal((void**)&portal, &portalSize) != 0) {
        printf("get portal failed\n");
        return -EFAULT;
    }

    if (SendContainerMsg(config, portal, mode) != 0) {
        printf("The send container msg execution is not successful!\n");
        return -EFAULT;
    }

    if (portal->ret != 0) {
        printf("The execution result of the portal is %d!\n", portal->ret);
        ret = portal->ret;
    }
    return ret;
}

int TeeDestroy(uint32_t sessionID)
{
    int ret = -1;
    uint32_t portalSize;
    struct TeePortalType *portal = NULL;

    if (GetPortal((void**)&portal, &portalSize) != 0) {
        printf("get portal failed\n");
        return -EFAULT;
    }

    portal->type = DESTROY;
    portal->sessionID = sessionID;
    portal->reeUID = getuid();
    ret = TriggerPortal();
    if (ret != 0) {
        printf("trigger portal failed\n");
    }
    if (portal->ret != 0) {
        ret = portal->ret;
    }
    return ret;
}

int TeeDelete(uint32_t sessionID)
{
    int ret = -1;
    uint32_t portalSize;
    struct TeePortalType *portal = NULL;
 
    if (GetPortal((void**)&portal, &portalSize) != 0) {
        printf("get portal failed\n");
        return -EFAULT;
    }
 
    portal->type = DELETE;
    portal->reeUID = getuid();
    portal->sessionID = sessionID;
    
    ret = TriggerPortal();
    if (ret != 0)
        printf("trigger portal failed\n");
    if (portal->ret != 0)
        ret = portal->ret;
    return ret;
}
