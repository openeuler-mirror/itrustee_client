/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2023. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef LIBTEEC_EXT_API_H
#define LIBTEEC_EXT_API_H

#include <stdint.h>
#include "tee_auth_common.h"

#ifdef CONFIG_PATH_NAMED_SOCKET
#define TC_NS_SOCKET_NAME        CONFIG_PATH_NAMED_SOCKET
#else
#define TC_NS_SOCKET_NAME        "#tc_ns_socket"
#endif

int CaDaemonConnectWithCaInfo(const CaAuthInfo *caInfo, int cmd, const TEEC_XmlParameter *halXmlPtr);

#endif
