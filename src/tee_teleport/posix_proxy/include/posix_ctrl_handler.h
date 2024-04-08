/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: posix proxy handler definations
 */
#ifndef REE_POSIX_CTRL_HANDLER_H
#define REE_POSIX_CTRL_HANDLER_H

#include <stdint.h>
#include <stdlib.h>

enum PosixCtrlCallTypes {
    POSIX_CALL_REGEISTER_CTRL_TASKLET       = 0,
    POSIX_CALL_REGEISTER_DATA_TASKLET       = 1,
    POSIX_CALL_UNREGEISTER_ALL_TASKLET      = 2,
};

long PosixCtrlTaskletCallHandler(uint8_t *membuf, void *priv);

#endif
