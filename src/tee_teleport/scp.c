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

static int CalcFileMD(const char *srcPath, unsigned char *md, uint32_t msize)
{
#ifdef OPENSSL_SUPPORT
    (void)msize;
    struct stat ss;
    if (lstat(srcPath, &ss) == -1) {
        printf("lstat %s failed: %s\n", srcPath, strerror(errno));
        return -EACCES;
    }
    unsigned char *content = (unsigned char *)malloc(ss.st_size);
    if (content == NULL) {
        printf("alloc buffer for file failed, %lu\n", ss.st_size);
        return -ENOMEM;
    }

    /* read file */
    FILE *fp = fopen(srcPath, "r");
    if (fp == NULL) {
        printf("open file %s failed: %s\n", srcPath, strerror(errno));
        free(content);
        return -EACCES;
    }

    uint32_t readSize = (uint32_t)fread((void *)content, 1, ss.st_size, fp);
    if (readSize != ss.st_size) {
        printf("read file failed, read size/total size=%u/%lu: %s\n", readSize, ss.st_size, strerror(errno));
        free(content);
        fclose(fp);
        return -EACCES;
    }
    /* calculate the md */
    SHA256(content, readSize, md);
    free(content);
    (void)fclose(fp);
#else
    (void)srcPath;
    (void)md;
    (void)msize;
#endif
    return 0;
}

// file existence and file type are in portal->args.query
static int FileExist(const char *fullPath, void *portal, unsigned char *md, uint32_t msize, uint32_t sessionID)
{
    int ret = -1;
    struct TeePortalType *scp = (struct TeePortalType *)portal;
    char filename[PATH_MAX] = { 0 };

    if (md == NULL || msize == 0) {
        scp->args.query.checkMD = false;
    } else {
        scp->args.query.checkMD = true;
        (void)memset_s(scp->args.query.md, sizeof(scp->args.query.md), 0, sizeof(scp->args.query.md));
        if (memcpy_s(scp->args.query.md, sizeof(scp->args.query.md), md, msize) != EOK) {
            printf("memcpy_s failed\n");
            return -EFAULT;
        }
    }

    scp->type = QUERY;
    scp->reeUID = getuid();
    scp->sessionID = sessionID;
    (void)memset_s(filename, PATH_MAX, 0, PATH_MAX);
    if (memcpy_s(filename, PATH_MAX, fullPath, strlen(fullPath)) != EOK) {
        printf("memcpy_s failed\n");
        return -EFAULT;
    }

    (void)memset_s(scp->args.query.basename, sizeof(scp->args.query.basename), 0, sizeof(scp->args.query.basename));
    if (memcpy_s(scp->args.query.basename, sizeof(scp->args.query.basename), basename(filename),
        strlen(basename(filename))) != EOK) {
        printf("memcpy_s failed\n");
        return -EFAULT;
    }
    (void)memset_s(scp->args.query.dstPath, sizeof(scp->args.query.dstPath), 0, sizeof(scp->args.query.dstPath));
    if (memcpy_s(scp->args.query.dstPath, sizeof(scp->args.query.dstPath), fullPath, strlen(fullPath)) != EOK) {
        printf("memcpy_s failed\n");
        return -EFAULT;
    }
    ret = TriggerPortal();
    if (ret != 0) {
        printf("trigger portal failed\n");
        return ret;
    }

    return ret;
}

static int CheckInputFileExist(const char *srcPath, const char *dstPath, const char *filename,
                               struct TeePortalType *scp, unsigned char md[SHA256_DIGEST_LENGTH])
{
    /* check the file exists */
    char fullPath[PATH_MAX] = { 0 };
    if (CalcFileMD(srcPath, md, SHA256_DIGEST_LENGTH) != 0) {
        printf("cannot calculate message digest\n");
        return -EINVAL;
    }

    if (snprintf_s(fullPath, PATH_MAX, PATH_MAX - 1, "%s/%s", dstPath, filename) < 0) {
        printf("snprintf_s failed\n");
        return -EFAULT;
    }

    if (FileExist(fullPath, scp, md, SHA256_DIGEST_LENGTH, scp->sessionID) != 0) {
        printf("cannot check file existence\n");
        return -EINVAL;
    }

    if (scp->args.query.exist) {
        printf("file %s exists, please delete it before push\n", scp->args.query.dstPath);
        return -EINVAL;
    }
    return 0;
}

static int CopyFileByBlk(const char *srcPath, unsigned char *content, struct TeePortalType *scp)
{
    int ret = -EFAULT;
    uint32_t fileSize = scp->args.transport.fileSize;
    uint32_t blks = scp->args.transport.blks;
    uint32_t bs = scp->args.transport.bs;
    FILE *fp = fopen(srcPath, "r");
    if (fp == NULL) {
        printf("open file %s failed: %s\n", srcPath, strerror(errno));
        return -EACCES;
    }

    /* read data to TEE */
    for (uint32_t i = 0; i < blks; i++) {
        uint32_t expectedReadSize;
        if (i < (blks - 1))
            expectedReadSize = bs;
        else
            expectedReadSize = fileSize - i * bs;

        (void)memset_s(content, bs, 0, bs);
        uint32_t readSize = (uint32_t)fread((void *)content, 1, expectedReadSize, fp);
        if (readSize != expectedReadSize) {
            printf("read file failed, read size/total size=%u/%u: %s\n", readSize, expectedReadSize, strerror(errno));
            goto out;
        }
        if (memcpy_s((void *)((char *)scp + sizeof(*scp)), bs, content, readSize) != EOK) {
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

static int SetCopyFileArgs(struct TeePortalType *scp, unsigned char md[SHA256_DIGEST_LENGTH], const char *filename,
                           const char *dstPath)
{
    (void)memset_s(scp->args.transport.md, sizeof(scp->args.transport.md), 0, sizeof(scp->args.transport.md));
    if (memcpy_s(scp->args.transport.md, sizeof(scp->args.transport.md), md, SHA256_DIGEST_LENGTH) != EOK) {
        printf("memcpy_s failed\n");
        return -EFAULT;
    }
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
    unsigned char md[SHA256_DIGEST_LENGTH] = { 0 };
    int ret = -EFAULT;
    uint32_t sessionID = scp->sessionID;
    if (CheckInputFileExist(srcPath, dstPath, filename, scp, md) != 0)
        return -EFAULT;
    /* get the file size */
    struct stat ss;
    if (lstat(srcPath, &ss) == -1) {
        printf("lstat %s failed: %s\n", srcPath, strerror(errno));
        return -EFAULT;
    }
    uint32_t fileSize = (uint32_t)ss.st_size;
    uint32_t bs = portalSize - (uint32_t)sizeof(struct TeePortalType);
    uint32_t blks = fileSize / bs;

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
    if (SetCopyFileArgs(scp, md, filename, dstPath) != 0) {
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

static const char *g_rootDir;

static int CopyFiles(const char *srcPath, const char *dstPath, void *portal, uint32_t portalSize);

static int CopyFilesInDir(char *p, const char *srcPath, const char *dstPath, void *portal, uint32_t portalSize)
{
    int ret = 0;
    struct stat ss;
    char dstDir[PATH_MAX];
    if (stat(p, &ss) < 0) {
        printf("invalid path %s:%s\n", p, strerror(errno));
        return -EFAULT;
    }

    if (S_ISREG(ss.st_mode)) {
        if (strcmp(g_rootDir, srcPath))
            ret = snprintf_s(dstDir, sizeof(dstDir), sizeof(dstDir) - 1, "%s/%s", dstPath,
                srcPath + strlen(g_rootDir) + 1);
        else
            ret = snprintf_s(dstDir, sizeof(dstDir), sizeof(dstDir) - 1, "%s", dstPath);
        if (ret == -1) {
            printf("copy dstDir failed\n");
            return -EFAULT;
        }

        ret = CopyFile(p, dstDir, basename(p), portal, portalSize);
        if (ret == 0) {
            printf("copy file %s successful\n", p);
        } else {
            printf("copy file %s failed\n", p);
        }
    } else if (S_ISDIR(ss.st_mode)) {
        return CopyFiles(p, dstPath, portal, portalSize);
    } else {
        printf("%s is unknown mode\n", p);
        ret = -EFAULT;
    }
    return ret;
}

static int CopyFiles(const char *srcPath, const char *dstPath, void *portal, uint32_t portalSize)
{
    DIR *d = NULL;
    struct dirent *dp = NULL;
    struct stat ss;
    char p[PATH_MAX];
    int ret = 0;

    if (stat(srcPath, &ss) < 0 || !S_ISDIR(ss.st_mode)) {
        printf("invalid path %s\n", srcPath);
        return -EACCES;
    }

    if (!(d = opendir(srcPath))) {
        printf("opendir %s failed\n", srcPath);
        return -EACCES;
    }

    while ((dp = readdir(d)) != NULL) {
        if ((!strcmp(dp->d_name, ".")) || (!strcmp(dp->d_name, "..")))
            continue;

        (void)memset_s(p, sizeof(p), 0, sizeof(p));
        if (snprintf_s(p, sizeof(p), sizeof(p) - 1, "%s/%s", srcPath, dp->d_name) < 0) {
            printf("copy dname failed\n");
            ret = -EFAULT;
            goto out;
        }
        if (CopyFilesInDir(p, srcPath, dstPath, portal, portalSize) != 0) {
            ret = -EACCES;
            goto out;
        }
    }
out:
    closedir(d);
    return ret;
}

static int PushFiles(const char *srcPath, const char *dstPath, const char *filename, void *portal, uint32_t portalSize)
{
    struct stat ss;
    if (lstat(srcPath, &ss) == -1) {
        printf("lstat %s failed: %s\n", srcPath, strerror(errno));
        return -1;
    }
    if (S_ISREG(ss.st_mode)) {
        return CopyFile(srcPath, dstPath, filename, portal, portalSize);
    } else if (S_ISDIR(ss.st_mode)) {
        g_rootDir = srcPath;
        return CopyFiles(srcPath, dstPath, portal, portalSize);
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
    uint32_t fileSize = scp->args.transport.fileSize;
    uint32_t blks = scp->args.transport.blks;
    int ret;
    int fd = open(srcPath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        printf("open file %s failed: %s\n", srcPath, strerror(errno));
        return -EACCES;
    }

    for (uint32_t i = 0; i < blks; i++) {
        uint32_t expectedWriteSize;
        if (i < (blks - 1))
            expectedWriteSize = bs;
        else
            expectedWriteSize = fileSize - i * bs;

        scp->type = GET;
        scp->args.transport.srcDataSize = expectedWriteSize;

        ret = TriggerPortal();
        if (ret != 0) {
            printf("trigger portal failed when copying the file\n");
            goto clear;
        }

        (void)memset_s(content, bs, 0, bs);
        if (memcpy_s(content, bs, (void *)((char *)scp + sizeof(*scp)), expectedWriteSize) != EOK) {
            printf("memcpy_s failed\n");
            ret = -EFAULT;
            goto clear;
        }

        ssize_t writtenSize = write(fd, content, expectedWriteSize);
        if (writtenSize != expectedWriteSize) {
            printf("write file failed, size/total = %ld/%u: %s\n", writtenSize, expectedWriteSize, strerror(errno));
            ret = -EFAULT;
            goto clear;
        }
        scp->args.transport.nbr++;
        printf("copy file block %d\n", scp->args.transport.nbr);
    }
clear:
    close(fd);
    return ret;
}

static int CopyFileFromTEE(char *srcPath, const char *dstPath, void *portal, uint32_t portalSize)
{
    struct TeePortalType *scp = (struct TeePortalType *)portal;
    uint32_t sessionID = scp->sessionID;
    (void)memset_s(scp, portalSize, 0, sizeof(struct TeePortalType));

    scp->type = OUTPUT;
    scp->reeUID = getuid();
    scp->sessionID = sessionID;
    uint32_t bs = portalSize - (uint32_t)sizeof(struct TeePortalType);

    (void)memset_s(scp->args.transport.basename, sizeof(scp->args.transport.basename), 0,
        sizeof(scp->args.transport.basename));
    if (memcpy_s(scp->args.transport.basename, sizeof(scp->args.transport.basename), basename(srcPath),
        strlen(basename(srcPath))) != EOK) {
        printf("memcpy_s failed\n");
        return -EFAULT;
    }
    (void)memset_s(scp->args.transport.dstPath, sizeof(scp->args.transport.dstPath), 0,
        sizeof(scp->args.transport.dstPath));
    if (memcpy_s(scp->args.transport.dstPath, sizeof(scp->args.transport.dstPath), dstPath, strlen(dstPath)) != EOK) {
        printf("memcpy_s failed\n");
        return -EFAULT;
    }

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

static int GetNewFilePath(const char *oldPath, const char *oldRoot, const char *newRoot, char *out)
{
    if (strlen(oldRoot) == 0) {
        printf("bad file path\n");
        return -EINVAL;
    }

    if (strncmp(oldPath, oldRoot, strlen(oldRoot)) != 0) {
        printf("path names do not match\n");
        return -EINVAL;
    }

    if (snprintf_s(out, PATH_MAX, PATH_MAX - 1, "%s/%s", newRoot, oldPath + strlen(oldRoot)) < 0) {
        printf("cannot generate new path name\n");
        return -EFAULT;
    }
    return 0;
}

static int CopyEveryFileInDir(const char *srcPath, char *dstPath, struct TeePortalType *scp, uint32_t portalSize)
{
    uint32_t numOfFiles = scp->args.list.numOfFiles;
    uint32_t offset = 0;
    char *needPullFile = (char *)scp + sizeof(struct TeePortalType);
    char *allFilePath = (char *)malloc((unsigned long)scp->args.list.offset);
    if (allFilePath == NULL)
        return -ENOMEM;

    if (memcpy_s(allFilePath, (unsigned long)scp->args.list.offset,
        needPullFile, (unsigned long)scp->args.list.offset) != EOK) {
        printf("cannot get all file path\n");
        free(allFilePath);
        return -EFAULT;
    }

    for (uint32_t i = 0; i < numOfFiles; i++) {
        char *filePath = strdup(allFilePath + offset);
        char newFilePath[PATH_MAX] = { 0 };
        if (filePath == NULL) {
            printf("failed to copy filepath!\n");
            free(allFilePath);
            return -1;
        }

        offset += (uint32_t)(strlen(filePath) + 1);

        if (GetNewFilePath(filePath, dstPath, srcPath, newFilePath) != 0) {
            printf("cannot get new file path\n");
            free(allFilePath);
            free(filePath);
            return -1;
        }

        if (CopyFileFromTEE(newFilePath, filePath, scp, portalSize) != 0) {
            printf("copy file from TEE failed\n");
            free(allFilePath);
            free(filePath);
            return -EFAULT;
        }
        free(filePath);
    }
    free(allFilePath);
    return 0;
}

static int CopyDirFromTee(const char *srcPath, char *dstPath, void *portal, uint32_t portalSize)
{
    struct TeePortalType *scp = (struct TeePortalType *)portal;
    uint32_t sessionID = scp->sessionID;
    (void)memset_s(scp, portalSize, 0, sizeof(struct TeePortalType));
    scp->type = OUTPUT;
    scp->reeUID = getuid();
    scp->sessionID = sessionID;
    scp->args.list.numOfFiles = 0;

    (void)memset_s(scp->args.transport.basename, sizeof(scp->args.transport.basename), 0,
        sizeof(scp->args.transport.basename));
    if (memcpy_s(scp->args.transport.basename, sizeof(scp->args.transport.basename), basename(dstPath),
        strlen(basename(dstPath))) != EOK) {
        printf("memcpy_s failed\n");
        return -EFAULT;
    }
    (void)memset_s(scp->args.transport.dstPath, sizeof(scp->args.transport.dstPath), 0,
        sizeof(scp->args.transport.dstPath));
    if (memcpy_s(scp->args.transport.dstPath, sizeof(scp->args.transport.dstPath), dstPath, strlen(dstPath)) != EOK) {
        printf("memcpy_s failed\n");
        return -EFAULT;
    }

    int ret = TriggerPortal();
    if (ret != 0) {
        printf("pull trigger portal failed\n");
        return ret;
    }

    return CopyEveryFileInDir(srcPath, dstPath, scp, portalSize);
}

// srcPath is the path in ree, dstPath is the path in tee
static int PullFiles(const char *srcPath, const char *dstPath, const char *filename, void *portal, uint32_t portalSize)
{
    (void)filename;
    struct TeePortalType *scp = (struct TeePortalType *)portal;
    int ret = 0;
    char fullReePath[PATH_MAX] = { 0 };
    char teePath[PATH_MAX] = { 0 };

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

    if (FileExist(dstPath, scp, NULL, 0, scp->sessionID) != 0 || scp->args.query.exist == false) {
        printf("cannot find file!\n");
        return -EFAULT;
    }

    if (snprintf_s(teePath, PATH_MAX, PATH_MAX - 1, "%s", dstPath) < 0) {
        printf("snprintf_s failed\n");
        return -EFAULT;
    }

    if (scp->args.query.isDir) {
        ret = CopyDirFromTee(fullReePath, teePath, portal, portalSize);
    } else {
        ret = CopyFileFromTEE(fullReePath, teePath, portal, portalSize);
    }

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

    if (GetPortal((void **)&portal, &portalSize) != 0) {
        printf("get portal failed\n");
        return -EFAULT;
    }

    portal->reeUID = getuid();
    portal->sessionID = sessionID;
    if (mode == INPUT)
        ret = PushFiles(srcPath, dstPath, filename, portal, portalSize);
    else if (mode == OUTPUT)
        ret = PullFiles(srcPath, dstPath, filename, portal, portalSize);
    return ret;
}

int TeeInstall(int mode, const char *filename, uint32_t *sessionID)
{
    int ret = -1;
    uint32_t portalSize;
    struct TeePortalType *portal = NULL;

    if (mode < 0 || mode >= INV_INSTALL_UNINSTALL_TYPE || filename == NULL || sessionID == NULL)
        return -EINVAL;

    if (GetPortal((void **)&portal, &portalSize) != 0) {
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

    if (sessionID != NULL)
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

    if (GetPortal((void **)&portal, &portalSize) != 0) {
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
        offset += (strlen(name) + 1);

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

    if (GetPortal((void **)&portal, &portalSize) != 0) {
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

int TeeDelete(const char *filename, uint32_t sessionID)
{
    if (filename == NULL) {
        printf("delete check failed\n");
        return -EINVAL;
    }
    int ret = -1;
    uint32_t portalSize;
    struct TeePortalType *portal = NULL;

    if (GetPortal((void **)&portal, &portalSize) != 0) {
        printf("get portal failed\n");
        return -EFAULT;
    }

    portal->type = DELETE;
    portal->reeUID = getuid();
    portal->sessionID = sessionID;
    (void)memset_s(portal->args.query.dstPath, sizeof(portal->args.query.dstPath), 0,
                   sizeof(portal->args.query.dstPath));
    if (memcpy_s(portal->args.query.dstPath, sizeof(portal->args.query.dstPath), filename, strlen(filename)) != EOK) {
        printf("memcpy_s failed\n");
        return -EFAULT;
    }

    ret = TriggerPortal();
    if (ret != 0)
        printf("trigger portal failed\n");
    if (portal->ret != 0)
        ret = portal->ret;
    return ret;
}

int TeeQuery(const char *filename, uint32_t sessionID, bool *exist)
{
    if (filename == NULL || exist == NULL) {
        printf("query check failed\n");
        return -EINVAL;
    }
    int ret = 0;
    uint32_t portalSize;
    struct TeePortalType *portal = NULL;

    if (GetPortal((void **)&portal, &portalSize) != 0) {
        printf("get portal failed\n");
        return -EFAULT;
    }

    if (FileExist(filename, portal, NULL, 0, sessionID) != 0) {
        printf("cannot query file\n");
        return -1;
    }

    *exist = portal->args.query.exist;
    if (portal->ret != 0)
        ret = portal->ret;
    return ret;
}

int TeeDestroy(uint32_t sessionID)
{
    int ret = -1;
    uint32_t portalSize;
    struct TeePortalType *portal = NULL;

    if (GetPortal((void **)&portal, &portalSize) != 0) {
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

