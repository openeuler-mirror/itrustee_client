/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2023. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "tee_get_native_cert.h"
#include "securec.h"
#include "tee_client_type.h"
#include "tee_log.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG       "teecd_auth"
#define INVLIAD_PARAM (-1)

static int SetPathToBuf(uint8_t *buffer, uint32_t *len, uint32_t *inputLen, const char *path)
{
    int ret = -1;
    uint32_t num;
    uint32_t pathLen;

    num     = (uint32_t)strlen(path);
    pathLen = num;

    if (*inputLen < sizeof(pathLen)) {
        tloge("buffer overflow for pathLen\n");
        return ret;
    }

    ret = memcpy_s(buffer, *inputLen, &pathLen, sizeof(pathLen));
    if (ret != EOK) {
        tloge("copy pkgname length failed\n");
        return ret;
    }

    buffer    += sizeof(pathLen);
    *len      += (uint32_t)sizeof(pathLen);
    *inputLen -= (uint32_t)sizeof(pathLen);
    ret = -1;

    if (num > *inputLen) {
        tloge("buffer overflow for path\n");
        return ret;
    }

    ret = memcpy_s(buffer, *inputLen, path, num);
    if (ret != EOK) {
        tloge("copy pkgname failed\n");
        return ret;
    }

    buffer    += num;
    *len      += num;
    *inputLen -= num;

    return ret;
}

static int SetUserNameToBuf(uint8_t *buffer, uint32_t *len, uint32_t *inputLen, const char *userName)
{
    int ret = -1;
    uint32_t num;
    uint32_t nameLen;

    num     = (uint32_t)strlen(userName);
    nameLen = num;

    if (*inputLen < sizeof(nameLen)) {
        tloge("buffer overflow for nameLen\n");
        return ret;
    }

    ret = memcpy_s(buffer, *inputLen, &nameLen, sizeof(nameLen));
    if (ret != EOK) {
        tloge("copy username length failed\n");
        return ret;
    }

    buffer    += sizeof(nameLen);
    *len      += sizeof(nameLen);
    *inputLen -= sizeof(nameLen);
    ret = -1;

    if (num > *inputLen) {
        tloge("buffer overflow for userName\n");
        return ret;
    }

    ret = memcpy_s(buffer, *inputLen, userName, num);
    if (ret != EOK) {
        tloge("copy username failed\n");
        return ret;
    }

    buffer    += num;
    *len      += num;
    *inputLen -= num;

    return ret;
}

static int SetUserInfoToBuf(uint8_t *buffer, uint32_t *len, uint32_t *inputLen, int caPid, unsigned int caUid)
{
    int ret;

    char userName[MAX_NAME_LENGTH] = { 0 };
    ret = TeeGetUserName(caPid, caUid, userName, sizeof(userName));
    if (ret != 0) {
        tloge("get user name failed\n");
        return ret;
    }

    ret = SetUserNameToBuf(buffer, len, inputLen, userName);
    if (ret != 0) {
        tloge("set username failed\n");
    }

    return ret;
}

int TeeGetNativeCert(int caPid, unsigned int caUid, uint32_t *len, uint8_t *buffer)
{
    int ret;
    char path[MAX_PATH_LENGTH] = { 0 };
    uint32_t inputLen;
    bool invalid = (len == NULL) || (buffer == NULL);
    if (invalid) {
        tloge("Param error!\n");
        return INVLIAD_PARAM;
    }
    inputLen = *len;

    ret = TeeGetPkgName(caPid, path, sizeof(path));
    if (ret != 0) {
        tloge("get ca path failed\n");
        return ret;
    }

    *len = 0;

    ret = SetPathToBuf(buffer, len, &inputLen, path);
    if (ret != 0) {
        tloge("set path failed\n");
        return ret;
    }
    buffer += (sizeof(uint32_t) + strlen(path));

    return SetUserInfoToBuf(buffer, len, &inputLen, caPid, caUid);
}
