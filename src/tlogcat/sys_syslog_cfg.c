/* 
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <syslog.h>
#include <stdbool.h>
#include <securec.h>
#include "tlogcat.h"
#include "sys_log_api.h"

static char g_logItemBuffer[LOG_ITEM_MAX_LEN];

void OpenTeeLog(void)
{
    openlog(LOG_TEEOS_TAG, LOG_CONS | LOG_NDELAY, LOG_USER);
}

void CloseTeeLog(void)
{
    closelog();
}

static void TeeSyslogPrint(const struct LogItem *logItem, const char *logItemBuffer)
{
    uint8_t logLevel = logItem->logLevel;
    uint8_t syslogLevel[TOTAL_LEVEL_NUMS] = {LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG, LOG_DEBUG};

    if (logLevel < TOTAL_LEVEL_NUMS) {
        logLevel = syslogLevel[logLevel];
    } else {
        logLevel = LOG_INFO;
    }

    (void)syslog(LOG_USER | logLevel, "index: %u: %s", logItem->serialNo, logItemBuffer);
}

void LogWriteSysLog(const struct LogItem *logItem, bool isTa)
{
    (void)isTa;
    if (logItem == NULL || logItem->logRealLen == 0) {
        return;
    }
    if (memcpy_s(g_logItemBuffer, LOG_ITEM_MAX_LEN, logItem->logBuffer, logItem->logRealLen) == EOK) {
        g_logItemBuffer[logItem->logRealLen - 1] = '\0';
        TeeSyslogPrint(logItem, (const char *)g_logItemBuffer);
    }
}
