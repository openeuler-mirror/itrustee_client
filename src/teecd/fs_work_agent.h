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
#ifndef LIBTEEC_FS_WORK_AGENT_H
#define LIBTEEC_FS_WORK_AGENT_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define FILE_NAME_MAX_BUF       256
#define FILE_NUM_LIMIT_MAX      1024
#define KINDS_OF_SSA_MODE       4
#define AGENT_FS_ID 0x46536673
#define AID_SYSTEM 1000

/* static func declare */
enum FsCmdType {
    SEC_OPEN,
    SEC_CLOSE,
    SEC_READ,
    SEC_WRITE,
    SEC_SEEK,
    SEC_REMOVE,
    SEC_TRUNCATE,
    SEC_RENAME,
    SEC_CREATE,
    SEC_INFO,
    SEC_ACCESS,
    SEC_ACCESS2,
    SEC_FSYNC,
    SEC_CP,
    SEC_DISKUSAGE,
    SEC_DELETE_ALL,
    SEC_MAX
};

enum {
    SEC_WRITE_SLOG,
    SEC_WRITE_SSA,
};

struct SecStorageType {
    enum FsCmdType cmd; /* for s to n */
    int32_t ret;   /* fxxx call's return */
    int32_t ret2;  /* fread: end-of-file or error;fwrite:the sendor is SSA or SLOG */
    uint32_t userId;
    uint32_t storageId;
    uint32_t magic;
    uint32_t error;
    union Args1 {
        struct {
            char mode[KINDS_OF_SSA_MODE];
            uint32_t nameLen;
            uint32_t name[1];
        } open;
        struct {
            int32_t fd;
        } close;
        struct {
            int32_t fd;
            uint32_t count;
            uint32_t buffer[1]; /* the same as name[0] --> name[1] */
        } read;
        struct {
            int32_t fd;
            uint32_t count;
            uint32_t buffer[1];
        } write;
        struct {
            int32_t fd;
            int32_t offset;
            uint32_t whence;
        } seek;
        struct {
            uint32_t nameLen;
            uint32_t name[1];
        } remove;
        struct {
            uint32_t len;
            uint32_t nameLen;
            uint32_t name[1];
        } truncate;
        struct {
            uint32_t oldNameLen;
            uint32_t newNameLen;
            uint32_t buffer[1]; /* old_name + new_name */
        } rename;
        struct {
            uint32_t fromPathLen;
            uint32_t toPathLen;
            uint32_t buffer[1]; /* from_path+to_path */
        } cp;
        struct {
            char mode[KINDS_OF_SSA_MODE];
            uint32_t nameLen;
            uint32_t name[1];
        } create;
        struct {
            int32_t fd;
            uint32_t curPos;
            uint32_t fileLen;
        } info;
        struct {
            int mode;
            uint32_t nameLen;
            uint32_t name[1];
        } access;
        struct {
            int32_t fd;
        } fsync;
        struct {
            uint32_t secStorage;
            uint32_t data;
        } diskUsage;
        struct {
            uint32_t pathLen;
            uint32_t path[1];
        } deleteAll;
    } args;
};

struct OpenedFile {
    FILE *file;
    struct OpenedFile *next;
    struct OpenedFile *prev;
};

int IsUserDataReady(void);
void *FsWorkThread(void *control);
void SetFileNumLimit(void);
int GetFsAgentFd(void);
void *GetFsAgentControl(void);

int FsAgentInit(void);
void FsAgentThreadCreate(void);
void FsAgentThreadJoin(void);
void FsAgentExit(void);

#endif
