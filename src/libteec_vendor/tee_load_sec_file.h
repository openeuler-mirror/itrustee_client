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

#ifndef __TEE_LOAD_SEC_FILE_H_
#define __TEE_LOAD_SEC_FILE_H_

#include <sys/ioctl.h> /* for ioctl */
#include "tee_client_api.h"
#include "tc_ns_client.h"

#define H_OFFSET 32
#define MAX_BUFFER_LEN (8 * 1024 * 1024)

int32_t LoadSecFile(int tzFd, FILE *fp, enum SecFileType fileType, const TEEC_UUID *uuid, int32_t *errCode);

#endif
