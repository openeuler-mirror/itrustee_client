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

#include <stdio.h>
#include <stdint.h>

#ifdef CONFIG_CUSTOM_LOGGING
#include <time.h>
#include <sys/time.h>
#include "securec.h"

#define FORMAT_TIME_LEN 128
#define NSEC_PER_USEC   1000

static int GetLogTimeInfo(char *logTimeInfo, size_t bufLen)
{
    time_t now;
    struct timeval tv;
    struct tm t;
    int count = 0;

    if (logTimeInfo == NULL || bufLen == 0 || bufLen > FORMAT_TIME_LEN) {
        fprintf(stderr, "logTimeInfo is null or bufLen is incorrect\n");
        return -1;
    }
    if (time(&now) == (time_t)-1) {
        fprintf(stderr, "can't init time\n");
        return -1;
    }
    if (localtime_r(&now, &t) == NULL) {
        fprintf(stderr, "localtime is NULL\n");
        return -1;
    }
    if (gettimeofday(&tv, NULL) != 0) {
        fprintf(stderr, "can't get time of day\n");
        return -1;
    }

    count = snprintf_s(logTimeInfo, bufLen, bufLen - 1,
        "[%02d/%02d %02d:%02d:%02d.%03d]",
        (t.tm_mon + 1), t.tm_mday, t.tm_hour, t.tm_min,
        t.tm_sec, (tv.tv_usec / NSEC_PER_USEC));
    if (count < 0) {
        fprintf(stderr, "the length of the print time is not as expected\n");
        return -1;
    }

    return 0;
}

void LogPrint(uint8_t logLevel, const char *fmt, ...)
{
    (void)logLevel;

    va_list ap;
    FILE *fp;
    int size;
    char logTimeInfo[FORMAT_TIME_LEN] = { 0 };

    if (fmt == NULL) {
        fprintf(stderr, "fmt is NULL\n");
        return;
    }

    if (GetLogTimeInfo(logTimeInfo, sizeof(logTimeInfo)) != 0) {
        fprintf(stderr, "get time info failed\n");
        return;
    }

    fp = fopen(CONFIG_CUSTOM_LOGGING, "ab+");
    if (fp == NULL) {
        fprintf(stderr, "open %s failed\n", CONFIG_CUSTOM_LOGGING);
        return;
    }

    size = (int)fwrite(logTimeInfo, 1, strnlen(logTimeInfo, FORMAT_TIME_LEN), fp);
    if ((size_t)size != strnlen(logTimeInfo, FORMAT_TIME_LEN)) {
        fprintf(stderr, "write file failed, errno=%d\n", errno);
        (void)fclose(fp);
        return;
    }

    va_start(ap, fmt);
    size = vfprintf(fp, fmt, ap);
    va_end(ap);
    if (size < 0) {
        fprintf(stderr, "write file failed, errno=%d\n", errno);
        (void)fclose(fp);
        return;
    }

    (void)fclose(fp);
}

#else
void LogPrint(uint8_t logLevel, const char *fmt, ...)
{
    (void)logLevel;
    (void)fmt;
}
#endif
