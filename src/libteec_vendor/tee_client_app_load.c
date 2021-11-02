/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2013-2021. All rights reserved.
 * iTrustee licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "tee_client_app_load.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>     /* for errno */
#include <sys/types.h> /* for open close */
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h> /* for ioctl */
#include <sys/mman.h>  /* for mmap */
#include <linux/limits.h>
#include "tee_log.h"
#include "tee_client_api.h"
#include "tc_ns_client.h"
#include "securec.h"
#include "tee_client_inner.h"
#include "secfile_load_agent.h"

/* debug switch */
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "teec_app_load"

#define MAX_PATH_LEN 256
const char *g_imagePath = "/vendor/bin";

static int32_t TEEC_ReadApp(const TaFileInfo *taFile, const char *loadFile, bool defaultPath,
                            TC_NS_ClientContext *cliContext);

int32_t TEEC_GetApp(const TaFileInfo *taFile, const TEEC_UUID *srvUuid, TC_NS_ClientContext *cliContext)
{
    int32_t ret;
    char fileName[MAX_FILE_NAME_LEN]                                        = { 0 };
    char tempName[MAX_FILE_PATH_LEN + MAX_FILE_NAME_LEN + MAX_FILE_EXT_LEN] = { 0 };

    if ((taFile == NULL) || (srvUuid == NULL) || (cliContext == NULL)) {
        tloge("param is null\n");
        return -1;
    }

    /* get file name and file patch */
    bool condition = (taFile->taPath != NULL) && (strlen((const char *)taFile->taPath) < MAX_PATH_LEN) &&
                     strstr((const char *)taFile->taPath, ".sec");
    if (condition) {
        ret = TEEC_ReadApp(taFile, (const char *)taFile->taPath, false, cliContext);
        if (ret < 0) {
            tloge("teec load app erro, ta path is not NULL\n");
        }
    } else {
        const char *filePath = g_imagePath;
        ret = snprintf_s(fileName, sizeof(fileName), sizeof(fileName) - 1,
                         "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", srvUuid->timeLow, srvUuid->timeMid,
                         srvUuid->timeHiAndVersion, srvUuid->clockSeqAndNode[0], srvUuid->clockSeqAndNode[1],
                         srvUuid->clockSeqAndNode[2], srvUuid->clockSeqAndNode[3], srvUuid->clockSeqAndNode[4],
                         srvUuid->clockSeqAndNode[5], srvUuid->clockSeqAndNode[6], srvUuid->clockSeqAndNode[7]);
        if (ret < 0) {
            tloge("get file name err\n");
            return -1;
        }

        size_t filePathLen = strnlen(filePath, MAX_FILE_PATH_LEN);
        filePathLen        = filePathLen + strnlen(fileName, MAX_FILE_NAME_LEN);
        filePathLen        = filePathLen + MAX_FILE_EXT_LEN;
        int32_t len        = snprintf_s(tempName, sizeof(tempName), filePathLen, "%s/%s.sec", filePath, fileName);
        if (len < 0) {
            tloge("file path too long\n");
            return -1;
        }

        ret = TEEC_ReadApp(taFile, (const char *)tempName, true, cliContext);
        if (ret < 0) {
            tloge("teec load app erro\n");
        }
    }

    return ret;
}

static int32_t GetTaVersion(FILE *fp, uint32_t *taHeadLen, uint32_t *version,
                            uint32_t *contextLen, uint32_t *totalImgLen)
{
    int32_t ret;
    TaImageHdrV3 imgIdentity = { { 0 }, 0, 0 };

    if (fp == NULL) {
        tloge("invalid fp\n");
        return -1;
    }

    /* get magic-num & version-num */
    ret = (int32_t)fread(&imgIdentity, sizeof(imgIdentity), 1, fp);
    if (ret != 1) {
        tloge("read file failed, ret=%d, error=%d\n", ret, ferror(fp));
        return -1;
    }

    bool condition = (imgIdentity.img_identity.magic_num1 == TA_HEAD_MAGIC1) &&
                     (imgIdentity.img_identity.magic_num2 == TA_HEAD_MAGIC2) &&
                     (imgIdentity.img_identity.version_num > 1);
    if (condition) {
        tlogd("new verison ta\n");
        *taHeadLen = sizeof(TeecTaHead);
        *version   = imgIdentity.img_identity.version_num;
        if (*version >= CIPHER_LAYER_VERSION) {
            *contextLen  = imgIdentity.context_len;
            *totalImgLen = *contextLen + sizeof(imgIdentity);
        } else {
            ret = fseek(fp, sizeof(imgIdentity.img_identity), SEEK_SET);
            if (ret != 0) {
                tloge("fseek error\n");
                return -1;
            }
        }
    } else {
        /* read the oldverison head again */
        tlogd("old verison ta\n");
        *taHeadLen = sizeof(TeecImageHead);
        ret       = fseek(fp, 0, SEEK_SET);
        if (ret != 0) {
            tloge("fseek error\n");
            return -1;
        }
    }
    return 0;
}

static int32_t TEEC_GetImageLenth(FILE *fp, uint32_t *imgLen)
{
    int32_t ret;
    TeecImageHead imageHead = { 0 };
    uint32_t totalImgLen;
    uint32_t taHeadLen = 0;
    uint32_t readSize;
    uint32_t version    = 0;
    uint32_t contextLen = 0;

    /* decide the TA verison */
    ret = GetTaVersion(fp, &taHeadLen, &version, &contextLen, &totalImgLen);
    if (ret != 0) {
        tloge("get Ta version failed\n");
        return ret;
    }

    if (version >= CIPHER_LAYER_VERSION) {
        goto CHECK_LENTH;
    }

    /* get image head */
    readSize = (uint32_t)fread(&imageHead, sizeof(TeecImageHead), 1, fp);
    if (readSize != 1) {
        tloge("read file failed, err=%u\n", readSize);
        return -1;
    }
    contextLen  = imageHead.context_len;
    totalImgLen = contextLen + taHeadLen;

CHECK_LENTH:
    /* for no overflow */
    if ((contextLen > MAX_IMAGE_LEN) || (totalImgLen > MAX_IMAGE_LEN)) {
        tloge("check img size failed\n");
        return -1;
    }

    ret = fseek(fp, 0, SEEK_SET);
    if (ret != 0) {
        tloge("fseek error\n");
        return -1;
    }

    *imgLen = totalImgLen;
    return 0;
}

static int32_t TEEC_DoReadApp(FILE *fp, TC_NS_ClientContext *cliContext)
{
    uint32_t totalImgLen = 0;

    /* get magic-num & version-num */
    int32_t ret = TEEC_GetImageLenth(fp, &totalImgLen);
    if (ret) {
        tloge("get image lenth fail\n");
        return -1;
    }

    /* alloc a less than 8M heap memory, it needn't slice. */
    char *fileBuffer = malloc(totalImgLen);
    if (fileBuffer == NULL) {
        tloge("alloc TA file buffer(size=%u) failed\n", totalImgLen);
        return -1;
    }

    /* read total ta file to file buffer */
    uint32_t readSize = (uint32_t)fread(fileBuffer, 1, totalImgLen, fp);
    if (readSize != totalImgLen) {
        tloge("read ta file failed, read size/total size=%u/%u\n", readSize, totalImgLen);
        free(fileBuffer);
        return -1;
    }
    cliContext->file_size   = totalImgLen;
    cliContext->file_buffer = fileBuffer;
    return 0;
}

static int32_t TEEC_ReadApp(const TaFileInfo *taFile, const char *loadFile, bool defaultPath,
                            TC_NS_ClientContext *cliContext)
{
    int32_t ret                     = 0;
    FILE *fp                        = NULL;
    FILE *fpTmp                     = NULL;
    char realLoadFile[PATH_MAX + 1] = { 0 };

    if (taFile->taFp != NULL) {
        fp = taFile->taFp;
        tlogd("libteec_vendor-read_app: get fp from ta fp\n");
        goto READ_APP;
    }

    if (realpath(loadFile, realLoadFile) == NULL) {
        if (defaultPath == false) {
            tloge("get file realpath error%d\n", errno);
            return -1;
        }

        /* maybe it's a built-in TA */
        tlogd("maybe it's a built-in TA or file is not in default path\n");
        return ret;
    }

    /* open image file */
    fpTmp = fopen(realLoadFile, "r");
    if (fpTmp == NULL) {
        tloge("open file error%d\n", errno);
        return -1;
    }
    fp = fpTmp;

READ_APP:
    ret = TEEC_DoReadApp(fp, cliContext);
    if (ret != 0) {
        tloge("do read app fail\n");
    }

    if (fpTmp != NULL) {
        fclose(fpTmp);
    }

    return ret;
}

int32_t TEEC_LoadSecfile(const char *filePath, int tzFd, FILE *fp)
{
    int ret;
    FILE *fpUsable              = NULL;
    bool checkValue             = (tzFd < 0 || filePath == NULL);
    FILE *fpCur                 = NULL;
    char realPath[PATH_MAX + 1] = { 0 };

    if (checkValue == true) {
        tloge("Param err!\n");
        return -1;
    }
    if (fp == NULL) {
        if (realpath(filePath, realPath) != NULL) {
            fpCur = fopen(realPath, "r");
        }
        if (fpCur == NULL) {
            tloge("realpath open file erro%d, path=%s\n", errno, filePath);
            return -1;
        }
        fpUsable = fpCur;
    } else {
        fpUsable = fp;
    }
    ret = LoadSecFile(tzFd, fpUsable, LOAD_LIB, NULL);
    if (fpCur != NULL) {
        fclose(fpCur);
    }
    return ret;
}
