/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2020. All rights reserved.
 * Description: tlogcat code head file.
 * Create: 2019-12-12
 */
#ifndef TLOGCAT_H
#define TLOGCAT_H

#include <stdint.h>

#define LOG_FILE_INDEX_MAX 4U

#define NEVER_USED_LEN    32U
#define TEE_UUID_LEN      16U
#define ITEM_RESERVED_LEN 2U

/* 64 byte head + user log */
struct LogItem {
    uint8_t neverUsed[NEVER_USED_LEN];
    uint16_t magic;
    uint16_t reserved0;
    uint32_t serialNo;
    int16_t logRealLen;    /* log real len */
    uint16_t logBufferLen; /* log buffer's len, multiple of 32 bytes */
    uint8_t uuid[TEE_UUID_LEN];
    uint8_t logSourceType;
    uint8_t reserved[ITEM_RESERVED_LEN];
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
