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

#include "tee_load_dynamic.h"
#include <sys/stat.h>  /* for stat */
#include <dirent.h>

#include "securec.h"
#include "tc_ns_client.h"
#include "tee_log.h"
#include "secfile_load_agent.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "teecd_load_dynamic"

#if defined(DYNAMIC_DRV_DIR) || defined(DYNAMIC_SRV_DIR)
#define MAX_FILE_NAME_LEN 64

static DIR *OpenDynamicDir(const char *dynDir)
{
    DIR *dir = opendir(dynDir);
    if (dir == NULL) {
        tloge("Open drv dir: %s failed\n", dynDir);
    }

    return dir;
}

static int32_t LoadOneFile(const char *dynDir, const struct dirent *dirFile, int32_t fd, uint32_t loadType)
{
    char name[MAX_FILE_NAME_LEN];
    FILE *fp = NULL;
    int32_t ret = -1;

    if (strcmp(dirFile->d_name, ".") == 0 || strcmp(dirFile->d_name, "..") == 0) {
        tloge("No need to load\n");
        goto END;
    }

    if (strstr(dirFile->d_name, ".sec") == NULL) {
        tloge("Only support sec file\n");
        goto END;
    }

    if (memset_s(name, sizeof(name), 0, sizeof(name)) != 0) {
        tloge("Mem set failed, name: %s, size: %u\n", name, (uint32_t)sizeof(name));
        goto END;
    }

    if (strcat_s(name, MAX_FILE_NAME_LEN, dynDir) != 0) {
        tloge("Dir name too long: %s\n", dynDir);
        goto END;
    }

    if (strcat_s(name, MAX_FILE_NAME_LEN, dirFile->d_name) != 0) {
        tloge("Drv name too long: %s\n", dirFile->d_name);
        goto END;
    }

    fp = fopen(name, "r");
    if (fp == NULL) {
        tloge("Open drv failed: %s\n", name);
	goto END;
    }

    ret = LoadSecFile(fd, fp, loadType, NULL);
    if (ret != 0) {
        tloge("Load dynamic failed: %s\n", name);
    }

END:
    if (fp != NULL) {
        (void)fclose(fp);
    }

    return ret;
}

static void LoadOneDynamicDir(int32_t fd, const char *dynDir, uint32_t loadType)
{
    int32_t ret;
    struct dirent *dirFile = NULL;

    DIR *dir = OpenDynamicDir(dynDir);
    if (dir == NULL) {
        tloge("Dynamic dir not exist\n");
	return;
    }
    while ((dirFile = readdir(dir)) != NULL) {
        ret = LoadOneFile(dynDir, dirFile, fd, loadType);
	if (ret != 0) {
            tloge("Load dynamic failed\n");
            continue;	    
        }
    }
    (void)closedir(dir);
}

void LoadDynamicDir()
{
    int32_t fd = GetSecLoadAgentFd();

#ifdef DYNAMIC_DRV_DIR
    LoadOneDynamicDir(fd, DYNAMIC_DRV_DIR, LOAD_DYNAMIC_DRV);
#endif

#ifdef DYNAMIC_SRV_DIR
    LoadOneDynamicDir(fd, DYNAMIC_SRV_DIR, LOAD_SERVICE);
#endif
}
#endif
