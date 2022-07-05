/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2013-2022. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef LIBTEEC_TEE_CLIENT_APP_LOAD_H
#define LIBTEEC_TEE_CLIENT_APP_LOAD_H

#include "tc_ns_client.h"
#include "tee_client_inner.h"
#include "tee_client_type.h"

#define MAX_FILE_PATH_LEN 20
#define MAX_FILE_NAME_LEN 40
#define MAX_FILE_EXT_LEN  6

#define MAX_IMAGE_LEN          0x800000 /* max image len */
#define MAX_SHARE_BUF_LEN      0x100000 /* max share buf len */
#define LOAD_IMAGE_FLAG_OFFSET 0x4
#define SEND_IMAGE_LEN         (MAX_SHARE_BUF_LEN - LOAD_IMAGE_FLAG_OFFSET)

#define TA_HEAD_MAGIC1         0xA5A55A5A
#define TA_HEAD_MAGIC2         0x55AA
#define NUM_OF_RESERVED_BITMAP 16

enum TA_VERSION {
    TA_SIGN_VERSION        = 1, /* first version */
    TA_RSA2048_VERSION     = 2, /* use rsa 2048, and use right crypt mode */
    CIPHER_LAYER_VERSION   = 3,
    CONFIG_SEGMENT_VERSION = 4,
    TA_SIGN_VERSION_MAX
};

typedef struct {
    uint32_t file_buf;
    uint32_t file_buf_h;
    uint32_t file_size;
    uint32_t reserved;
} RegisterBufStruct;

typedef struct {
    uint32_t context_len;         /* manifest_crypto_len + cipher_bin_len */
    uint32_t manifest_crypto_len; /* manifest crypto len */
    uint32_t manifest_plain_len;  /* manfiest str + manifest binary */
    uint32_t manifest_str_len;    /* manifest str len */
    uint32_t cipher_bin_len;      /* cipher elf len */
    uint32_t sign_len;            /* sign file len, now rsa 2048 this len is 256 */
} TeecImageHead;

typedef struct {
    uint32_t magic_num1;
    uint16_t magic_num2;
    uint16_t version_num;
} TeecImageIdentity;

typedef struct {
    TeecImageIdentity img_identity;
    uint32_t context_len;
    uint32_t ta_key_version;
} TaImageHdrV3;

typedef struct {
    TeecImageHead img_hd;
    TeecImageIdentity img_identity;
    uint8_t reserved[NUM_OF_RESERVED_BITMAP];
} TeecTaHead;

int32_t TEEC_GetApp(const TaFileInfo *taFile, const TEEC_UUID *srvUuid, TC_NS_ClientContext *cliContext);
int32_t TEEC_LoadSecfile(const char *filePath, int tzFd, FILE *fp);

#endif
