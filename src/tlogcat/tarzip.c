/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2014-2023. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "tarzip.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include <securec.h>

#include "tee_log.h"
#include "tlogcat.h"

#define HEADER_NUM           512
#define CHECK_SUM_APPEND     256
#define NAME_LEN             100U
#define FILE_MODE_LEN        8U
#define UID_LEN              8U
#define GID_LEN              8U
#define FILE_SIZE            12U
#define UNIX_TIME_LEN        12U
#define CHECK_SUM_LEN        8U
#define HEADER_RESVD_LEN     356U

/* tar file header struct */
struct TagHeader { /* byte offset */
    char name[NAME_LEN];  /* 0 */
    char mode[FILE_MODE_LEN]; /* 100 */
    char uid[UID_LEN]; /* 108 */
    char gid[GID_LEN]; /* 116 */
    char size[FILE_SIZE]; /* 124 */
    char unixTime[UNIX_TIME_LEN]; /* 136 */
    char checkSum[CHECK_SUM_LEN]; /* 148 */
    char reserved[HEADER_RESVD_LEN]; /* 156 */
};

/* tar file header mode permissions data */
static const char FILE_MODE[] = { 0x31, 0x30, 0x30, 0x36, 0x36, 0x36, 0x20, 0 };

/* tar file haeder UID and GID date */
static const char ID_BYTE[] = {
    0x20, 0x20, 0x20, 0x20, 0x20, 0x30, 0x20, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x30, 0x20, 0x00,
};

static int32_t WriteOneUint(size_t startIndex, size_t unitLen, const char *input, char *ouput)
{
    size_t i;
    int32_t sum = 0;

    for (i = 0; i < unitLen; ++i) {
        ouput[i + startIndex] = input[i];
        sum += (int32_t)input[i];
    }

    return sum;
}

/* write the tar file harder to header struct */
static void WriteHeader(struct TagHeader *header, const char *fileName, long fileSize)
{
    int32_t sumAppend = 0;
    size_t nameLen;
    size_t i;
    size_t j = 0;
    char *index = (char *)header;
    char buf[FILE_SIZE] = {0};
    errno_t rc;

    if (fileName == NULL || fileSize == 0) {
        return;
    }

    nameLen = strlen(fileName);
    nameLen = ((nameLen >= NAME_LEN) ? (NAME_LEN - 1) : nameLen);

    sumAppend += WriteOneUint(j, nameLen, fileName, index);
    j += NAME_LEN;

    sumAppend += WriteOneUint(j, FILE_MODE_LEN, FILE_MODE, index);
    j += FILE_MODE_LEN;

    sumAppend += WriteOneUint(j, (UID_LEN + GID_LEN), ID_BYTE, index);
    j += (UID_LEN + GID_LEN);

    rc = snprintf_s(buf, FILE_SIZE, FILE_SIZE - 1, "%o", (unsigned int)fileSize);
    if (rc == -1) {
        tloge("snprintf_s failed: %d\n", rc);
        return;
    }

    sumAppend += WriteOneUint(j, FILE_SIZE, buf, index);
    j += (FILE_SIZE + UNIX_TIME_LEN);

    sumAppend += CHECK_SUM_APPEND;
    rc = snprintf_s(buf, FILE_SIZE, FILE_SIZE - 1, "%o", sumAppend);
    if (rc == -1) {
        tloge("snprintf_s failed: %d\n", rc);
        return;
    }
    for (i = 0; i < CHECK_SUM_LEN; ++i) {
        index[j + i] = buf[i];
    }
}

#define ZIP_OPEN_MODE 0400U

/* write file content to tar zip file */
static void WriteZipContent(gzFile gzFd, const char *fileName, long fileSize)
{
    char buf[HEADER_NUM];
    ssize_t ret;
    int32_t iret;
    long temFileSize = fileSize;
    bool cond = (gzFd == NULL || fileName == NULL || fileSize == 0);

    if (cond) {
        tloge("fd or fileName or fileSize invalid\n");
        return;
    }

    int32_t fileFd = open(fileName, O_CREAT | O_RDWR, ZIP_OPEN_MODE);
    if (fileFd < 0) {
        return;
    }
    while (temFileSize > 0) {
        (void)memset_s(buf, HEADER_NUM, 0, HEADER_NUM);
        ret = read(fileFd, buf, HEADER_NUM);
        if (ret < 0) {
            tloge("read failed\n");
            goto CLOSE_FD;
        }

        iret = gzwrite(gzFd, buf, HEADER_NUM);
        if (iret < 0) {
            tloge("gzwrite failed\n");
            goto CLOSE_FD;
        } else if (iret < HEADER_NUM) {
            tloge("incomplete gzwrite\n");
            goto CLOSE_FD;
        }

        temFileSize -= HEADER_NUM;
    }

CLOSE_FD:
    close(fileFd);
}

static int32_t OpenZipFile(const char *outputName, gzFile *outFile, gid_t pathGroup)
{
    int32_t ret;

    *outFile = NULL;

    int32_t fd = open(outputName, O_CREAT | O_WRONLY, ZIP_OPEN_MODE);
    if (fd < 0) {
        tloge("open file[%s] failed, errno = %d\n", outputName, errno);
        return -1;
    }
    gzFile out = gzdopen(fd, "w");
    if (out == NULL) {
        tloge("change fd to file failed\n");
        close(fd);
        return -1;
    }
    ret = fchown(fd, (uid_t)-1, pathGroup);
    if (ret < 0) {
        tloge("chown failed, errno = %d\n", errno);
        gzclose(out);
        return -1;
    }
    ret = fchmod(fd, S_IRUSR | S_IRGRP);
    if (ret < 0) {
        tloge("chmod failed, errno = %d\n", errno);
        gzclose(out);
        return -1;
    }

    *outFile = out;
    return 0;
}

static bool JudgeFileValidite(const char *index, struct stat *fileAttr)
{
    if (index == NULL) {
        return false;
    }

    if (lstat(index, fileAttr) < 0) {
        return false;
    }

    if (!S_ISREG(fileAttr->st_mode)) {
        return false;
    }
    return true;
}

#define FILE_NAME_INVALID_LEN  8U
static int32_t WriteSingleFile(const char *fileName, gzFile out)
{
    struct stat fileAttr = {0};
    struct TagHeader header;
    char *s1 = NULL;
    int32_t ret;

    if (!JudgeFileValidite(fileName, &fileAttr)) {
        return 0;
    }

    /* fileName contain file path and file name, search for the file name from fileName. */
    s1 = strrchr(fileName, '/');
    if (s1 == NULL || strlen(s1) < FILE_NAME_INVALID_LEN) {
        return -1;
    }

    (void)memset_s(&header, sizeof(header), 0, sizeof(header));
    WriteHeader(&header, (const char *)(s1 + 1), fileAttr.st_size);
    ret = gzwrite(out, &header, sizeof(struct TagHeader));
    if (ret < 0) {
        tloge("gzwrite failed\n");
        return -1;
    }
    WriteZipContent(out, fileName, fileAttr.st_size);
    return 0;
}

/* tar and zip input files to output file */
void TarZipFiles(uint32_t nameCount, const char **inputNames, const char *outputName, gid_t pathGroup)
{
    gzFile out = NULL;
    int32_t ret;
    struct TagHeader endHeader;
    const char **fileName = NULL;
    uint32_t i;
    bool cond = (inputNames == NULL || outputName == NULL || nameCount != LOG_FILE_INDEX_MAX);

    if (cond) {
        return;
    }

    ret = OpenZipFile(outputName, &out, pathGroup);
    if (ret != 0) {
        return;
    }

    fileName = inputNames;
    for (i = 0; i < nameCount; ++i) {
        if (*fileName == NULL) {
            ++fileName;
            continue;
        }
        ret = WriteSingleFile(*fileName, out);
        if (ret != 0) {
            goto GZ_CLOSE;
        }

        ++fileName;
    }

    (void)memset_s(&endHeader, sizeof(endHeader), 0, sizeof(endHeader));
    ret = gzwrite(out, &endHeader, sizeof(endHeader));
    if (ret < 0) {
        tloge("gzwrite failed\n");
    }

GZ_CLOSE:
    gzclose(out);
    return;
}
