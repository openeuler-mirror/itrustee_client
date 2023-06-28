/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2014-2022. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "tlogcat.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h> /* for ioctl */
#include <sys/select.h>
#include <securec.h>

#include "tee_log.h"
#include "tarzip.h"
#include "proc_tag.h"
#include "sys_log_api.h"
#include "tee_client_version.h"
#include "tc_ns_client.h"
#include "tee_version_check.h"

#define LOG_FILE_TA_DEMO            "LOG@A32B3D00CB5711E39C1A0800200C9A66-0"
#define LOG_FILE_TA_COMPRESS_DEMO   "LOG@A32B3D00CB5711E39C1A0800200C9A66-0.tar.gz"
#define LOG_FILE_TEE_DEMO           "teeOS_log-0"
#define LOG_FILE_TEE_COMPRESS_DEMO  "teeos-log-0.tar.gz"

#define TLOGCAT_FILE_MODE           0750
#define UUID_MAX_STR_LEN            40U
/* for log item */
#define LOG_ITEM_MAGIC              0x5A5A
#define LOG_ITEM_LEN_ALIGN          64
#define LOG_READ_STATUS_ERROR       0x000FFFF

#ifndef FILE_NAME_MAX_BUF
#define FILE_NAME_MAX_BUF           256
#endif

#define LOG_BUFFER_LEN              0x2000 /* 8K */
#define LOG_FILES_MAX               128U
#define FILE_VALID                  0x5a5aa5a5
#define LOG_FILE_LIMIT              (500 * 1024) /* log file size limit:500k */
#define MAX_TEE_VERSION_LEN         256U

#ifndef TEE_LOG_SUBFOLDER
#define TEE_LOG_SUBFOLDER "tee"
#endif

char g_teePath[FILE_NAME_MAX_BUF] = {0};
char g_teeTempPath[FILE_NAME_MAX_BUF] = {0};
gid_t g_teePathGroup = 0;

struct LogFile {
    FILE *file;
    struct TeeUuid uuid;
    long fileLen;
    uint32_t fileIndex; /* 0,1,2,3 */
    int32_t valid;      /* FILE_VALID */
    char logName[FILE_NAME_MAX_BUF];
};
static struct LogFile *g_files = NULL;
char *g_logBuffer = NULL;

/* for ioctl */
#define TEELOGGERIO                   0xBE
#define GET_VERSION_BASE              5
#define SET_READERPOS_CUR_BASE        6
#define SET_TLOGCAT_STAT_BASE         7
#define GET_TLOGCAT_STAT_BASE         8
#define GET_TEE_INFO_BASE             9

#define TEELOGGER_GET_VERSION       _IOR(TEELOGGERIO, GET_VERSION_BASE, char[MAX_TEE_VERSION_LEN])
/* set the log reader pos to current pos */
#define TEELOGGER_SET_READERPOS_CUR _IO(TEELOGGERIO, SET_READERPOS_CUR_BASE)
#define TEELOGGER_SET_TLOGCAT_STAT  _IO(TEELOGGERIO, SET_TLOGCAT_STAT_BASE)
#define TEELOGGER_GET_TLOGCAT_STAT  _IO(TEELOGGERIO, GET_TLOGCAT_STAT_BASE)
#define TEELOGGER_GET_TEE_INFO      _IOR(TEELOGGERIO, GET_TEE_INFO_BASE, TC_NS_TEE_Info)

static int32_t g_devFd = -1;
static char g_teeVersion[MAX_TEE_VERSION_LEN];
static int32_t g_readposCur = 0;
static struct ModuleInfo g_tlogcatModuleInfo = {
	.deviceName = TC_LOGGER_DEV_NAME,
	.moduleName = "tlogcat",
	.ioctlNum = TEELOGGER_GET_TEE_INFO,
};

static int32_t GetLogPathBasePos(const char *temp, char **pos)
{
    if (access(TEE_LOG_PATH_BASE, F_OK) != 0) {
        tloge("log dir is not exist\n");
        return -1;
    }

    if (strncmp(temp, TEE_LOG_PATH_BASE, strlen(TEE_LOG_PATH_BASE)) == 0) {
        *pos += strlen(TEE_LOG_PATH_BASE);
    }
    return 0;
}

/*
 * path:file path name.
 * P: /data/vendor/log/hisi_logs/tee/
 * before P version: /data/hisi_logs/running_trace/teeos_logs/LOG@A32B3D00CB5711E39C1A0800200C9A66-0
 */
static int32_t LogFilesMkdirR(const char *path)
{
    int32_t ret;
    bool check = false;
    char *temp = strdup(path);
    char *pos = temp;

    if (temp == NULL) {
        return -1;
    }

    ret = GetLogPathBasePos(temp, &pos);
    if (ret != 0) {
        free(temp);
        return ret;
    }

    for (; *pos != '\0'; ++pos) {
        if (*pos == '/') {
            *pos = '\0';

            ret = mkdir(temp, S_IRWXU | S_IRWXG);
            check = (ret < 0 && errno == EEXIST);
            if (check) {
                /* file is exist */
                *pos = '/';
                continue;
            } else if (ret != 0) {
                tloge("mkdir %s fail, errno %d\n", temp, errno);
                free(temp);
                return -1;
            }

            ret = chown(temp, (uid_t)-1, g_teePathGroup);
            if (ret < 0) {
                tloge("chown %s err %d\n", temp, ret);
            }
            ret = chmod(temp, TLOGCAT_FILE_MODE);
            if (ret < 0) {
                tloge("chmod %s err %d\n", temp, ret);
            }
            tlogv("for %s\n", temp);
            *pos = '/';
        }
    }

    free(temp);
    return 0;
}

static void GetUuidStr(const struct TeeUuid *uuid, char *name, uint32_t nameLen)
{
    int32_t ret = snprintf_s(name, nameLen, nameLen - 1, "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
                             uuid->timeLow, uuid->timeMid, uuid->timeHiAndVersion, uuid->clockSeqAndNode[0],
                             uuid->clockSeqAndNode[1], uuid->clockSeqAndNode[2], uuid->clockSeqAndNode[3],
                             uuid->clockSeqAndNode[4], uuid->clockSeqAndNode[5], uuid->clockSeqAndNode[6],
                             uuid->clockSeqAndNode[7]);
    if (ret <= 0) {
        tloge("convert uuid to string failed\n");
    }

    return;
}

static struct LogFile *LogFilesAdd(const struct TeeUuid *uuid, const char *logName,
    FILE *file, long fileLen, uint32_t index)
{
    uint32_t i;
    errno_t rc;

    for (i = 0; i < LOG_FILES_MAX; i++) {
        /* filter valid file */
        if (g_files[i].file != NULL || g_files[i].valid == FILE_VALID) {
            continue;
        }

        rc = memcpy_s(g_files[i].logName, sizeof(g_files[i].logName), logName, strlen(logName) + 1);
        if (rc != EOK) {
            tloge("memcpy log name failed\n");
            goto CLOSE_F;
        }

        rc = memcpy_s(&g_files[i].uuid, sizeof(g_files[i].uuid), uuid, sizeof(struct TeeUuid));
        if (rc != EOK) {
            tloge("memcpy uuid failed\n");
            goto CLOSE_F;
        }

        g_files[i].file = file;
        g_files[i].fileLen = fileLen;
        g_files[i].fileIndex = index;
        g_files[i].valid = FILE_VALID;

        return &g_files[i];
    }

CLOSE_F:
    (void)fclose(file);
    return NULL;
}

static void CloseFileFd(int32_t *fd)
{
    if (*fd < 0) {
        return;
    }

    close(*fd);
    *fd = -1;
}

static FILE *LogFilesOpen(const char *logName, long *fileLen)
{
    int32_t ret;
    FILE *file = NULL;
    bool isNewFile = false;

    if (access(logName, F_OK) == 0) {
        ret = chmod(logName, S_IRUSR | S_IWUSR | S_IRGRP);
        if (ret != 0) {
            tloge("open chmod error, ret: %d, file:%s\n", ret, logName);
            return NULL;
        }
    }

    int32_t fd1 = open(logName, O_WRONLY);
    if (fd1 < 0) {
        /* file is not exist */
        file = fopen(logName, "a+");
        isNewFile = true;
    } else {
        /* file is exist */
        file = fdopen(fd1, "w");
    }

    if (file == NULL) {
        tloge("open error:logName %s, errno %d\n", logName, errno);
        CloseFileFd(&fd1);
        return NULL;
    }

    int32_t fd2 = fileno(file);
    if (fd2 < 0) {
        tloge("fileno is error\n");
        (void)fclose(file);
        return NULL;
    }
    ret = fchown(fd2, (uid_t)-1, g_teePathGroup);
    if (ret < 0) {
        tloge("chown error\n");
    }
    ret = fchmod(fd2, S_IRUSR | S_IWUSR | S_IRGRP);
    if (ret < 0) {
        tloge("chmod error\n");
    }

    (void)fseek(file, 0L, SEEK_END); /* seek to file ending */
    if (fileLen != NULL) {
        *fileLen = ftell(file); /* get file len */
    }

    /* write tee version info */
    if (isNewFile) {
        size_t ret1 = fwrite(g_teeVersion, 1, strlen(g_teeVersion), file);
        if (ret1 == 0) {
            tloge("write tee version to %s failed %zu\n", logName, ret1);
        }
    }

    return file;
}

static bool IsTaUuid(const struct TeeUuid *uuid)
{
    if (uuid == NULL) {
        return false;
    }

    uint32_t i;
    uint8_t *p = (uint8_t *)uuid;
    for (i = 0; i < sizeof(*uuid); i++) {
        if (p[i] != 0) {
            return true;
        }
    }

    return false;
}

struct FileNameAttr {
    const char *uuidAscii;
    bool isTa;
    uint32_t index;
};

static void SetFileNameAttr(struct FileNameAttr *nameAttr, const char *uuidAscii,
    bool isTa, uint32_t index)
{
    nameAttr->uuidAscii = uuidAscii;
    nameAttr->isTa = isTa;
    nameAttr->index = index;
}

#ifndef TEE_LOG_NON_REWINDING
static int32_t LogAssembleFilename(char *logName, size_t logNameLen,
    const char *logPath, const struct FileNameAttr *nameAttr)
{
    if (nameAttr->isTa) {
        return snprintf_s(logName, logNameLen, logNameLen - 1, "%s%s%s-%u",
            logPath, "LOG@", nameAttr->uuidAscii, nameAttr->index);
    } else {
        return snprintf_s(logName, logNameLen, logNameLen - 1, "%s%s-%u",
            logPath, "teeOS_log", nameAttr->index);
    }
}

static char *g_compressFile = NULL;
static struct TeeUuid g_compressUuid;

static int32_t LogAssembleCompressFilename(char *logName, size_t logNameLen,
    const char *logPath, const struct FileNameAttr *nameAttr)
{
    if (nameAttr->isTa) {
        return snprintf_s(logName, logNameLen, logNameLen - 1,
            "%s%s%s-%u.tar.gz", logPath, "LOG@", nameAttr->uuidAscii, nameAttr->index);
    } else {
        return snprintf_s(logName, logNameLen, logNameLen - 1,
            "%s%s-%u.tar.gz", logPath, "teeos-log", nameAttr->index);
    }
}

static void TriggerCompress(void)
{
    char *filesToCompress[LOG_FILE_INDEX_MAX] = {0};
    uint32_t i;
    int32_t ret;
    char uuidAscii[UUID_MAX_STR_LEN] = {0};
    struct FileNameAttr nameAttr = {0};
    bool isTa = IsTaUuid(&g_compressUuid);

    GetUuidStr(&g_compressUuid, uuidAscii, sizeof(uuidAscii));

    for (i = 0; i < LOG_FILE_INDEX_MAX; i++) {
        filesToCompress[i] = malloc(FILE_NAME_MAX_BUF);
        if (filesToCompress[i] == NULL) {
            tloge("malloc file for compress failed\n");
            goto FREE_RES;
        }

        SetFileNameAttr(&nameAttr, uuidAscii, isTa, i);
        ret = LogAssembleFilename(filesToCompress[i], FILE_NAME_MAX_BUF, g_teeTempPath, &nameAttr);
        if (ret < 0) {
            tloge("snprintf file to compress error %d %s, %s, %u\n", ret, g_teeTempPath, uuidAscii, i);
            continue;
        }
        ret = chmod(filesToCompress[i], S_IRUSR | S_IWUSR | S_IRGRP);
        if (ret != 0) {
           tloge("trigger compress chmod failed\n");
        }
    }

    TarZipFiles(LOG_FILE_INDEX_MAX, (const char**)filesToCompress, g_compressFile, g_teePathGroup);

FREE_RES:
    /* remove compressed logs */
    for (i = 0; i < LOG_FILE_INDEX_MAX; i++) {
        if (filesToCompress[i] == NULL) {
            continue;
        }

        ret = unlink(filesToCompress[i]);
        if (ret < 0) {
            tloge("unlink file %s failed, ret %d\n", filesToCompress[i], ret);
        }
        free(filesToCompress[i]);
    }

    ret = rmdir(g_teeTempPath);
    if (ret < 0) {
        tloge("rmdir failed %s, ret %d, errno %d\n", g_teeTempPath, ret, errno);
    }
}

char g_logNameCompress[FILE_NAME_MAX_BUF] = {0};
char g_uuidAscii[UUID_MAX_STR_LEN] = {0};
char g_logName[FILE_NAME_MAX_BUF] = {0};

static char *GetCompressFile(const struct TeeUuid *uuid)
{
    uint32_t i;
    int32_t rc;
    struct FileNameAttr nameAttr = {0};
    bool isTa = IsTaUuid(uuid);

    /*
     * Find a suitable compressed file name,
     * %s-0.tar.gz, %s-1.tar.gz %s-2.tar.gz %s-3.tar.gz then to zero, recycle using
     */
    for (i = 0; i < LOG_FILE_INDEX_MAX; i++) {
        SetFileNameAttr(&nameAttr, g_uuidAscii, isTa, i);
        rc = LogAssembleCompressFilename(g_logNameCompress, sizeof(g_logNameCompress), g_teePath, &nameAttr);
        if (rc < 0) {
            tloge("snprintf log name compresserror error %d %s %s %u\n", rc, g_teePath, g_uuidAscii, i);
            continue;
        }

        if (access(g_logNameCompress, F_OK) != 0) {
            break;
        }
    }

    if (i >= LOG_FILE_INDEX_MAX) {
        return NULL;
    }

    return g_logNameCompress;
}

static void ArrangeCompressFile(const struct TeeUuid *uuid)
{
    uint32_t i;
    int32_t ret;
    struct FileNameAttr nameAttr = {0};
    bool isTa = IsTaUuid(uuid);

    /* delete first file, and other files's name number rename forward. */
    SetFileNameAttr(&nameAttr, g_uuidAscii, isTa, 0);
    ret = LogAssembleCompressFilename(g_logName, sizeof(g_logName), g_teePath, &nameAttr);
    if (ret < 0) {
        tloge("arrange snprintf error %d %s %s %d\n", ret, g_teePath, g_uuidAscii, 0);
        return;
    }
    ret = unlink(g_logName);
    if (ret < 0) {
        tloge("unlink failed %s, %d\n", g_logName, ret);
    }

    /*
     * Updates the names of files except the first file.
     * Use the last file name as the compress file name.
     */
    for (i = 1; i < LOG_FILE_INDEX_MAX; i++) {
        SetFileNameAttr(&nameAttr, g_uuidAscii, isTa, i);
        ret = LogAssembleCompressFilename(g_logNameCompress, sizeof(g_logNameCompress), g_teePath, &nameAttr);
        if (ret < 0) {
            tloge("snprintf log name compress error %d %s %s %u\n", ret, g_teePath, g_uuidAscii, i);
            continue;
        }

        ret = rename(g_logNameCompress, g_logName);
        if (ret < 0) {
            tloge("rename error %s, %s, %d, errno %d\n", g_logNameCompress, g_logName, ret, errno);
        }

        ret = memcpy_s(g_logName, sizeof(g_logName), g_logNameCompress, sizeof(g_logNameCompress));
        if (ret != EOK) {
            tloge("memcpy_s error %d\n", ret);
            return;
        }
    }
}

static void UnlinkTmpFile(const char *name)
{
    struct stat st = {0};

    if (lstat(name, &st) < 0) {
        tloge("lstat %s failed, errno is %d\n", name, errno);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        return;
    }

    if (unlink(name) < 0) {
        tloge("unlink %s failed, errno is %d\n", name, errno);
    }
}

static void LogTmpDirClear(const char *tmpPath)
{
    int32_t ret;
    char filePathName[FILE_NAME_MAX_BUF] = {0};

    DIR *dir = opendir(tmpPath);
    if (dir == NULL) {
        tloge("open dir %s failed, errno:%d\n", tmpPath, errno);
        return;
    }

    struct dirent *de = readdir(dir);

    while (de != NULL) {
        if (strncmp(de->d_name, "..", sizeof("..")) == 0 || strncmp(de->d_name, ".", sizeof(".")) == 0) {
            de = readdir(dir);
            continue;
        }
        ret = snprintf_s(filePathName, sizeof(filePathName), sizeof(filePathName) - 1,
            "%s/%s", tmpPath, de->d_name);
        if (ret == -1) {
            tloge("get file path name failed %d\n", ret);
            de = readdir(dir);
            continue;
        }
        UnlinkTmpFile(filePathName);
        de = readdir(dir);
    }

    (void)closedir(dir);
    ret = rmdir(tmpPath);
    if (ret < 0) {
        tloge("clear %s failed, err:%d, errno:%d\n", tmpPath, ret, errno);
    }
}

static int32_t MkdirTmpPath(const char *tmpPath)
{
    int32_t ret;

    /* create a temp path, and move these files to this path for compressing */
    ret = rmdir(tmpPath);

    bool check = (ret < 0 && errno != ENOENT);
    if (check) {
        LogTmpDirClear(tmpPath);
    }

    ret = mkdir(tmpPath, TLOGCAT_FILE_MODE);
    if (ret < 0) {
        tloge("mkdir %s failed, errno:%d\n", tmpPath, errno);
        return -1;
    }
    return 0;
}

static void MoveFileToTmpPath(bool isTa, uint32_t index)
{
    int32_t ret;
    struct FileNameAttr nameAttr = {0};

    SetFileNameAttr(&nameAttr, g_uuidAscii, isTa, index);
    ret = LogAssembleFilename(g_logName, sizeof(g_logName), g_teePath, &nameAttr);
    if (ret < 0) {
        tloge("snprintf log name error %d %s %s %u\n", ret, g_teePath, g_uuidAscii, index);
        return;
    }

    ret = LogAssembleFilename(g_logNameCompress, sizeof(g_logNameCompress), g_teeTempPath, &nameAttr);
    if (ret < 0) {
        tloge("snprintf log name compress error %d %s %s %u\n", ret, g_teeTempPath, g_uuidAscii, index);
        return;
    }

    ret = rename(g_logName, g_logNameCompress);
    bool check = (ret < 0 && errno != ENOENT);
    /* File is exist, but rename is failed */
    if (check) {
        tloge("rename %s failed, err: %d, errno:%d\n", g_logName, ret, errno);
        ret = unlink(g_logName);
        if (ret < 0) {
            tloge("unlink failed %s %d\n", g_logName, ret);
        }
    }
}

static void LogFilesCompress(const struct TeeUuid *uuid)
{
    int32_t i;
    int32_t rc;
    bool isTa = IsTaUuid(uuid);

    rc = MkdirTmpPath(g_teeTempPath);
    if (rc != 0) {
        return;
    }

    GetUuidStr(uuid, g_uuidAscii, sizeof(g_uuidAscii));

    for (i = LOG_FILE_INDEX_MAX - 1; i >= 0; i--) {
        MoveFileToTmpPath(isTa, (uint32_t)i);
    }

    /*
     * Find a suitable compressed file name,
     * %s-0.tar.gz, %s-1.tar.gz %s-2.tar.gz %s-3.tar.gz then to zero, recycle using.
     */
    if (GetCompressFile(uuid) == NULL) {
        /*
         * Delete first file, and other files's name number rename forward.
         * Use the last file name as the compress file name.
         */
        ArrangeCompressFile(uuid);
    }

    g_compressFile = g_logNameCompress;
    rc = memcpy_s(&g_compressUuid, sizeof(g_compressUuid), uuid, sizeof(struct TeeUuid));
    if (rc != EOK) {
        tloge("memcpy_s error %d\n", rc);
        return;
    }

    TriggerCompress();
}

static void LogFileFull(uint32_t fileNum)
{
    char logName[FILE_NAME_MAX_BUF] = {0};
    char logName2[FILE_NAME_MAX_BUF] = {0};
    char uuidAscii[UUID_MAX_STR_LEN] = {0};
    int32_t rc;
    struct FileNameAttr nameAttr = {0};

    if (g_files[fileNum].fileIndex >= LOG_FILE_INDEX_MAX) {
        tloge("the file index is overflow %u\n", g_files[fileNum].fileIndex);
        return;
    }

    bool isTa = IsTaUuid(&g_files[fileNum].uuid);

    GetUuidStr(&g_files[fileNum].uuid, uuidAscii, sizeof(uuidAscii));

    SetFileNameAttr(&nameAttr, uuidAscii, isTa, 0);
    rc = LogAssembleFilename(logName, sizeof(logName), g_teePath, &nameAttr);
    if (rc < 0) {
        tloge("snprintf log name error %d %s %s %d\n", rc, g_teePath, uuidAscii, 0);
        return;
    }

    SetFileNameAttr(&nameAttr, uuidAscii, isTa, g_files[fileNum].fileIndex + 1);
    rc = LogAssembleFilename(logName2, sizeof(logName2), g_teePath, &nameAttr);
    if (rc < 0) {
        tloge("snprintf log name2 error %d %s %s %u\n", rc, g_teePath, uuidAscii, g_files[fileNum].fileIndex + 1);
        return;
    }

    rc = rename(logName, logName2);
    if (rc < 0) {
        tloge("file full and rename error %s, %s, %d, errno %d\n", logName, logName2, rc, errno);
        return;
    }

    rc = chmod(logName2, S_IRUSR | S_IRGRP);
    if (rc != 0) {
        tloge("chmod file: %s failed, ret: %d\n", logName2, rc);
    }

    return;
}

static int32_t LogFilesChecklimit(uint32_t fileNum)
{
    errno_t rc;

    if (g_files[fileNum].fileLen >= LOG_FILE_LIMIT) {
        (void)fclose(g_files[fileNum].file);

        if (g_files[fileNum].fileIndex >= (LOG_FILE_INDEX_MAX - 1)) {
            /* four files are all full, need to compress files. */
            LogFilesCompress(&g_files[fileNum].uuid);
        } else {
            /* this file is full */
            LogFileFull(fileNum);
        }

        rc = memset_s(&g_files[fileNum], sizeof(g_files[fileNum]), 0, sizeof(struct LogFile));
        if (rc != EOK) {
            tloge("memset failed %d\n", rc);
        }

        return -1;
    }

    return 0;
}

#else

static int32_t LogAssembleFilename(char *logName, size_t logNameLen,
		const char *logPath, const struct FileNameAttr *nameAttr)
{
	if (nameAttr->isTa) {
		return snprintf_s(logName, logNameLen, logNameLen - 1, "%s%s", logPath, "ta_runlog.log");
	} else {
		return snprintf_s(logName, logNameLen, logNameLen - 1, "%s%s". logPath, "teeos_runlog.log");
	}
}
#endif

static struct LogFile *GetUsableFile(const struct TeeUuid *uuid)
{
    uint32_t i;

    for (i = 0; i < LOG_FILES_MAX; i++) {
        if (memcmp(&g_files[i].uuid, uuid, sizeof(struct TeeUuid)) != 0) {
            continue;
        }

        if (g_files[i].valid != FILE_VALID) {
            continue;
        }

        if (g_files[i].file == NULL) {
            tloge("unexpected error in index %u, file is null\n", i);
            (void)memset_s(&g_files[i], sizeof(g_files[i]), 0, sizeof(g_files[i]));
            continue;
        }

#ifndef TEE_LOG_NON_REWINDING
        /* check file len is limit */
        if (LogFilesChecklimit(i) != 0) {
            continue;
        }
#endif

        tlogd("get log file %s\n", g_files[i].logName);
        return &g_files[i];
    }

    return NULL;
}

static void GetFileIndex(const char *uuidAscii, bool isTa, uint32_t *fileIndex)
{
    char logName[FILE_NAME_MAX_BUF] = {0};
    int32_t i;
    int32_t ret;
    struct FileNameAttr nameAttr = {0};

    /* get the number of file */
    for (i = LOG_FILE_INDEX_MAX - 1; i >= 0; i--) {
        *fileIndex = (uint32_t)i;

        SetFileNameAttr(&nameAttr, uuidAscii, isTa, (uint32_t)i);
        ret = LogAssembleFilename(logName, sizeof(logName), g_teePath, &nameAttr);
        if (ret < 0) {
            tloge("snprintf log name error %d %s %s %d\n", ret, g_teePath, uuidAscii, i);
            continue;
        }

        if (access(logName, F_OK) == 0) {
            break;
        }
    }
}

static struct LogFile *LogFilesGet(const struct TeeUuid *uuid, bool isTa)
{
    uint32_t fileIndex;
    errno_t rc;
    char logName[FILE_NAME_MAX_BUF] = {0};
    char uuidAscii[UUID_MAX_STR_LEN] = {0};
    long fileLen;
    FILE *file = NULL;
    struct FileNameAttr nameAttr = {0};

    if (uuid == NULL) {
        return NULL;
    }

    struct LogFile *logFile = GetUsableFile(uuid);
    if (logFile != NULL) {
        return logFile;
    }

    /* base on uuid data, new a file */
    if (LogFilesMkdirR(g_teePath) != 0) {
        tloge("mkdir log path is failed\n");
        return NULL;
    }
    GetUuidStr(uuid, uuidAscii, sizeof(uuidAscii));

    /* get the number of file */
    GetFileIndex(uuidAscii, isTa, &fileIndex);

    /* each time write the "-0" suffix name file */
    SetFileNameAttr(&nameAttr, uuidAscii, isTa, 0);
    rc = LogAssembleFilename(logName, sizeof(logName), g_teePath, &nameAttr);
    if (rc < 0) {
        tloge("snprintf log name error %d %s %s\n", rc, g_teePath, uuidAscii);
        return NULL;
    }

    file = LogFilesOpen(logName, &fileLen);
    if (file == NULL) {
        return NULL;
    }

    return LogFilesAdd(uuid, logName, file, fileLen, fileIndex);
}

static void LogFilesClose(void)
{
    uint32_t i;

    if (g_files == NULL) {
        return;
    }

    /*
     * Check whether the file size exceeds the value of LOG_FILE_LIMIT. If yes, create another file.
     * If the four files are all full, compress the files and delete the original files.
     */
    for (i = 0; i < LOG_FILES_MAX; i++) {
        if (g_files[i].file == NULL) {
            continue;
        }

        tlogd("close file %s, fileLen %ld\n", g_files[i].logName, g_files[i].fileLen);
        (void)fflush(g_files[i].file);
		int32_t fd = fileno(g_files[i].file);
		(void)fsync(fd);
        (void)fclose(g_files[i].file);

#ifndef TEE_LOG_NON_REWINDING
        if (g_files[i].fileLen >= LOG_FILE_LIMIT) {
            if (g_files[i].fileIndex >= (LOG_FILE_INDEX_MAX - 1)) {
                /* four files are all full, need to compress files. */
                LogFilesCompress(&g_files[i].uuid);
            } else {
                /* this file is full */
                LogFileFull(i);
            }
        }

        int32_t ret = chmod(g_files[i].logName, S_IRUSR | S_IRGRP);
        if (ret != 0) {
            tlogi("close file: %s failed, chmod ret: %d errno: %d\n", g_files[i].logName, ret, errno);
        }
#endif
        (void)memset_s(&g_files[i], sizeof(g_files[i]), 0, sizeof(g_files[i]));
    }
}

static void HelpShow(void)
{
    printf("this is help, you should input:\n");
    printf("    -v:  print iTrustee version\n");
    printf("    -t:  only print the new log\n");
}

static struct LogItem *LogItemGetNext(const char *logBuffer, size_t scopeLen)
{
    if (logBuffer == NULL) {
        return NULL;
    }

    struct LogItem *logItemNext = (struct LogItem *)logBuffer;

    size_t itemMaxSize = ((scopeLen > LOG_ITEM_MAX_LEN) ? LOG_ITEM_MAX_LEN : scopeLen);

    bool isValidItem = ((logItemNext->magic == LOG_ITEM_MAGIC) &&
        (logItemNext->logBufferLen > 0) && (logItemNext->logRealLen > 0) &&
        (logItemNext->logBufferLen % LOG_ITEM_LEN_ALIGN == 0) &&
        (logItemNext->logRealLen <= logItemNext->logBufferLen) &&
        ((logItemNext->logBufferLen - logItemNext->logRealLen) < LOG_ITEM_LEN_ALIGN) &&
        (logItemNext->logBufferLen + sizeof(struct LogItem) <= itemMaxSize));
    if (isValidItem) {
        return logItemNext;
    }

    tlogd("logItemNext info: magic %x, logBufferLen %x, logRealLen %x\n",
          logItemNext->magic, logItemNext->logBufferLen, logItemNext->logRealLen);
    return NULL;
}

static void LogSetReadposCur(void)
{
    int32_t ret;
    if (g_devFd < 0) {
        tloge("open log device error\n");
        return;
    }
    ret = ioctl(g_devFd, TEELOGGER_SET_READERPOS_CUR, 0);
    if (ret != 0) {
        tloge("set readpos cur failed %d\n", ret);
    }

    g_readposCur = 1;
    return;
}

static int32_t LogSetTlogcatF(void)
{
    int32_t ret;

    if (g_devFd < 0) {
        tloge("open log device error\n");
        return -1;
    }

    ret = ioctl(g_devFd, TEELOGGER_SET_TLOGCAT_STAT, 0);
    if (ret != 0) {
        tloge("set tlogcat status failed %d\n", ret);
    }

    return ret;
}

static int32_t LogGetTlogcatF(void)
{
    int32_t ret;

    if (g_devFd < 0) {
        tloge("open log device error\n");
        return -1;
    }

    ret = ioctl(g_devFd, TEELOGGER_GET_TLOGCAT_STAT, 0);
    if (ret != 0) {
        tloge("get tlogcat status failed %d\n", ret);
    }

    return ret;
}

static void WritePrivateLogFile(const struct LogItem *logItem, bool isTa)
{
    size_t writeNum;
    struct LogFile *logFile = NULL;

    logFile = LogFilesGet((struct TeeUuid *)logItem->uuid, isTa);
    if ((logFile == NULL) || (logFile->file == NULL)) {
        tloge("can not save log, file is null\n");
        return;
    }

    writeNum = fwrite(logItem->logBuffer, 1, (size_t)logItem->logRealLen, logFile->file);
    if (writeNum != (size_t)logItem->logRealLen) {
        tloge("save file failed %zu, %u\n", writeNum, logItem->logRealLen);
        (void)fclose(logFile->file);
        (void)memset_s(logFile, sizeof(struct LogFile), 0, sizeof(struct LogFile));
    }

    logFile->fileLen += (long)writeNum;
}

static void WriteLogFile(const struct LogItem *logItem)
{
    bool isTa = IsTaUuid((struct TeeUuid *)logItem->uuid);
#ifdef TEE_LOG_NON_REWINDING
    LogWriteSysLog(logItem, isTa);
#endif
    WritePrivateLogFile(logItem, isTa);
}

static void OutputLog(struct LogItem *logItem, bool writeFile)
{
    if (writeFile) {
        /* write log file */
        WriteLogFile(logItem);
        return;
    }

    /* ouput log info to display interface */
    if (logItem->logRealLen < logItem->logBufferLen) {
        logItem->logBuffer[logItem->logRealLen] = 0;
    } else {
        logItem->logBuffer[logItem->logRealLen - 1] = 0;
    }
    printf("%s", (char *)logItem->logBuffer);
}

static void ReadLogBuffer(bool writeFile, const char *logBuffer, size_t readLen)
{
    size_t logItemTotalLen = 0;
    struct LogItem *logItem = NULL;

    /* Cyclically processes all log records. */
    logItem = LogItemGetNext(logBuffer, readLen);

    while (logItem != NULL) {
        tlogd("get log length %u\n", logItem->logBufferLen);

        OutputLog(logItem, writeFile);

        /* check log item have been finished */
        logItemTotalLen += logItem->logBufferLen + sizeof(struct LogItem);
        if (logItemTotalLen >= readLen) {
            tlogd("totallen %zd, readLen %zd\n", logItemTotalLen, readLen);
            break;
        }

        logItem = LogItemGetNext((char *)(logItem->logBuffer + logItem->logBufferLen),
            readLen - logItemTotalLen);
    }
}

#define SLEEP_NAO_SECONDS 300000000
static void ProcReadStatusError(void)
{
    /* sleep 300 ms then retry, tv_nsec' unit is nanosecond */
    struct timespec requst = {0};

    requst.tv_nsec = SLEEP_NAO_SECONDS;
    (void)nanosleep(&requst, NULL);
}

static int32_t ProcReadLog(bool writeFile, const fd_set *readset)
{
    ssize_t ret;
    size_t readLen;

    if (FD_ISSET(g_devFd, readset)) {
    READ_RETRY:
        ret = read(g_devFd, g_logBuffer, LOG_BUFFER_LEN);
        if (ret == 0) {
            tlogd("tlogcat read no data:ret=%zd\n", ret);
            return -1;
        } else if (ret < 0) {
            tloge("tlogcat read failed:ret=%zd\n", ret);
            return -1;
        } else if (ret > LOG_BUFFER_LEN) {
            tloge("invalid read length = %zd\n", ret);
            return -1;
        } else {
            tlogd("read length ret = %zd\n", ret);
        }

        readLen = (size_t)ret;

        /* if the log crc check is error , maybe the log memory need update, wait a while and try again */
        if (ret == LOG_READ_STATUS_ERROR) {
            ProcReadStatusError();
            goto READ_RETRY;
        }

        /* Cyclically processes all log records. */
        ReadLogBuffer(writeFile, g_logBuffer, readLen);
        goto READ_RETRY; /* read next buffer from log mem */
    } else {
        tloge("no have read signal\n");
    }

    return 0;
}

static void LogPrintTeeVersion(void);

static void Func(bool writeFile)
{
    int32_t result;
    int32_t ret;
    fd_set readset;

    if (!writeFile) {
        LogPrintTeeVersion();
    }

    while (1) {
        /*
         * When current logs read finished, the code will return here, close this file,
         * and waiting a new start reading.
         */
        LogFilesClose();

        /* Wait for the log memory read signal. */
        do {
            FD_ZERO(&readset);
            FD_SET(g_devFd, &readset);
            tlogd("while select\n");
            result = select((g_devFd + 1), &readset, NULL, NULL, NULL);
        } while (result == -1 && errno == EINTR);

        if (result < 0) {
            continue;
        }

        ret = ProcReadLog(writeFile, &readset);
        if (ret != 0) {
            continue;
        }

        /* close file */
        LogFilesClose();

        FreeTagNode();
    }

    return;
}

static void GetTeePathGroup(void)
{
#ifdef AID_SYSTEM
	g_teePathGroup = AID_SYSTEM;
#else
    struct stat pathStat = {0};
    
    if (stat(TEE_LOG_PATH_BASE, &pathStat) != 0) {
        tloge("get base path stat failed\n");
        return;
    }
    g_teePathGroup = pathStat.st_gid;
#endif
}

#define MAX_TEE_LOG_SUBFOLDER_LEN      30U
#define TEE_COMPRESS_SUBFOLDER         "_tmp"
static int32_t GetTeeLogPath(void)
{
    int32_t ret;

    if (strnlen(TEE_LOG_PATH_BASE, FILE_NAME_MAX_BUF) >= FILE_NAME_MAX_BUF ||
        strnlen(TEE_LOG_SUBFOLDER, MAX_TEE_LOG_SUBFOLDER_LEN) >= MAX_TEE_LOG_SUBFOLDER_LEN) {
        tloge("invalid tee path params cfg, please check make scripts\n");
        return -1;
    }

    GetTeePathGroup();
#ifdef TEE_LOG_NON_REWINDING
    ret = snprintf_s(g_teePath, sizeof(g_teePath), sizeof(g_teePath) - 1,
		"%s", TEE_LOG_PATH_BASE);
#else
	ret = snprintf_s(g_teePath, sizeof(g_teePath), sizeof(g_teePath) - 1,
        "%s/%s/", TEE_LOG_PATH_BASE, TEE_LOG_SUBFOLDER);
#endif
    if (ret < 0) {
        tloge("get tee log path failed\n");
        return -1;
    }

    ret = snprintf_s(g_teeTempPath, sizeof(g_teeTempPath), sizeof(g_teeTempPath) - 1,
        "%s/%s/", g_teePath, TEE_COMPRESS_SUBFOLDER);
    if (ret < 0) {
        tloge("get tee temp log path failed\n");
        return -1;
    }
    return 0;
}

static int32_t TlogcatCheckTzdriverVersion(void)
{
	InitModuleInfo(&g_tlogcatModuleInfo);
	return CheckTzdriverVersion();
}

static int32_t Prepare(void)
{
    int32_t ret = GetTeeLogPath();
    if (ret != 0) {
        return ret;
    }

    g_logBuffer = malloc(LOG_BUFFER_LEN);
    if (g_logBuffer == NULL) {
        tloge("malloc log buffer failed\n");
        return -1;
    }

    g_files = malloc(sizeof(struct LogFile) * LOG_FILES_MAX);
    if (g_files == NULL) {
        tloge("malloc files failed\n");
        return -1;
    }

    (void)memset_s(g_files, (sizeof(struct LogFile) * LOG_FILES_MAX), 0, (sizeof(struct LogFile) * LOG_FILES_MAX));

    g_devFd = open(TC_LOGGER_DEV_NAME, O_RDONLY);
    if (g_devFd < 0) {
        tloge("open log device error\n");
        return -1;
    }

    tlogd("open dev success g_devFd=%d\n", g_devFd);

	if (TlogcatCheckTzdriverVersion() != 0) {
		tloge("check tlogcat & tzdriver version failed\n");
		return -1;
	}

    /* get tee version info */
    ret = ioctl(g_devFd, TEELOGGER_GET_VERSION, g_teeVersion);
    if (ret != 0) {
        tloge("get tee version failed %d\n", ret);
    }

    OpenTeeLog();
    return 0;
}

static void Destruct(void)
{
    if (g_logBuffer != NULL) {
        free(g_logBuffer);
        g_logBuffer = NULL;
    }

    if (g_files != NULL) {
        free(g_files);
        g_files = NULL;
    }

    if (g_devFd >= 0) {
        close(g_devFd);
        g_devFd = -1;
    }

    CloseTeeLog();
}

static void LogPrintTeeVersion(void)
{
    g_teeVersion[sizeof(g_teeVersion) - 1] = '\0';
    printf("%s\n", g_teeVersion);
}

#define SET_TLOGCAT_F 1
static int32_t LogCmdF(void)
{
    printf("HAVE option: -f\n");

    if (LogGetTlogcatF() == SET_TLOGCAT_F) {
        tlogd("tlogcat is running\n");
        printf("tlogcat -f is running, only one instance is allowed at the same time\n");
        return 0;
    } else {
        tlogd("tlogcat running prop has not been set, first time running tlogat -f\n");
    }

    if (LogSetTlogcatF() != 0) {
        tloge("set tlogcat running prop to 1 failed\n");
        return -1;
    } else {
        tlogi("set tlogcat running prop to 1 succ\n");
    }

    Func(true);
    return 0;
}

static bool g_defaultOp = true;
static int32_t SwitchSelect(int32_t ch)
{
    switch (ch) {
        case 'v':
            LogPrintTeeVersion();
            g_defaultOp = false;
            break;
        case 't':
            LogSetReadposCur();
            break;
        case 'f':
            if (LogCmdF() != 0) {
                return -1;
            }
            break;
        case 'h':
            printf("HAVE option: -h\n");
            HelpShow();
            break;
        default:
            printf("Unknown option: %c\n", (char)optopt);
            HelpShow();
            break;
    }
    return 0;
}

int32_t main(int32_t argc, char *argv[])
{
    printf("tlogcat start ++\n");
    int32_t ch;
    g_defaultOp = true;

    if (Prepare() < 0) {
        goto FREE_RES;
    }

    while ((ch = getopt(argc, argv, "f::ms:ghvt")) != -1) {
        g_defaultOp = false;
        if (optind > 0 && optind < argc) {
            tlogd("optind: %d, argc:%d, argv[%d]:%s\n", optind, argc, optind, argv[optind]);
        }
        if (SwitchSelect(ch) != 0) {
            tloge("option failed\n");
        }

        printf("----------------------------\n");
        if (optind > 0 && optind < argc) {
            tlogd("optind=%d, argv[%d]=%s\n", optind, optind, argv[optind]);
        }
    }

    if (g_defaultOp || g_readposCur != 0) {
        Func(false);
    }

    printf("tlogcat end --\n");

FREE_RES:
    Destruct();
    return 0;
}
