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
#ifndef XTASKLET_COMMON_H
#define XTASKLET_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#define CONFIG_ENCRYPTION_CONCURRENCY 3

#define INFO(fmt, ...) \
    fprintf(stdout, "[info][pid %d] "fmt, getpid(), ##__VA_ARGS__)
#define ERR(fmt, ...) \
    fprintf(stderr, "[error][pid %d] "fmt, getpid(), ##__VA_ARGS__)
#ifdef CONFIG_DEBUG_BUILD
#define DBG(fmt, ...) \
    fprintf(stderr, "[debug][pid %d] "fmt, getpid(), ##__VA_ARGS__)
#else
#define DBG(fmt, ...)
#endif

#define B       (1)
#define KB      (1024 * B)
#define MB      (1024 * KB)
#define GB      (1024 * MB)

#define US      (1)
#define MS      (1000 * US)
#define S       (1000 * MS)

int GetTimestampUs(unsigned long *usec);

#endif