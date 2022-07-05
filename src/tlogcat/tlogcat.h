/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef TLOGCAT_H
#define TLOGCAT_H

#include <stdint.h>

#ifdef LOG_TEEOS_TAG
#undef LOG_TEEOS_TAG
#endif
#define LOG_TEEOS_TAG      "teeos"
#define LOG_ITEM_MAX_LEN   1024
#define LEVEL_ERROR        0
#define LEVEL_WARNING      1
#define LEVEL_INFO         2
#define LEVEL_DEBUG        3
#define LEVEL_VERBO        4
#define TOTAL_LEVEL_NUMS   5
#define LOG_FILE_INDEX_MAX 4U

#define NEVER_USED_LEN     32U
#define TEE_UUID_LEN       16U
#define ITEM_RESERVED_LEN  1U

/* 64 byte head + user log */
struct LogItem {
    uint8_t neverUsed[NEVER_USED_LEN];
    uint16_t magic;
    uint16_t reserved0;
    uint32_t serialNo;
    uint16_t logRealLen;    /* log real len */
    uint16_t logBufferLen; /* log buffer's len, multiple of 32 bytes */
    uint8_t uuid[TEE_UUID_LEN];
    uint8_t logSourceType;
    uint8_t reserved[ITEM_RESERVED_LEN];
    uint8_t logLevel;
    uint8_t newLine; /* '\n' char, easy viewing log in bbox.bin file */
    uint8_t logBuffer[0];
};

#define CLOCK_SEG_NODE_LEN 8U
struct TeeUuid {
    uint32_t timeLow;
    uint16_t timeMid;
    uint16_t timeHiAndVersion;
    uint8_t clockSeqAndNode[CLOCK_SEG_NODE_LEN];
};
#endif
