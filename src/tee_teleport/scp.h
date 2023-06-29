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

#ifndef TEE_TB_SCP_H
#define TEE_TB_SCP_H

#include <stdint.h>
#include <stdbool.h>

int TeeScp(int mode, const char *srcPath, const char *dstPath, const char *filename, uint32_t sessionID);
int TeeInstall(int mode, const char *filename, uint32_t *sessionID);
int TeeUninstall(int mode);
int TeeList(int mode);
int TeeDelete(const char *filename, uint32_t sessionID);
int TeeQuery(const char *filename, uint32_t sessionID, bool *exist);
int TeeDestroy(uint32_t sessionID);

#endif
