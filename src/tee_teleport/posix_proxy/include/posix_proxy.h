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
#ifndef TELEPORT_POSIX_PROXY_H
#define TELEPORT_POSIX_PROXY_H

#include <stdio.h>

void SetDataTaskletThreadConcurrency(long concurrency);
void SetDataTaskletBufferSize(long size);

int PosixProxyInit(void);
void PosixProxyDestroy(void);

void PosixProxySetDevFD(int devFD);
int PosixProxyRegisterCtrlTasklet(void);
int PosixProxyRegisterDataTasklet(void);
int PosixProxyUnregisterAllTasklet(void);

#endif