/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: tee custom log api
 */

#ifndef TEEC_CUSTOM_LOG_H
#define TEEC_CUSTOM_LOG_H

#include <stdio.h>
#include <stdint.h>

#ifndef LOG_TAG
#define LOG_TAG ""
#endif

#ifdef DEF_ENG
#define TEE_LOG_MASK    0
#else
#define TEE_LOG_MASK    2
#endif

#define TAG_VERBOSE "[VERBOSE]"
#define TAG_DEBUG   "[DEBUG]"
#define TAG_INFO    "[INFO]"
#define TAG_WARN    "[WARN]"
#define TAG_ERROR   "[ERROR]"

#define LEVEL_VERBOSE   0
#define LEVEL_DEBUG     1
#define LEVEL_INFO      2
#define LEVEL_WARN      3
#define LEVEL_ERROR     4

/*
 * log print to custom file
 */
void LogPrint(uint8_t logLevel, const char *fmt, ...);

#define tlogv(fmt, args...)                                                                             \
    do {                                                                                                \
        if (TEE_LOG_MASK == LEVEL_VERBOSE) {                                                            \
            printf("[%s]%s[LINE:%d] " fmt, LOG_TAG, TAG_VERBOSE, __LINE__, ##args);                     \
            LogPrint(LEVEL_VERBOSE, "[%s]%s[LINE:%d] " fmt, LOG_TAG, TAG_VERBOSE, __LINE__, ##args);    \
        }                                                                                               \
    } while (0)

#define tlogd(fmt, args...)                                                                         \
    do {                                                                                            \
        if (TEE_LOG_MASK <= LEVEL_DEBUG) {                                                          \
            printf("[%s]%s[LINE:%d] " fmt, LOG_TAG, TAG_DEBUG, __LINE__, ##args);                   \
            LogPrint(LEVEL_DEBUG, "[%s]%s[LINE:%d] " fmt, LOG_TAG, TAG_DEBUG, __LINE__, ##args);    \
        }                                                                                           \
    } while (0)

#define tlogi(fmt, args...)                                                                         \
    do {                                                                                            \
        if (TEE_LOG_MASK <= LEVEL_INFO) {                                                           \
            printf("[%s]%s[LINE:%d] " fmt, LOG_TAG, TAG_INFO, __LINE__, ##args);                    \
            LogPrint(LEVEL_INFO, "[%s]%s[LINE:%d] " fmt, LOG_TAG, TAG_INFO, __LINE__, ##args);      \
        }                                                                                           \
    } while (0)

#define tlogw(fmt, args...)                                                                         \
    do {                                                                                            \
        if (TEE_LOG_MASK <= LEVEL_WARN) {                                                           \
            printf("[%s]%s[LINE:%d] " fmt, LOG_TAG, TAG_WARN, __LINE__, ##args);                    \
            LogPrint(LEVEL_WARN, "[%s]%s[LINE:%d] " fmt, LOG_TAG, TAG_WARN, __LINE__, ##args);      \
        }                                                                                           \
    } while (0)

#define tloge(fmt, args...)                                                                         \
    do {                                                                                            \
        if (TEE_LOG_MASK <= LEVEL_ERROR) {                                                          \
            printf("[%s]%s[LINE:%d] " fmt, LOG_TAG, TAG_ERROR, __LINE__, ##args);                   \
            LogPrint(LEVEL_ERROR, "[%s]%s[LINE:%d] " fmt, LOG_TAG, TAG_ERROR, __LINE__, ##args);    \
        }                                                                                           \
    } while (0)

#endif

