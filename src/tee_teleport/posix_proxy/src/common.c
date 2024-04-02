/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All ritghts reserved.
 * Description: Common utils for xtasklet
 */
#include <common.h>

#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

int GetTimestampUs(unsigned long *usec)
{
    int ret = 0;
    if (usec == NULL) {
        ret = -EINVAL;
        ERR("output ptr is NULL\n");
        goto end;
    }
    struct timeval tv;
    ret = gettimeofday(&tv, NULL);
    if (ret == -1) {
        ret = -errno;
        ERR("get time of day failed, %s\n", strerror(-ret));
        goto end;
    }
    *usec = tv.tv_sec * (S) + tv.tv_usec;
end:
    return ret;
}
