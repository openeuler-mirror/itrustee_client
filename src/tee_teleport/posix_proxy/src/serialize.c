/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include "common.h"
#include "securec.h"
#include "tee_log.h"
#include "serialize.h"

static bool OverflowCheck(uint32_t a, uint32_t b)
{
    if (a > UINT32_MAX - b)
        return true;
    return false;
}

int CalculateBuffSize(size_t *retSize, uint32_t argCount, ...)
{
    int ret = 0;
    va_list arg_ptr;
    uint32_t buffSize = 0;
    DataType_t argType = 0;
    size_t argBuffSize = 0;
    uint64_t argValue = 0;
    void *argBuffAddr = NULL;

    buffSize += sizeof(argCount);
    va_start(arg_ptr, argCount);
    for (uint32_t i = 0; i < argCount; i++) {
        argType = va_arg(arg_ptr, DataType_t);
        switch (argType) {
            case INTEGERTYPE:
                argValue = va_arg(arg_ptr, uint64_t);
                (void)argValue;
                if (OverflowCheck(buffSize, sizeof(IntegerStorage_t))) {
                    ERR("too large argBuff size\n");
                    ret = -EINVAL;
                    goto end;
                } else {
                    buffSize += sizeof(IntegerStorage_t);
                }
                break;
            case POINTTYPE:
                argBuffAddr = va_arg(arg_ptr, void *);
                (void)argBuffAddr;
                argBuffSize = va_arg(arg_ptr, size_t);
                if (OverflowCheck(sizeof(PointStorage_t), argBuffSize) ||
                    OverflowCheck(buffSize, sizeof(PointStorage_t) + argBuffSize)) {
                    ERR("too large argBuff size\n");
                    ret = -EINVAL;
                    goto end;
                } else {
                    buffSize += sizeof(PointStorage_t) + argBuffSize;
                }
                break;
            default:
                ERR("unknown param type\n");
                ret = -EINVAL;
                goto end;
        }
    }
end:
    va_end(arg_ptr);
    *retSize = buffSize;
    return ret;
}

static int SerializeParamValidCheck(void *buff, uint32_t buffSize)
{
    if (buff == NULL || buffSize < sizeof(uint32_t)) {
        ERR("bad paramters for buf to serialize\n");
        return -EINVAL;
    }
    return 0;
}

static int SerializeInterBuffValidCheck(uint32_t buffTotalSize, uint32_t offset)
{
    if (buffTotalSize < offset || buffTotalSize - offset < sizeof(IntegerStorage_t)) {
        ERR("too small buffer for INTEGERTYPE\n");
        return -ENOMEM;
    }
    return 0;
}

static int SerializePointBuffValidCheck(uint32_t buffTotalSize, uint32_t offset)
{
    if (buffTotalSize < offset || buffTotalSize - offset < sizeof(PointStorage_t)) {
        ERR("too small buffer for POINTTYPE\n");
        return -ENOMEM;
    }
    return 0;
}

static int InterTypeSerialize(void *destBuff, uint32_t destBuffSize, uint32_t byteOffset, uint64_t inputArgValue64)
{
    if (SerializeInterBuffValidCheck(destBuffSize, byteOffset) != 0) {
        return -ENOMEM;
    }
    IntegerStorage_t *interParam = (IntegerStorage_t *)((uint8_t *)destBuff + byteOffset);
    interParam->type = (uint64_t)INTEGERTYPE;
    interParam->value = inputArgValue64;
    return 0;
}

static int PointTypeSerialize(void *destBuff, uint32_t destBuffSize, uint32_t byteOffset,
                              const void *inputArgBuffAddr, uint64_t inputArgBuffSize)
{
    if (SerializePointBuffValidCheck(destBuffSize, byteOffset) != 0) {
        return -ENOMEM;
    }
    PointStorage_t *pointParam = (PointStorage_t *)((uint8_t *)destBuff + byteOffset);
    pointParam->type = (uint64_t)POINTTYPE;
    if (inputArgBuffSize != 0 &&
        memcpy_s(pointParam->buffer, destBuffSize - byteOffset - sizeof(PointStorage_t),
        inputArgBuffAddr, inputArgBuffSize) != 0) {
        ERR("too small buffer for POINTTYPE buf\n");
        return -ENOMEM;
    }
    pointParam->size = inputArgBuffSize;
    return 0;
}

int Serialize(uint32_t argCount, void *destBuff, uint32_t destBuffSize, ...)
{
    int ret = 0;
    va_list arg_ptr;
    DataType_t argType = 0;
    uint64_t argValue64 = 0;
    uint64_t argBuffSize = 0;
    void *argBuffAddr = NULL;

    if (SerializeParamValidCheck(destBuff, destBuffSize)) {
        return -EINVAL;
    }

    *(uint32_t *)destBuff = argCount;
    uint32_t byteOffset = sizeof(uint32_t);

    va_start(arg_ptr, destBuffSize);
    for (uint32_t i = 0; i < argCount; i++) {
        argType = va_arg(arg_ptr, DataType_t);

        switch (argType) {
            case INTEGERTYPE:
                argValue64 = va_arg(arg_ptr, uint64_t);
                ret = InterTypeSerialize(destBuff, destBuffSize, byteOffset, argValue64);
                if (ret != 0) {
                    goto end;
                }
                byteOffset += sizeof(IntegerStorage_t);
                break;
            case POINTTYPE:
                argBuffAddr = va_arg(arg_ptr, void *);
                argBuffSize = va_arg(arg_ptr, size_t);
                if (argBuffSize != 0 && argBuffAddr == NULL) {
                    ERR("bad parameters for POINTTYPE\n");
                    ret = -EINVAL;
                    goto end;
                }
                ret = PointTypeSerialize(destBuff, destBuffSize, byteOffset, argBuffAddr, argBuffSize);
                if (ret != 0) {
                    goto end;
                }
                byteOffset += sizeof(PointStorage_t) + argBuffSize;
                break;
            default:
                ERR("unknown param type\n");
                ret = -EINVAL;
                goto end;
        }
    }
end:
    va_end(arg_ptr);
    return ret;
}

static int DeSerializeParamValidCheck(uint32_t argCount, void *buff, uint32_t buffSize)
{
    if (buff == NULL || buffSize < sizeof(uint32_t)) {
        ERR("bad paramters for buf to deserialize\n");
        return -EINVAL;
    }

    uint32_t retArgCount = *(uint32_t *)buff;
    if (retArgCount != argCount) {
        ERR("arg count is not match with deserialize input\n");
        return -EINVAL;
    }
    return 0;
}

static int ParamTypeCheck(DataType_t src, DataType_t dest)
{
    if (src != dest) {
        if (dest == INTEGERTYPE) {
            ERR("arg type is not match with INTEGERTYPE, src %d, dest %d\n", src, dest);
        } else if (dest == POINTTYPE) {
            ERR("arg type is not match with POINTTYPE, src %d, dest %d\n", src, dest);
        } else {
            ERR("unknown param type to judge, src %d, dest %d\n", src, dest);
        }
        return -EINVAL;
    }
    return 0;
}

static int InterTypeDeserialize(void *srcBuff, uint32_t srcBuffSize, uint32_t byteOffset, uint64_t *retArgValue64Ref)
{
    if (SerializeInterBuffValidCheck(srcBuffSize, byteOffset) != 0) {
        return -ENOMEM;
    }
    IntegerStorage_t *interParam = (IntegerStorage_t *)((uint8_t *)srcBuff + byteOffset);
    if (ParamTypeCheck((DataType_t)interParam->type, INTEGERTYPE) != 0) {
        return -EINVAL;
    }

    *retArgValue64Ref = interParam->value;
    return 0;
}

static int PointTypeDeserialize(void *srcBuff, uint32_t srcBuffSize, uint32_t byteOffset,
                                void **retArgPointRef, uint64_t *retArgPointSize)
{
    if (SerializePointBuffValidCheck(srcBuffSize, byteOffset) != 0) {
        return -ENOMEM;
    }
    PointStorage_t *pointParam = (PointStorage_t *)((uint8_t *)srcBuff + byteOffset);
    if (ParamTypeCheck((DataType_t)pointParam->type, POINTTYPE) != 0) {
        return -EINVAL;
    }
    if (srcBuffSize < byteOffset + sizeof(PointStorage_t) + pointParam->size) {
        ERR("too small buffer for POINTTYPE\n");
        return -ENOMEM;
    }
    if (pointParam->size != 0) {
        *retArgPointRef = (void *)((uint8_t *)srcBuff + byteOffset + sizeof(PointStorage_t));
    } else {
        *retArgPointRef = NULL;
    }
    *retArgPointSize = pointParam->size;
    return 0;
}

int DeSerialize(uint32_t argCount, void *srcBuff, uint32_t srcBuffSize, ...)
{
    int ret = 0;
    va_list arg_ptr;
    DataType_t argType = 0;
    uint64_t *argValue64Ref = NULL;
    void **argPointRef = NULL;

    if (DeSerializeParamValidCheck(argCount, srcBuff, srcBuffSize)) {
        return -EINVAL;
    }

    uint32_t byteOffset = sizeof(uint32_t);

    va_start(arg_ptr, srcBuffSize);
    for (uint32_t i = 0; i < argCount; i++) {
        argType = va_arg(arg_ptr, DataType_t);
        switch (argType) {
            case INTEGERTYPE:
                argValue64Ref = va_arg(arg_ptr, uint64_t *);
                if (argValue64Ref == NULL) {
                    ERR("bad parameters for INTEGERTYPE\n");
                    ret = -EINVAL;
                    goto end;
                }
                ret = InterTypeDeserialize(srcBuff, srcBuffSize, byteOffset, argValue64Ref);
                if (ret != 0) {
                    goto end;
                }
                byteOffset += sizeof(IntegerStorage_t);
                break;
            case POINTTYPE:
                argPointRef = va_arg(arg_ptr, void **);
                if (argPointRef == NULL) {
                    ERR("bad parameters for POINTTYPE\n");
                    ret = -EINVAL;
                    goto end;
                }
                uint64_t argPointSize = 0;
                ret = PointTypeDeserialize(srcBuff, srcBuffSize, byteOffset, argPointRef, &argPointSize);
                if (ret != 0) {
                    goto end;
                }
                byteOffset += sizeof(PointStorage_t) + argPointSize;
                break;
            default:
                ERR("unknown param type\n");
                ret = -EINVAL;
                goto end;
        }
    }
end:
    va_end(arg_ptr);
    return ret;
}