/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * iTrustee licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef _TEE_CLIENT_EXT_API_H_
#define _TEE_CLIENT_EXT_API_H_

#include "tee_client_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CA register an agent for TA communicate with CA
 *
 * @param agentId [IN] user defined, do not conflict with other agent
 * @param devFd [OUT] TEE driver fd
 * @param buffer [OUT] shared memory between CA&TA, size is 4K
 *
 * @return TEEC_SUCCESS operation success
 * @return TEEC_ERROR_GENERIC error happened
 */
TEEC_Result TEEC_EXT_RegisterAgent(uint32_t agentId, int *devFd, void **buffer);

/*
 * CA wait event from TA
 * when call this interface, CA thread will block until TA send a msg
 *
 * @param agentId [IN] user registered agent
 * @param devFd [IN] TEE driver fd
 *
 * @return TEEC_SUCCESS CA receive msg from TA success
 * @return TEEC_ERROR_GENERIC error happened
 */
TEEC_Result TEEC_EXT_WaitEvent(uint32_t agentId, int devFd);

/*
 * CA send response to TA
 *
 * @param agentId [IN] user registered agent
 * @param devFd [IN] TEE driver fd
 *
 * @return TEEC_SUCCESS operation success
 * @return TEEC_ERROR_GENERIC error happened
 */
TEEC_Result TEEC_EXT_SendEventResponse(uint32_t agentId, int devFd);

/*
 * CA unregister an agent
 *
 * @param agentId [IN] user registered agent
 * @param devFd [IN] TEE driver fd
 * @param buffer [IN] shared mem between CA&TA, TEE will release this buffer and set it to NULL
 *
 * @return TEEC_SUCCESS operation success
 * @return OTHERS error happened
 */
TEEC_Result TEEC_EXT_UnregisterAgent(uint32_t agentId, int devFd, void **buffer);

/*
 * CA sends a secfile to TEE
 *
 * @param path [IN] path of the secfile
 * @param session [IN] session beturn CA&TA
 *
 * @return TEEC_SUCCESS operation success
 * @return OTHERS error happened
 */
TEEC_Result TEEC_SendSecfile(const char *path, TEEC_Session *session);

/*
 * get version of TEE
 *
 * @return version info of TEE
 */
uint32_t TEEC_GetTEEVersion();

/*
 * Function:     TEEC_EXT_ProcEncRoT
 * Description:  This interface is a cross-platform ca function, invoke TA to handle auth token key over this session
 * Parameters:   session: a pointer to a Session structure.
 *               authType: a enum value defined in enum hw_authenticator_type_t
 *               invkCmdId: a cmd id to info TA how to handle this request.
 * Return:       TEEC_SUCCESS: success
 *               other:        failure
 */
TEEC_Result TEEC_EXT_ProcEncRoT(const TEEC_Session *session, uint32_t authType, uint32_t invkCmdId);

#ifdef __cplusplus
}
#endif

#endif
