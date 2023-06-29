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

#ifndef TEE_TELEPORT_H
#define TEE_TELEPORT_H

#include <stdio.h>
#include <libgen.h>
#include <getopt.h>
#include <unistd.h>
#include "portal.h"
#include "scp.h"
#include "run.h"
#include "securec.h"

#define PARAM_NUM_MAX 64
#define PARAM_LEN_MAX PATH_MAX

enum TeleportArgType {
    TP_INSTALL,
    TP_IMPORT,
    TP_TYPE,
    TP_CREATE,
    TP_RUN,
    TP_ID,
    TP_INPUT,
    TP_OUTPUT,
    TP_RENAME,
    TP_SAVE,
    TP_PARAM,
    TP_DELETE,
    TP_QUERY,
    TP_DESTROY,
    TP_UNINSTALL,
    TP_LIST,
    TP_HELP,
    TP_TYPE_MAX,
};

struct TeeTeleportArgs {
    bool cmd[TP_TYPE_MAX];
    char installPath[PATH_MAX];
    char importPath[PATH_MAX];
    char typeParam[PATH_MAX];
    char createPath[PATH_MAX];
    char runPath[PATH_MAX];
    char idPath[PATH_MAX];
    char inputPath[PATH_MAX];
    char outputPath[PATH_MAX];
    char renamePath[PATH_MAX];
    char savePath[PATH_MAX];
    char paramVal[PARAM_LEN_MAX];
    char deletePath[PATH_MAX];
    char queryPath[PATH_MAX];
};

struct TeeTeleportFunc {
    enum TeleportArgType type;
    int32_t (*func)(const struct TeeTeleportArgs *args, uint32_t sessionID);
    bool needId;
};

#endif
