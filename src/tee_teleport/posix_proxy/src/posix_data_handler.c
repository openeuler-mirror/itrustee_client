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
#include <posix_data_handler.h>
#include <errno.h>
#include <stdint.h>
#include <common.h>

static long PosixFuncCall(struct PosixFunc *func, struct PosixProxyParam *param, int *err)
{
    long ret = 0;
    if (func->funcPtr == NULL) {
        ret = -1;
        *err = ENOSYS;
        ERR("function not implemented\n");
        goto end;
    }
    ret = func->funcPtr(param);
    *err = errno;
end:
    return ret;
}

long PosixDataTaskletCallHandler(uint8_t *membuf, void *priv)
{
    struct PosixCall *call = (struct PosixCall *)membuf;
    long ret = 0;
    struct PosixFunc *funcs = NULL;
    size_t funcsSz;
    switch (call->type) {
        case POSIX_CALL_FILE:
            funcs = POSIX_FUNCS_GET(POSIX_FILE);
            funcsSz = POSIX_FUNCS_SIZE(POSIX_FILE);
            break;
        case POSIX_CALL_NETWORK:
            funcs = POSIX_FUNCS_GET(POSIX_NETWORK);
            funcsSz = POSIX_FUNCS_SIZE(POSIX_NETWORK);
            break;
        case POSIX_CALL_OTHER:
            funcs = POSIX_FUNCS_GET(POSIX_OTHER);
            funcsSz = POSIX_FUNCS_SIZE(POSIX_OTHER);
            break;
        default:
            ERR("invalid posix call type: %d\n", call->type);
            ret = 1;
            goto end;
    }

    if (call->func > funcsSz) {
        ERR("invalid function number\n");
        ret = 1;
        goto end;
    }

    struct PosixFunc *func = &funcs[call->func];
    struct PosixProxyParam param = {
        .args = call->args,
        .argsSz = call->argsSz,
        .argsCnt = func->argsCnt,
        .ctx = priv
    };
    ret = PosixFuncCall(func, &param, &call->err);
    if (ret != 0 && call->err != 0) {
        DBG("posix function call failed, type: %d, func: %d\n", call->type, call->func);
    }
end:
    return ret;
}
