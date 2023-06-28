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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include "securec.h"

int MkdirIteration(const char *dir)
{
    struct stat ss;
    if (lstat(dir, &ss) == 0) {
        if (!S_ISDIR(ss.st_mode)) {
            printf("%s exist, but not a directory, mode=%d\n", dir, ss.st_mode);
            return -EOPNOTSUPP;
        }
        return 0;
    }

    char tmpPath[PATH_MAX] = {0};
    char *subDir = strrchr(dir, '/');
    if (subDir == NULL) {
        printf("bad parameter, dir is %s\n", dir);
        return -1;
    }

    if (strncpy_s(tmpPath, PATH_MAX, dir, (size_t)(subDir - dir)) != 0) {
        printf("failed to copy subDir!\n");
        return -1;
    }

    int ret = MkdirIteration(tmpPath);
    if (ret != 0) {
        printf("mkdir %s failed\n", tmpPath);
        return ret;
    }
    ret = mkdir(dir, S_IRWXU | S_IRGRP | S_IXGRP);
    if (ret != 0 && errno != EEXIST) {
        printf("mkdir %s failed: %s\n", dir, strerror(errno));
        return ret;
    }
    return 0;
}
