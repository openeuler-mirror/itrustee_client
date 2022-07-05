/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "tee_client_api.h"
#include "tee_log.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "libteec_vendor"

/*
 * Function:     TEEC_EXT_ProcEncRoT
 * Description:  This interface is a cross-platform ca function, invoke TA to handle auth token key over this session
 * Parameters:   session: a pointer to a Session structure.
 *               authType: a enum value defined in enum hw_authenticator_type_t
 *               invkCmdId: a cmd id to info TA how to handle this request.
 * Return:       TEEC_SUCCESS: success
 *               other:        failure
 */
TEEC_Result TEEC_EXT_ProcEncRoT(const TEEC_Session *session, uint32_t authType, uint32_t invkCmdId)
{
    (void)session;
    (void)authType; /* unused on this platform */
    (void)invkCmdId;
    tlogi("TEEC_EXT_ProcEncRoT is not support on this platform\n");
    return TEEC_ERROR_NOT_SUPPORTED;
}
