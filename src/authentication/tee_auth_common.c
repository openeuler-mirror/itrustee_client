/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2021. All rights reserved.
 * iTrustee licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "tee_auth_common.h"
#include <stdbool.h>
#include <errno.h>
#include "securec.h"
#include "tee_log.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "teecd_auth"

#define PASSWD_FILE "/etc/passwd"
#define DELIM_COUNT 6U
#define NAME_POS    0U
#define UID_POS     1U

#define DECIMAL 10
static int UidCompare(unsigned int caUid, const char *uidString, size_t uidLen)
{
    size_t i = 0;
    size_t uidNum = 0;

    // convert uid string to integer
    for (; i < uidLen; i++) {
        bool is_number = uidString[i] >= '0' && uidString[i] <= '9';
        if (!is_number) {
            tloge("passwd info wrong format: uid missing\n");
            return -1;
        }
        uidNum = DECIMAL * uidNum + (size_t)(uidString[i] - '0');
    }

    if (uidNum == caUid) {
        return 0;
    }

    return -1;
}

static int SplitFields(int delimIndices[], const char *userName, int nameBufLen)
{
    int i = 0;
    unsigned int count = 0;

    for (; i < nameBufLen && userName[i] != '\0'; i++) {
        if (count > DELIM_COUNT) {
            tloge("passwd info wrong format: extra field\n");
            return -1;
        }
        if (userName[i] == ':') {
            delimIndices[count] = i;
            count++;
        }
    }

    return 0;
}

static int ParseUserName(unsigned int caUid, const char *userName, int nameBufLen)
{
    int delimIndices[DELIM_COUNT + 1] = { 0 };

    int ret = SplitFields(delimIndices, userName, nameBufLen);
    if (ret != 0) {
        return ret;
    }

    const char *uidString = userName + delimIndices[UID_POS] + 1;
    int uidLen = delimIndices[UID_POS + 1] - delimIndices[UID_POS] - 1;

    if (UidCompare(caUid, uidString, uidLen) == 0) {
        return delimIndices[NAME_POS];
    }

    return -1;
}

/* get username by uid,
 * on linux, user info is stored in system file "/etc/passwd",
 * each line represents a user, fields are separated by ':',
 * formatted as such: "username:[encrypted password]:uid:gid:[comments]:home directory:login shell"
 */
int TeeGetUserName(unsigned int caUid, char *userName, size_t nameBufLen)
{
    int i;

    FILE *fd = fopen(PASSWD_FILE, "r");
    if (fd == NULL) {
        tloge("open passwd file failed\n");
        return -1;
    }

    // fgets will append a '\0' to userName, no need to memset it every time
    while (fgets(userName, nameBufLen - 1, fd) != NULL) {
        int userNameLen = ParseUserName(caUid, userName, (int)nameBufLen);
        if (userNameLen != -1) {
            // erase the buffer after username
            for (i = userNameLen; i < (int)nameBufLen; i++) {
                userName[i] = '\0';
            }
            fclose(fd);
            return 0;
        }
    }

    fclose(fd);
    return -1;
}

// locate pkgname start index
static int LocatePkgName(const char *cmdLine, size_t cmdLen)
{
    int rIndex = (int)cmdLen - 1;
    bool rFound = false;

    while (rIndex >= 0) {
        // there could be some '\0's at the end of this buffer, should skip them
        if (cmdLine[rIndex] == '\0') {
            if (rFound) {
                break;
            }
            rIndex--;
            continue;
        }
        rFound = true;
        rIndex--;
    }

    return rIndex + 1;
}

/* fd is valid, cmdLine won't be NULL, already checked in TeeGetCaName
 * cmdline format looks like this:
 * "java option1 option2 ... optionn -jar com.company.module.app"
 * we need to extract the package name "com.company.module.app" from it
 */
#define JAVA_CMD_MIN_INDEX 3
#define JAVA_OFFSET_3      3
#define JAVA_OFFSET_2      2
#define JAVA_OFFSET_1      1
static int ParsePkgName(const char *cmdLine, size_t cmdLen, char *caName, size_t nameLen)
{
    // there will be a '\0' at the end of this string
    int rIndex = (int)strnlen(caName, nameLen) - 1;
    bool is_java_cmd = rIndex >= JAVA_CMD_MIN_INDEX &&
                       caName[rIndex - JAVA_OFFSET_3] == 'j' &&
                       caName[rIndex - JAVA_OFFSET_2] == 'a' &&
                       caName[rIndex - JAVA_OFFSET_1] == 'v' &&
                       caName[rIndex]                 == 'a';
    if (!is_java_cmd) {
        return 0;
    }

    rIndex = LocatePkgName(cmdLine, cmdLen);
    int pkgLen = strnlen(cmdLine + rIndex, cmdLen - rIndex);

    errno_t ret = strncpy_s(caName, nameLen - 1, cmdLine + rIndex, pkgLen);
    if (ret != EOK) {
        tloge("copy caName failed\n");
        return -1;
    }

    // erase the buffer after pkgname
    for (; pkgLen < (int)nameLen; pkgLen++) {
        caName[pkgLen] = '\0';
    }

    return 0;
}

static int ReadCmdLine(const char *path, char *buffer, int bufferLen, char *caName, size_t nameLen)
{
    FILE *fd = fopen(path, "rb");
    if (fd == NULL) {
        tloge("fopen is error: %d\n", errno);
        return -1;
    }
    int bytesRead = fread(buffer, sizeof(char), bufferLen - 1, fd);
    bool readError = bytesRead <= 0 || ferror(fd);
    if (readError) {
        tloge("cannot read from cmdline\n");
        fclose(fd);
        return -1;
    }
    fclose(fd);

    int firstStringLen = (int)strnlen(buffer, bufferLen - 1);
    errno_t res = strncpy_s(caName, nameLen - 1, buffer, firstStringLen);
    if (res != EOK) {
        tloge("copy caName failed\n");
        return -1;
    }

    return bytesRead;
}

/*
 * this file "/proc/pid/cmdline" can be modified by any user,
 * so the package name we get from it is not to be trusted,
 * the CA authentication strategy does not rely much on the pkgname,
 * this is mainly to make it compatible with POHNE_PLATFORM
 */
static int TeeGetCaName(int caPid, char *caName, size_t nameLen)
{
    char path[MAX_PATH_LENGTH] = { 0 };
    char temp[CMD_MAX_SIZE] = { 0 };

    if (caName == NULL || nameLen == 0) {
        tloge("input :caName invalid\n");
        return -1;
    }

    int ret = snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/cmdline", caPid);
    if (ret == -1) {
        tloge("tee get ca name snprintf_s err\n");
        return ret;
    }

    int bytesRead = ReadCmdLine(path, temp, CMD_MAX_SIZE, caName, nameLen);

    bool stat = bytesRead <= 0 || ParsePkgName(temp, bytesRead, caName, nameLen) != 0;
    if (stat) {
        tloge("parse package name from cmdline failed\n");
        return -1;
    }

    return bytesRead;
}

int TeeGetPkgName(int caPid, char *path, size_t pathLen)
{
    if (path == NULL || pathLen > MAX_PATH_LENGTH) {
        tloge("path is null or path len overflow\n");
        return -1;
    }

    if (TeeGetCaName(caPid, path, pathLen) < 0) {
        tloge("get ca name failed\n");
        return -1;
    }

    if (strncmp(path, MEDIA_CODEC_PATH, strlen(MEDIA_CODEC_PATH) + 1) == 0) {
        int ret = snprintf_s(path, pathLen, strlen(OMX_PATH), OMX_PATH);
        if (ret < 0) {
            tloge("copy omx path failed");
            return ret;
        }
    }

    return 0;
}

