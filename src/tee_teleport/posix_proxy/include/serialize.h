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
#ifndef TELEPORT_POSIX_SERIALIZE_H
#define TELEPORT_POSIX_SERIALIZE_H

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

typedef enum DataType {
    INTEGERTYPE = 17, /* all Integer type transfer in 64-bit format */
    POINTTYPE
} DataType_t;

typedef struct PointStorage {
    uint64_t type;
    uint64_t size;
    uint8_t buffer[0];
} PointStorage_t;

typedef struct IntegerStorage {
    uint64_t type;
    uint64_t value;
} IntegerStorage_t;

/* params: retSize, argCount, { POINTTYPE, point, buffSize }, { INTEGERTYPE, Value64 }, ... */
int CalculateBuffSize(size_t *retSize, uint32_t argCount, ...);

/* params: argCount, destBuff, destBuffSize, { POINTTYPE, point, buffSize }, { INTEGERTYPE, Value64 }, ... */
int Serialize(uint32_t argCount, void *destBuff, uint32_t destBuffSize, ...);

/* params: argCount, destBuff, destBuffSize, { POINTTYPE, point, buffSize }, { INTEGERTYPE, Value64 }, ... */
int Deserialize(uint32_t argCount, void *srcBuff, uint32_t srcBuffSize, ...);

#endif