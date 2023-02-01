/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2022. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "tee_load_sec_file.h"
#include "tc_ns_client.h"
#include "tee_log.h"
#include "securec.h"

static int GetImgLen(FILE *fp, long *totalLlen)
{
    int ret;

    ret = fseek(fp, 0, SEEK_END);
    if (ret != 0) {
        tloge("fseek error\n");
        return -1;
    }
    *totalLlen = ftell(fp);
    if (*totalLlen <= 0 || *totalLlen > MAX_BUFFER_LEN) {
        tloge("file is not exist or size is too large, filesize = %ld\n", *totalLlen);
        return -1;
    }
    ret = fseek(fp, 0, SEEK_SET);
    if (ret != 0) {
        tloge("fseek error\n");
        return -1;
    }
    return ret;
}

/* input param uuid may be NULL, so don need to check if uuid is NULL */
int32_t LoadSecFile(int tzFd, FILE *fp, enum SecFileType fileType, const TEEC_UUID *uuid, int32_t *errCode)
{
    int32_t ret;
    char *fileBuffer                   = NULL;
    struct SecLoadIoctlStruct ioctlArg = { { 0 }, { 0 }, { NULL } };

    if (tzFd < 0 || fp == NULL) {
        tloge("param error!\n");
        return -1;
    }

    do {
        long totalLen = 0;
        ret           = GetImgLen(fp, &totalLen);
        if (ret != 0) {
            break;
        }

        ret = -1;
        if (totalLen <= 0) {
            tloge("totalLen is invalid\n");
            break;
        }
        /* alloc a less than 8M heap memory, it needn't slice. */
        fileBuffer = malloc((size_t)totalLen);
        if (fileBuffer == NULL) {
            tloge("alloc TA file buffer(size=%ld) failed\n", totalLen);
            break;
        }

        /* read total ta file to file buffer */
        long fileSize = (long)fread(fileBuffer, 1, (size_t)totalLen, fp);
        if (fileSize != totalLen) {
            tloge("read ta file failed, read size/total size=%ld/%ld\n", fileSize, totalLen);
            break;
        }

        ioctlArg.secFileInfo.fileType = fileType;
        ioctlArg.secFileInfo.fileSize = (uint32_t)totalLen;
        ioctlArg.memref.file_addr = (uint32_t)(uintptr_t)fileBuffer;
        ioctlArg.memref.file_h_addr = (uint32_t)(((uint64_t)(uintptr_t)fileBuffer) >> H_OFFSET);
        if (uuid != NULL && memcpy_s((void *)(&ioctlArg.uuid), sizeof(ioctlArg.uuid), uuid, sizeof(*uuid)) != EOK) {
            tloge("memcpy uuid fail\n");
            break;
        }

        ret = ioctl(tzFd, TC_NS_CLIENT_IOCTL_LOAD_APP_REQ, &ioctlArg);
        if (ret != 0) {
            tloge("ioctl to load sec file failed, ret = 0x%x\n", ret);
        }
    } while (false);

    if (fileBuffer != NULL) {
        free(fileBuffer);
    }
    if (errCode != NULL) {
        *errCode = ioctlArg.secFileInfo.secLoadErr;
    }
    return ret;
}
