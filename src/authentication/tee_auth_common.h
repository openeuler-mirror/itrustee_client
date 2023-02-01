/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2022. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef TEE_AUTH_COMMON_H
#define TEE_AUTH_COMMON_H

#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PATH_LENGTH        256

#define MAX_NAME_LENGTH        256

#define BUF_MAX_SIZE           4096
#define CMD_MAX_SIZE           1024
#define BACKLOG_LEN            10
#define HASH_FILE_MAX_SIZE     (16 * 1024)
#define MEDIA_CODEC_PATH       "media.codec"
#define OMX_PATH               "/vendor/bin/hw/android.hardware.media.omx@1.0-service"
#define MAX_SCTX_LEN           128

#ifndef CA_HIDL_PATH_UID_AUTH_CTX
#define CA_HIDL_PATH_UID_AUTH_CTX ""
#endif

typedef enum {
    SYSTEM_CA = 1,
    VENDOR_CA,
    APP_CA,
    MAX_CA,
} CaType;

typedef struct {
    uint8_t certs[BUF_MAX_SIZE];
    CaType type;
    uid_t uid;
    pid_t pid;
    int fromHidlSide;
} CaAuthInfo;

typedef struct {
    int cmd;
    CaAuthInfo caAuthInfo;
    uint32_t xmlBufSize;
    uint8_t xmlBuffer[HASH_FILE_MAX_SIZE];
} CaRevMsg;

typedef struct {
    uint32_t fileSize;                      /* xml file size */
    uint8_t fileBuf[HASH_FILE_MAX_SIZE];    /* read xml file data to this buffer */
} TEEC_XmlParameter;


#define HIDL_SIDE     0xffe0
#define NON_HIDL_SIDE 0xffe1

int TeeGetPkgName(int caPid, char *path, size_t pathLen);

int TeeGetUserName(unsigned int caUid, char *userName, size_t nameLen);

int TeeCheckHidlAuth(unsigned int uid, int pid);

#ifdef __cplusplus
}
#endif

#endif
