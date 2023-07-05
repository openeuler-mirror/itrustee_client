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
#ifndef TEE_TP_PORTAL_H
#define TEE_TP_PORTAL_H

#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define PORTAL_CMD_FILE_MASK 0x10000
#define PORTAL_CMD_RUN_MASK 0x20000

#ifndef SHA256_DIGEST_LENGTH
#define SHA256_DIGEST_LENGTH 32
#endif

enum TeePortalCmdType {
    INPUT = PORTAL_CMD_FILE_MASK,
    OUTPUT,
    GET,
    QUERY,
    DELETE,
    DESTROY,
    INSTALL,              // include install python, java, python third party lib and app
    UNINSTALL,
    LIST,
    RUN_PYTHON = PORTAL_CMD_RUN_MASK,
    RUN_JAVA,
};

enum TeeInstallUninstallType {
    PYTHON_INTERPRETER,
    JAVA_RUNTIME,
    PYTHON_THIRD_PARTY,
    APPLICATION,
    INV_INSTALL_UNINSTALL_TYPE,
};

struct TeePortalTransportType {
    uint32_t fileSize;
    uint32_t blks;
    uint32_t bs;
    uint32_t nbr;
    char basename[FILENAME_MAX]; // the filename in REE
    char dstPath[PATH_MAX];      // the path in TEE
    unsigned char md[SHA256_DIGEST_LENGTH];
    uint32_t srcDataSize;
    uint32_t srcData[1];
};

struct TeePortalQueryType {
    unsigned char md[SHA256_DIGEST_LENGTH]; // file checksum
    char basename[FILENAME_MAX];            // the filename in REE
    char dstPath[PATH_MAX];                 // the path in TEE
    bool exist;
    bool isDir;
    bool checkMD;
};
#define PORTAL_RUN_ARGS_MAXSIZE 512
struct TeePortalRunType {
    char file[FILENAME_MAX]; // the path in TEE
    uint32_t xargsSize;
    char xargs[PORTAL_RUN_ARGS_MAXSIZE];
};

struct TeePortalListType {
    uint32_t numOfFiles;
    uint32_t offset;
    char filePath[1];
};

struct TeePortalInstallType {
    enum TeeInstallUninstallType type;
    char file[FILENAME_MAX];
};

struct TeePortalType {
    enum TeePortalCmdType type;
    int ret;
    uint32_t reeUID;
    uint32_t sessionID;
    union Args1 {
        struct TeePortalTransportType transport;
        struct TeePortalQueryType query;
        struct TeePortalRunType run;
        struct TeePortalListType list;
        struct TeePortalInstallType install;
    } args;
};

int InitPortal(void);
int GetPortal(void **portal, uint32_t *portalSize);
int TriggerPortal(void);
void DestroyPortal(void);

#endif
