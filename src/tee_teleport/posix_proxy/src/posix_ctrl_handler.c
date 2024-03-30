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
#include <posix_ctrl_handler.h>
#include <posix_data_handler.h>
#include <posix_proxy.h>
#include <errno.h>
#include <stdint.h>
#include <common.h>

long PosixCtrlTaskletCallHandler(uint8_t *membuf, void *priv)
{
    (void)priv;
    struct PosixCall *call = (struct PosixCall *)membuf;
    long ret = 0;

    switch (call->type) {
        case POSIX_CALL_REGEISTER_CTRL_TASKLET:
            ret = PosixProxyRegisterCtrlTasklet();
            break;
        case POSIX_CALL_REGEISTER_DATA_TASKLET:
            ret = PosixProxyRegisterDataTasklet();
            break;
        case POSIX_CALL_UNREGEISTER_ALL_TASKLET:
            ret = PosixProxyUnregisterAllTasklet();
            break;
        default:
            ret = 1;
            ERR("invalid posix ctrl call type: %d\n", call->type);
            goto end;
    }

    bool is_failed = (ret <= 0 && call->type == POSIX_CALL_REGEISTER_CTRL_TASKLET) ||
                    (ret != 0 && call->type != POSIX_CALL_REGEISTER_CTRL_TASKLET);
    if (is_failed) {
        ERR("posix ctrl call failed, type: %d, ret: %d\n", call->type, (int)ret);
    }
end:
    return ret;
}