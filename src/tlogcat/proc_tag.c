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

#include "proc_tag.h"

#include <errno.h>
#include <securec.h>
#include <sys/types.h>
#include <tee_client_list.h>
#include "tlogcat.h"
#include "tee_log.h"
#include "tlogcat.h"

struct DriverTagInfo {
    uint8_t driverType;
    char *tagName;
    struct ListNode tagNode;
};

static LIST_DECLARE(g_driverTagList);

#define COMMON_DRV_SOURCE  0
#define SERVICE_NAME_MAX   100

/* Description-"[*-%d]": * is ta task name, and %d is sessionid&0xffff. */
#define TA_NAME_EXTENSION_LEN (SERVICE_NAME_MAX + 8)

struct TaTagInfo {
    struct TeeUuid uuid;
    char *taTagName;
    struct ListNode taTagNode;
};

static LIST_DECLARE(g_taTagList);

static char *QueryTaTagNode(const struct TeeUuid *uuid)
{
    struct TaTagInfo *tagInfo = NULL;
    struct ListNode *ptr = NULL;

    if (LIST_EMPTY(&g_taTagList)) {
        return NULL;
    }

    LIST_FOR_EACH(ptr, &g_taTagList) {
        tagInfo = CONTAINER_OF(ptr, struct TaTagInfo, taTagNode);
        if ((tagInfo == NULL) || (memcmp(&(tagInfo->uuid), uuid, sizeof(*uuid)) != 0)) {
            continue;
        }
        return tagInfo->taTagName;
    }

    return NULL;
}

static char *QueryDriverTagNode(uint8_t driverType)
{
    struct DriverTagInfo *tagInfo = NULL;
    struct ListNode *ptr = NULL;

    if (LIST_EMPTY(&g_driverTagList)) {
        return NULL;
    }

    LIST_FOR_EACH(ptr, &g_driverTagList) {
        tagInfo = CONTAINER_OF(ptr, struct DriverTagInfo, tagNode);
        if ((tagInfo == NULL) || (tagInfo->driverType != driverType)) {
            continue;
        }
        return tagInfo->tagName;
    }

    return NULL;
}

static void GetMiddleChar(const char *startStr, const char *endStr, char **middleStr)
{
    char *p = NULL;
    uint32_t subLen;
    uint32_t i;

    *middleStr = NULL;

    /*
     * Find the position of the tail '-' character.
     * Reverses the position of the '-' character from the position of the ']' character.
     */
    p = (char *)endStr;
    subLen = endStr - startStr + 1;
    for (i = 0; i < subLen; i++) {
        if (*p == '-') {
            *middleStr = p;
            break;
        }
        p--;
    }
}

static uint32_t GetLogItemTag(const struct LogItem *logItem, char *tagStr, uint32_t tagStrLen)
{
    char *startStr = NULL;
    char *middleStr = NULL;
    char *endStr = NULL;
    uint32_t tagLen;

    /* Ta log tag name is [taskname[-sessionid]] */
    startStr = strchr((char *)logItem->logBuffer, '[');
    endStr = strchr((char *)logItem->logBuffer, ']');

    bool condition = ((startStr == NULL) || (endStr == NULL) || (endStr - startStr <= 1) ||
                      (endStr - startStr + 1 > TA_NAME_EXTENSION_LEN));
    if (condition) {
        tloge("Wrong format log buffer\n");
        return 0;
    }

    /* Find the position of the tail '-' character. */
    GetMiddleChar(startStr, endStr, &middleStr);
    condition = ((middleStr != NULL) && (middleStr - startStr - 1 > SERVICE_NAME_MAX));
    if (condition) {
        tloge("Too long task name\n");
	return 0;
    }

    tagLen = ((middleStr != NULL) ? (middleStr - startStr - 1) : (endStr - startStr - 1));
    if (memcpy_s(tagStr, tagStrLen, startStr + 1, tagLen) != EOK) {
        tloge("Memcpy tag str is failed\n");
	return 0;
    }

    tagLen = ((tagStrLen == tagLen) ? (tagStrLen - 1) : tagLen);
    tagStr[tagLen] = '\0';
    return tagLen;
}

static int32_t InsertTaTagNode(char *tagName, uint32_t tagLen, const char **logTag, const struct LogItem *logItem)
{
    struct TaTagInfo *tagInfo = NULL;

    tagInfo = malloc(sizeof(*tagInfo));
    if (tagInfo == NULL) {
        tloge("Malloc ta tag node failed\n");
	return -1;
    }

    errno_t rc = memcpy_s(&tagInfo->uuid, sizeof(tagInfo->uuid),
        (struct TeeUuid *)logItem->uuid, sizeof(logItem->uuid));
    if (rc != EOK) {
        tloge("Memcpy_s uuid error %d\n", rc);
	goto FREE_TAG;
    }

    tagName[tagLen - 1] = '\0';
    tagInfo->taTagName = tagName;
    *logTag = tagName;

    ListInit(&(tagInfo->taTagNode));
    ListInsertTail(&g_taTagList, &(tagInfo->taTagNode));
    /* cppcheck-suppress */
    return 0;

FREE_TAG:
    free(tagInfo);
    tagInfo = NULL;
    return -1;
}

static int32_t InsertDriverTagNode(char *tagName, uint32_t tagLen, const char **logTag, const struct LogItem *logItem)
{
    struct DriverTagInfo *tagInfo = NULL;

    tagInfo = malloc(sizeof(*tagInfo));
    if (tagInfo == NULL) {
        tloge("Malloc driver tag node failed\n");
	return -1;
    }

    tagName[tagLen - 1] = '\0';
    tagInfo->tagName = tagName;
    *logTag = tagName;

    tagInfo->driverType = logItem->logSourceType;

    ListInit(&(tagInfo->tagNode));
    ListInsertTail(&g_driverTagList, &(tagInfo->tagNode));
    /* cppcheck-suppress */
    return 0;
}

static char *QueryTagNode(bool isTa, const struct LogItem *logItem)
{
    if (isTa) {
        return QueryTaTagNode((struct TeeUuid *)logItem->uuid);
    } else {
        return QueryDriverTagNode(logItem->logSourceType);
    }
}

static int32_t InsertTagNode(bool isTa, char *tagName, uint32_t tagLen,
                             const char **logTag, const struct LogItem *logItem)
{
    if (isTa) {
        return InsertTaTagNode(tagName, tagLen, logTag, logItem);
    } else {
        return InsertDriverTagNode(tagName, tagLen, logTag, logItem);
    }
}

static bool GetTagName(bool isTa, const struct LogItem *logItem, const char **logTag)
{
    char logItemTag[SERVICE_NAME_MAX + 1] = {0};
    char *tagName = QueryTagNode(isTa, logItem);
    if (tagName != NULL) {
        *logTag = tagName;
        return true;
    }

    uint32_t taTagTempLen = GetLogItemTag(logItem, logItemTag, (uint32_t)sizeof(logItemTag));
    if (taTagTempLen == 0) {
        return false;
    }

    uint32_t tagLen = taTagTempLen + strlen("teeos-") + 1;
    tagName = malloc(tagLen);
    if (tagName == NULL) {
        return false;
    }

    int32_t ret = snprintf_s(tagName, tagLen, tagLen - 1, "%s-%s", "teeos", logItemTag);
    if (ret < 0) {
        tloge("Snprintf_s for ta tag name is failed:0x%x\n", ret);
        goto FREE_NAME;
    }

    ret = InsertTagNode(isTa, tagName, tagLen, logTag, logItem);
    if (ret < 0) {
        tloge("Insert tag node to list is failed:0x%x\n", ret);
        goto FREE_NAME;
    }

    return true;

FREE_NAME:
    free(tagName);
    return false;
}

void JudgeLogTag(const struct LogItem *logItem, bool isTa, const char **logTag)
{
    if (logItem == NULL || logTag == NULL) {
        return;
    }

    /* ta or special drv module from teeos need get proc log tag */
    if (isTa || logItem->logSourceType > COMMON_DRV_SOURCE) {
        /* write ta log to applocat file */
        bool getTagRet = GetTagName(isTa, logItem, logTag);
        if (!getTagRet) {
            tloge("Find ta task name is failed\n");
            *logTag = LOG_TEEOS_TAG;
        }
        return;
    }

    /* write teeos log to applogcat file */
    *logTag = LOG_TEEOS_TAG;
}

static void FreeTaTagNode(void)
{
    struct ListNode *node = NULL;
    struct ListNode *n = NULL;
    struct TaTagInfo *taTagInfo = NULL;

    if (LIST_EMPTY(&g_taTagList)) {
        return;
    }

    LIST_FOR_EACH_SAFE(node, n, &g_taTagList) {
        taTagInfo = CONTAINER_OF(node, struct TaTagInfo, taTagNode);
        if (taTagInfo != NULL) {
            ListRemoveTail(&taTagInfo->taTagNode);
            if (taTagInfo->taTagName != NULL) {
                free(taTagInfo->taTagName);
                taTagInfo->taTagName = NULL;
            }    
            free(taTagInfo);
            taTagInfo = NULL;
        }
    }
}

static void FreeDriverTagNode(void)
{
    struct ListNode *node = NULL;
    struct ListNode *n = NULL;
    struct DriverTagInfo *tagInfo = NULL;

    if (LIST_EMPTY(&g_driverTagList)) {
        return;
    }

    LIST_FOR_EACH_SAFE(node, n, &g_driverTagList) {
        tagInfo = CONTAINER_OF(node, struct DriverTagInfo, tagNode);
        if (tagInfo != NULL) {
            ListRemoveTail(&tagInfo->tagNode);
            if (tagInfo->tagName != NULL) {
                free(tagInfo->tagName);
                tagInfo->tagName = NULL;
            }
            free(tagInfo);
            tagInfo = NULL;
        }
    }
}

void FreeTagNode(void)
{
    FreeDriverTagNode();
    FreeTaTagNode();
}
