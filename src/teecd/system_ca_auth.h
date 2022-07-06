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

#ifndef SYSTEM_CA_AUTH_H
#define SYSTEM_CA_AUTH_H

#include <sys/socket.h>
#include "tee_auth_common.h"

int GetLoginInfoHidl(const struct ucred *cr, const CaRevMsg *caRevInfo,
                     int fd, uint8_t *buf, unsigned int bufLen);
int RecvCaMsg(int socket, CaRevMsg *caInfo);

#endif
