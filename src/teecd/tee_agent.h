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
#ifndef LIBTEEC_TEE_AGENT_H
#define LIBTEEC_TEE_AGENT_H

#define TRANS_BUFF_SIZE           (4 * 1024) /* agent transfer share buffer size */
#define FS_TRANS_BUFF_SIZE_CCOS   (512 * 1024) /* agent transfer share buffer size on ccos */

int AgentInit(unsigned int id, unsigned int bufferSize, void **control);
void AgentExit(unsigned int id, int fd);
int ProcessAgentInit(void);
void ProcessAgentThreadCreate(void);
void ProcessAgentThreadJoin(void);
void ProcessAgentExit(void);
void TrySyncSysTimeToSecure(void);

#endif
