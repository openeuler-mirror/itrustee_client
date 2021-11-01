/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * iTrustee licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "fs_work_agent.h"
#include <errno.h>     /* for errno */
#include <sys/types.h> /* for open close */
#include <fcntl.h>
#include <sys/ioctl.h> /* for ioctl */
#include <time.h>
#include <dirent.h>
#include <sys/statfs.h>
#include <sys/resource.h>
#include <securec.h>
#include <mntent.h>

#include "tc_ns_client.h"
#include "tee_log.h"
#include "tee_ca_daemon.h"
#include "tee_agent.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG       "teecd_agent"
#define USER_PATH_LEN 10

/* record the current g_userId and g_storageId */
static uint32_t g_userId;
static uint32_t g_storageId;
static void SetCurrentUserId(uint32_t id)
{
    g_userId = id;
}
static uint32_t GetCurrentUserId(void)
{
    return g_userId;
}

static void SetCurrentStorageId(uint32_t id)
{
    g_storageId = id;
}
static uint32_t GetCurrentStorageId(void)
{
    return g_storageId;
}

int32_t IsUserDataReady(void)
{
    return 1;
}

/* open file list head */
static struct OpenedFile *g_firstFile = NULL;

/* add to tail */
static int32_t AddOpenFile(FILE *pFile)
{
    struct OpenedFile *newFile = malloc(sizeof(struct OpenedFile));
    if (newFile == NULL) {
        tloge("malloc OpenedFile failed\n");
        return -1;
    }
    newFile->file = pFile;

    if (g_firstFile == NULL) {
        g_firstFile   = newFile;
        newFile->next = newFile;
        newFile->prev = newFile;
    } else {
        if (g_firstFile->prev == NULL) {
            tloge("the tail is null\n");
            free(newFile);
            return -1;
        }
        g_firstFile->prev->next = newFile;
        newFile->prev           = g_firstFile->prev;
        newFile->next           = g_firstFile;
        g_firstFile->prev       = newFile;
    }
    return 0;
}

static void DelOpenFile(struct OpenedFile *file)
{
    struct OpenedFile *next = NULL;

    if (file == NULL) {
        return;
    }
    next = file->next;

    if (file == next) { /* only 1 node */
        g_firstFile = NULL;
    } else {
        if (file->next == NULL || file->prev == NULL) {
            tloge("the next or the prev is null\n");
            return;
        }
        if (file == g_firstFile) {
            g_firstFile = file->next;
        }
        next->prev       = file->prev;
        file->prev->next = next;
    }
}

static int32_t FindOpenFile(int32_t fd, struct OpenedFile **file)
{
    struct OpenedFile *p = g_firstFile;
    int32_t findFlag     = 0;

    if (p == NULL) {
        return findFlag;
    }

    do {
        if (p->file != NULL) {
            if (fileno(p->file) == fd) {
                findFlag = 1;
                break;
            }
        }
        p = p->next;
    } while (p != g_firstFile && p != NULL);

    if (findFlag == 0) {
        p = NULL;
    }
    if (file != NULL) {
        *file = p;
    }
    return findFlag;
}

/*
 * path:file or dir to change own
 * flag: 0(dir);1(file)
 */
static void ChownSecStorageDataToSystem(const char *path, bool flag)
{
    if (path == NULL) {
        return;
    }
    /*
     * check path is SEC_STORAGE_ROOT_DIR or SEC_STORAGE_DATA_DIR,
     * we only need to change SEC_STORAGE_DATA_DIR from root to system
     */
    if (strstr(path, "sec_storage_data") != NULL) {
        int32_t ret = chown(path, AID_SYSTEM, AID_SYSTEM);
        if (ret < 0) {
            tloge("chown erro\n");
        }
        if (flag) {
            ret = chmod(path, S_IRUSR | S_IWUSR);
        } else {
            ret = chmod(path, S_IRUSR | S_IWUSR | S_IXUSR);
        }
        if (ret < 0) {
            tloge("chmod erro\n");
        }
    }
}

static int32_t CheckPathLen(const char *path, size_t pathLen)
{
    uint32_t i = 0;

    while (i < pathLen && path[i] != '\0') {
        i++;
    }
    if (i >= pathLen) {
        tloge("path is too long\n");
        return -1;
    }
    return 0;
}

/*
 * path:file path name.
 * e.g. sec_storage_data/app1/sub1/fileA.txt
 * then CreateDir will make dir sec_storage_data, app1 and sub1.
 */
static int32_t CreateDir(const char *path, size_t pathLen)
{
    int32_t ret;

    ret = CheckPathLen(path, pathLen);
    if (ret != 0) {
        return -1;
    }

    char *pathTemp = strdup(path);
    char *position  = pathTemp;

    if (pathTemp == NULL) {
        tloge("strdup error\n");
        return -1;
    }

    if (strncmp(pathTemp, "/", strlen("/")) == 0) {
        position++;
    } else if (strncmp(pathTemp, "./", strlen("./")) == 0) {
        position += strlen("./");
    }

    for (; *position != '\0'; ++position) {
        if (*position == '/') {
            *position = '\0';

            if (access(pathTemp, F_OK) == 0) {
                *position = '/';
                continue;
            }

            if (mkdir(pathTemp, ROOT_DIR_PERM) != 0) {
                tloge("mkdir fail\n");
                free(pathTemp);
                return -1;
            }

            ChownSecStorageDataToSystem(pathTemp, false);
            *position = '/';
        }
    }

    free(pathTemp);
    return 0;
}

static int32_t CheckFileNameAndPath(const char *name, const char *path)
{
    if (name == NULL || path == NULL) {
        return -1;
    }

    if (strstr(name, FILE_NAME_INVALID_STR) != NULL) {
        tloge("Invalid file name\n");
        return -1;
    }

    return 0;
}

#ifdef SEC_STORAGE_DATA_MDC_PATH
static int32_t CheckEnvPath(const char* envPath, char* trustPath, size_t trustPathLen)
{
    struct stat st;

    if (strlen(envPath) > trustPathLen) {
        tloge("too long envPath\n");
        return -1;
    }
    char *retPath = realpath(envPath, trustPath);
    if (retPath == NULL) {
        tloge("error envpath\n");
        return -1;
    }

    if (stat(trustPath, &st) < 0) {
        tloge("stat failed, errno is %x\n", errno);
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        tloge("error path: is not a dir\n");
        return -1;
    }
    size_t pathLen = strlen(trustPath);
    if (pathLen >= trustPathLen - 1) {
        tloge("too long to add / \n");
        return -1;
    }
    trustPath[pathLen] = '/';
    trustPath[pathLen + 1] = '\0';
    return 0;
}
#endif

static int32_t GetPathStorage(char *path, size_t pathLen, const char *env)
{
    errno_t rc;
    char *defaultPath = NULL;

    if (strncmp(env, "SFS_PARTITION_TRANSIENT", strlen("SFS_PARTITION_TRANSIENT")) == 0) {
        defaultPath = USER_DATA_DIR;
    } else if (strncmp(env, "SFS_PARTITION_PERSISTENT", strlen("SFS_PARTITION_PERSISTENT")) == 0) {
        defaultPath = ROOT_DIR;
    } else {
        return -1;
    }
#ifdef SEC_STORAGE_DATA_MDC_PATH
    const char *envPath = getenv(env);
    if (envPath == NULL) {
        tloge("envPath is NULL.\n");
        return -1;
    }
    char trustPath[PATH_MAX] = { 0 };
    if (CheckEnvPath(envPath, trustPath, PATH_MAX) != 0) {
        return -1;
    }
    defaultPath = trustPath;
#endif
    rc = strncpy_s(path, pathLen, defaultPath, strlen(defaultPath) + 1);
    if (rc != EOK) {
        tloge("strncpy_s failed %d\n", rc);
        return -1;
    }
    return 0;
}

static int32_t GetTransientDir(char* path, size_t pathLen)
{
    return GetPathStorage(path, pathLen, "SFS_PARTITION_TRANSIENT");
}

static int32_t GetPersistentDir(char* path, size_t pathLen)
{
    return GetPathStorage(path, pathLen, "SFS_PARTITION_PERSISTENT");
}

#define USER_PATH_SIZE 10
static int32_t JoinFileNameTransient(const char *name, char *path, size_t pathLen)
{
    errno_t rc;
    int32_t ret;
    uint32_t userId = GetCurrentUserId();

    ret = GetTransientDir(path, pathLen);
    if (ret != 0)
        return ret;

    if (userId != 0) {
        char userPath[USER_PATH_SIZE] = { 0 };
        rc = snprintf_s(userPath, sizeof(userPath), sizeof(userPath) - 1, "%u/", userId);
        if (rc == -1) {
            tloge("snprintf_s failed %d\n", rc);
            return -1;
        }

        rc = strncat_s(path, pathLen, SFS_PARTITION_USER_SYMLINK, strlen(SFS_PARTITION_USER_SYMLINK));
        if (rc != EOK) {
            tloge("strncat_s failed %d\n", rc);
            return -1;
        }

        rc = strncat_s(path, pathLen, userPath, strlen(userPath));
        if (rc != EOK) {
            tloge("strncat_s failed %d\n", rc);
            return -1;
        }

        if (strlen(name) <= strlen(SFS_PARTITION_TRANSIENT)) {
            tloge("name length is too small\n");
            return -1;
        }
        rc = strncat_s(path, pathLen, name + strlen(SFS_PARTITION_TRANSIENT),
                       (strlen(name) - strlen(SFS_PARTITION_TRANSIENT)));
        if (rc != EOK) {
            tloge("strncat_s failed %d\n", rc);
            return -1;
        }
    } else {
        rc = strncat_s(path, pathLen, name, strlen(name));
        if (rc != EOK) {
            tloge("strncat_s failed %d\n", rc);
            return -1;
        }
    }

    return 0;
}

static int32_t GetDefaultDir(char *path, size_t pathLen)
{
    errno_t rc;
    int32_t ret;

    ret = GetPersistentDir(path, pathLen);
    if (ret != 0)
        return ret;

    rc = strncat_s(path, pathLen, SFS_PARTITION_PERSISTENT, strlen(SFS_PARTITION_PERSISTENT));
    if (rc != EOK) {
        tloge("strncat_s failed %d\n", rc);
        return -1;
    }
    return 0;
}

static int32_t DoJoinFileName(const char *name, char *path, size_t pathLen)
{
    errno_t rc;
    int32_t ret;

    if (name == strstr(name, SFS_PARTITION_TRANSIENT_PERSO) || name == strstr(name, SFS_PARTITION_TRANSIENT_PRIVATE)) {
        ret = GetTransientDir(path, pathLen);
    } else if (name == strstr(name, SFS_PARTITION_PERSISTENT)) {
        ret = GetPersistentDir(path, pathLen);
    } else {
        ret = GetDefaultDir(path, pathLen);
    }

    if (ret != 0) {
        tloge("get dir failed %d\n", ret);
        return -1;
    }

    rc = strncat_s(path, pathLen, name, strlen(name));
    if (rc != EOK) {
        tloge("strncat_s failed %d\n", rc);
        return -1;
    }

    return 0;
}

static int32_t JoinFileNameForStorageCE(const char *name, char *path, size_t pathLen)
{
    errno_t rc;
    char temp[FILE_NAME_MAX_BUF] = { '\0' };
    char *nameTemp               = temp;
    char *nameWithoutUserId      = NULL;
    char *idString               = NULL;

    rc = memcpy_s(nameTemp, FILE_NAME_MAX_BUF, name, strlen(name));
    if (rc != EOK) {
        tloge("copy failed");
        return -1;
    }

    idString = strtok_r(nameTemp, ROOT_DIR, &nameWithoutUserId);
    if (idString == NULL) {
        tloge("the name %s does not match th rule as userid/xxx\n", name);
        return -1;
    }

    rc = strncpy_s(path, pathLen, SEC_STORAGE_DATA_CE, sizeof(SEC_STORAGE_DATA_CE));
    if (rc != EOK) {
        tloge("strncpy_s failed %d\n", rc);
        return -1;
    }

    rc = strncat_s(path, pathLen, idString, strlen(idString));
    if (rc != EOK) {
        tloge("strncat_s failed %d\n", rc);
        return -1;
    }

    rc = strncat_s(path, pathLen, SEC_STORAGE_DATA_ROOT_DIR, sizeof(SEC_STORAGE_DATA_ROOT_DIR));
    if (rc != EOK) {
        tloge("strncat_s failed %d\n", rc);
        return -1;
    }

    rc = strncat_s(path, pathLen, nameWithoutUserId, strlen(nameWithoutUserId));
    if (rc != EOK) {
        tloge("strncat_s failed %d\n", rc);
        return -1;
    }

    return 0;
}

static int32_t JoinFileName(const char *name, char *path, size_t pathLen)
{
    int32_t ret = -1;
    uint32_t storageId = GetCurrentStorageId();

    if (CheckFileNameAndPath(name, path) != 0) {
        return ret;
    }

    if (storageId == TEE_OBJECT_STORAGE_CE) {
        ret = JoinFileNameForStorageCE(name, path, pathLen);
    } else {
        /*
        * If the path name does not start with sec_storage or sec_storage_data,
        * add sec_storage str for the path
        */
        if (name == strstr(name, SFS_PARTITION_TRANSIENT)) {
            ret = JoinFileNameTransient(name, path, pathLen);
        } else {
            ret = DoJoinFileName(name, path, pathLen);
        }
    }

    tlogv("joined path done\n");
    return ret;
}

static bool IsDataDir(const char *path, bool isUsers)
{
    char secDataDir[FILE_NAME_MAX_BUF] = { 0 };
    int32_t ret;
    errno_t rc;

    ret = GetTransientDir(secDataDir, FILE_NAME_MAX_BUF);
    if (ret != 0)
        return false;
    if (isUsers) {
        rc = strncat_s(secDataDir, FILE_NAME_MAX_BUF, SFS_PARTITION_USER_SYMLINK, strlen(SFS_PARTITION_USER_SYMLINK));
    } else {
        rc = strncat_s(secDataDir, FILE_NAME_MAX_BUF, SFS_PARTITION_TRANSIENT, strlen(SFS_PARTITION_TRANSIENT));
    }
    if (rc != EOK)
        return false;
    if (path == strstr(path, secDataDir))
        return true;
    return false;
}

static bool IsRootDir(const char *path)
{
    char secRootDir[FILE_NAME_MAX_BUF] = { 0 };
    int32_t ret;
    errno_t rc;

    ret = GetPersistentDir(secRootDir, FILE_NAME_MAX_BUF);
    if (ret != 0)
        return false;
    rc = strncat_s(secRootDir, FILE_NAME_MAX_BUF, SFS_PARTITION_PERSISTENT, strlen(SFS_PARTITION_PERSISTENT));
    if (rc != EOK)
        return false;
    if (path == strstr(path, secRootDir))
        return true;
    return false;
}

static bool IsValidFilePath(const char *path)
{
    if (IsDataDir(path, false) || IsDataDir(path, true) || IsRootDir(path) ||
        (path == strstr(path, SEC_STORAGE_DATA_CE))) {
        return true;
    }
    tloge("path is invalid\n");
    return false;
}

static uint32_t GetRealFilePath(const char *originPath, char *trustPath, size_t tPathLen)
{
    char *retPath = realpath(originPath, trustPath);
    if (retPath == NULL) {
        /* the file may be not exist, will create after */
        if ((errno != ENOENT) && (errno != EACCES)) {
            tloge("get realpath failed: %d\n", errno);
            return (uint32_t)errno;
        }
        /* check origin path */
        if (!IsValidFilePath(originPath)) {
            tloge("origin path is invalid\n");
            return ENFILE;
        }
        errno_t rc = strncpy_s(trustPath, tPathLen, originPath, strlen(originPath));
        if (rc != EOK) {
            tloge("strncpy_s failed %d\n", rc);
            return EPERM;
        }
    } else {
        /* check real path */
        if (!IsValidFilePath(trustPath)) {
            tloge("path is invalid\n");
            return ENFILE;
        }
    }
    return 0;
}

static int32_t UnlinkRecursive(const char *name);
static int32_t UnlinkRecursiveDir(const char *name)
{
    bool fail         = false;
    char dn[PATH_MAX] = { 0 };
    errno_t rc;

    /* a directory, so open handle */
    DIR *dir = opendir(name);
    if (dir == NULL) {
        tloge("dir open failed\n");
        return -1;
    }

    /* recurse over components */
    errno = 0;

    struct dirent *de = readdir(dir);

    while (de != NULL) {
        if (strncmp(de->d_name, "..", sizeof("..")) == 0 || strncmp(de->d_name, ".", sizeof(".")) == 0) {
            de = readdir(dir);
            continue;
        }
        rc = snprintf_s(dn, sizeof(dn), sizeof(dn) - 1, "%s/%s", name, de->d_name);
        if (rc == -1) {
            tloge("snprintf_s failed %d\n", rc);
            fail = true;
            break;
        }

        if (UnlinkRecursive(dn) < 0) {
            tloge("loop UnlinkRecursive() failed, there are read-only file\n");
            fail = true;
            break;
        }
        errno = 0;
        de    = readdir(dir);
    }

    /* in case readdir or UnlinkRecursive failed */
    if (fail || errno < 0) {
        int32_t save = errno;
        closedir(dir);
        errno = save;
        tloge("fail is %d, errno is %d\n", fail, errno);
        return -1;
    }

    /* close directory handle */
    if (closedir(dir) < 0) {
        tloge("closedir failed, errno is %d\n", errno);
        return -1;
    }

    /* delete target directory */
    if (rmdir(name) < 0) {
        tloge("rmdir failed, errno is %d\n", errno);
        return -1;
    }
    return 0;
}

static int32_t UnlinkRecursive(const char *name)
{
    struct stat st;

    /* is it a file or directory? */
    if (lstat(name, &st) < 0) {
        tloge("lstat failed, errno is %x\n", errno);
        return -1;
    }

    /* a file, so unlink it */
    if (!S_ISDIR(st.st_mode)) {
        if (unlink(name) < 0) {
            tloge("unlink failed, errno is %d\n", errno);
            return -1;
        }
        return 0;
    }

    return UnlinkRecursiveDir(name);
}

static int32_t IsFileExist(const char *name)
{
    struct stat statbuf;

    if (name == NULL) {
        return 0;
    }
    if (stat(name, &statbuf) != 0) {
        if (errno == ENOENT) { /* file not exist */
            tloge("file stat failed\n");
            return 0;
        }
        return 1;
    }

    return 1;
}

static uint32_t DoOpenFile(const char *path, struct SecStorageType *transControl)
{
    char trustPath[PATH_MAX] = { 0 };

    uint32_t rRet = GetRealFilePath(path, trustPath, sizeof(trustPath));
    if (rRet != 0) {
        tloge("get real path failed. err=%u\n", rRet);
        return rRet;
    }

    FILE *pFile = fopen(trustPath, transControl->args.open.mode);
    if (pFile == NULL) {
        tloge("open file with flag %s failed: %d\n", transControl->args.open.mode, errno);
        return (uint32_t)errno;
    }
    ChownSecStorageDataToSystem(trustPath, true);
    int32_t ret = AddOpenFile(pFile);
    if (ret != 0) {
        tloge("add OpenedFile failed\n");
        (void)fclose(pFile);
        return ENOMEM;
    }
    transControl->ret = fileno(pFile); /* return fileno */
    return 0;
}

static int32_t CheckPartitionReady(const char *mntDir)
{
    (void)mntDir;
    return 1;
}

static void OpenWork(struct SecStorageType *transControl)
{
    uint32_t error;
    char nameBuff[FILE_NAME_MAX_BUF] = { 0 };

    SetCurrentUserId(transControl->userId);
    SetCurrentStorageId(transControl->storageId);

    tlogv("sec storage : open\n");

    if (strstr((char *)(transControl->args.open.name), SFS_PARTITION_PERSISTENT) != NULL) {
        if (CheckPartitionReady("/sec_storage") <= 0) {
            tloge("check /sec_storage partition_ready failed ----------->\n");
            transControl->ret = -1;
            return;
        }
    }

    if (JoinFileName((char *)(transControl->args.open.name), nameBuff, sizeof(nameBuff)) != 0) {
        transControl->ret = -1;
        return;
    }

    if (transControl->cmd == SEC_CREATE) {
        /* create a exist file, remove it at first */
        errno_t rc = strncpy_s(transControl->args.open.mode,
            sizeof(transControl->args.open.mode), "w+", sizeof("w+"));
        if (rc != EOK) {
            tloge("strncpy_s failed %d\n", rc);
            error = ENOENT;
            goto ERROR;
        }
    } else {
        if (IsFileExist(nameBuff) == 0) {
            /* open a nonexist file, return fail */
            tloge("file is not exist, open failed\n");
            error = ENOENT;
            goto ERROR;
        }
    }

    /* mkdir -p for new create files */
    if (CreateDir(nameBuff, sizeof(nameBuff)) != 0) {
        error = (uint32_t)errno;
        goto ERROR;
    }

    error = DoOpenFile(nameBuff, transControl);
    if (error != 0) {
        goto ERROR;
    }

    return;

ERROR:
    transControl->ret   = -1;
    transControl->error = error;
    return;
}

static void CloseWork(struct SecStorageType *transControl)
{
    int32_t ret;
    struct OpenedFile *selFile = NULL;

    tlogv("sec storage : close\n");

    if (FindOpenFile(transControl->args.close.fd, &selFile) != 0) {
        ret = fclose(selFile->file);
        if (ret == 0) {
            tlogv("close file %d success\n", transControl->args.close.fd);
            DelOpenFile(selFile);
            free(selFile);
            selFile = NULL;
            (void)selFile;
        } else {
            tloge("close file %d failed: %d\n", transControl->args.close.fd, errno);
            transControl->error = (uint32_t)errno;
        }
        transControl->ret = ret;
    } else {
        tloge("can't find opened file(fileno = %d)\n", transControl->args.close.fd);
        transControl->ret   = -1;
        transControl->error = EBADF;
    }
}

static void ReadWork(struct SecStorageType *transControl)
{
    struct OpenedFile *selFile = NULL;
    size_t count;

    tlogv("sec storage : read count = %u\n", transControl->args.read.count);

    if (FindOpenFile(transControl->args.read.fd, &selFile) != 0) {
        count = fread((void *)(transControl->args.read.buffer), 1, transControl->args.read.count, selFile->file);
        transControl->ret = (int32_t)count;

        if (count < transControl->args.read.count) {
            if (feof(selFile->file)) {
                transControl->ret2 = 0;
                tlogv("read end of file\n");
            } else {
                transControl->ret2  = -1;
                transControl->error = (uint32_t)errno;
                tloge("read file failed: %d\n", errno);
            }
        } else {
            transControl->ret2 = 0;
            tlogv("read file success, content len=%zu\n", count);
        }
    } else {
        transControl->ret   = 0;
        transControl->ret2  = -1;
        transControl->error = EBADF;
        tloge("can't find opened file(fileno = %d)\n", transControl->args.read.fd);
    }
}

static void WriteWork(struct SecStorageType *transControl)
{
    struct OpenedFile *selFile = NULL;
    size_t count;

    tlogv("sec storage : write count = %u\n", transControl->args.write.count);

    if (FindOpenFile(transControl->args.write.fd, &selFile) != 0) {
        count = fwrite((void *)(transControl->args.write.buffer), 1, transControl->args.write.count, selFile->file);
        if (count < transControl->args.write.count) {
            tloge("write file failed: %d\n", errno);
            transControl->ret   = (int32_t)count;
            transControl->error = (uint32_t)errno;
            return;
        }

        if (transControl->ret2 == SEC_WRITE_SSA) {
            if (fflush(selFile->file) != 0) {
                tloge("fflush file failed: %d\n", errno);
                transControl->ret   = 0;
                transControl->error = (uint32_t)errno;
            } else {
                transControl->ret = (int32_t)count;
            }
        } else {
            transControl->ret   = (int32_t)count;
            transControl->error = 0;
        }
    } else {
        tloge("can't find opened file(fileno = %d)\n", transControl->args.write.fd);
        transControl->ret   = 0;
        transControl->error = EBADF;
    }
}

static void SeekWork(struct SecStorageType *transControl)
{
    int32_t ret;
    struct OpenedFile *selFile = NULL;

    tlogv("sec storage : seek offset=%d, whence=%u\n", transControl->args.seek.offset, transControl->args.seek.whence);

    if (FindOpenFile(transControl->args.seek.fd, &selFile) != 0) {
        ret = fseek(selFile->file, transControl->args.seek.offset, (int32_t)transControl->args.seek.whence);
        if (ret) {
            tloge("seek file failed: %d\n", errno);
            transControl->error = (uint32_t)errno;
        } else {
            tlogv("seek file success\n");
        }
        transControl->ret = ret;
    } else {
        tloge("can't find opened file(fileno = %d)\n", transControl->args.seek.fd);
        transControl->ret   = -1;
        transControl->error = EBADF;
    }
}

static void RemoveWork(struct SecStorageType *transControl)
{
    int32_t ret;
    char nameBuff[FILE_NAME_MAX_BUF] = { 0 };

    tlogv("sec storage : remove\n");

    SetCurrentUserId(transControl->userId);
    SetCurrentStorageId(transControl->storageId);

    if (JoinFileName((char *)(transControl->args.remove.name), nameBuff, sizeof(nameBuff)) == 0) {
        ret = UnlinkRecursive(nameBuff);
        if (ret != 0) {
            tloge("remove file failed: %d\n", errno);
            transControl->error = (uint32_t)errno;
        } else {
            tlogv("remove file success\n");
        }
        transControl->ret = ret;
    } else {
        transControl->ret = -1;
    }
}

static void TruncateWork(struct SecStorageType *transControl)
{
    int32_t ret;
    char nameBuff[FILE_NAME_MAX_BUF] = { 0 };

    tlogv("sec storage : truncate, len=%u\n", transControl->args.truncate.len);

    SetCurrentUserId(transControl->userId);
    SetCurrentStorageId(transControl->storageId);

    if (JoinFileName((char *)(transControl->args.truncate.name), nameBuff, sizeof(nameBuff)) == 0) {
        ret = truncate(nameBuff, (long)transControl->args.truncate.len);
        if (ret != 0) {
            tloge("truncate file failed: %d\n", errno);
            transControl->error = (uint32_t)errno;
        } else {
            tlogv("truncate file success\n");
        }
        transControl->ret = ret;
    } else {
        transControl->ret = -1;
    }
}

static void RenameWork(struct SecStorageType *transControl)
{
    int32_t ret;
    char nameBuff[FILE_NAME_MAX_BUF]  = { 0 };
    char nameBuff2[FILE_NAME_MAX_BUF] = { 0 };

    SetCurrentUserId(transControl->userId);
    SetCurrentStorageId(transControl->storageId);

    int32_t joinRet1 = JoinFileName((char *)(transControl->args.rename.buffer), nameBuff, sizeof(nameBuff));
    int32_t joinRet2 = JoinFileName((char *)(transControl->args.rename.buffer) + transControl->args.rename.oldNameLen,
                                    nameBuff2, sizeof(nameBuff2));
    if (joinRet1 == 0 && joinRet2 == 0) {
        ret = rename(nameBuff, nameBuff2);
        if (ret != 0) {
            tloge("rename file failed: %d\n", errno);
            transControl->error = (uint32_t)errno;
        } else {
            tlogv("rename file success\n");
        }
        transControl->ret = ret;
    } else {
        transControl->ret = -1;
    }
}

#define MAXBSIZE 65536

static int32_t DoCopy(int32_t fromFd, int32_t toFd)
{
    int32_t ret;
    ssize_t rcount;
    ssize_t wcount;

    char *buf = (char *)malloc(MAXBSIZE * sizeof(char));
    if (buf == NULL) {
        tloge("malloc buf failed\n");
        return -1;
    }

    rcount = read(fromFd, buf, MAXBSIZE);
    while (rcount > 0) {
        wcount = write(toFd, buf, (size_t)rcount);
        if (rcount != wcount || wcount == -1) {
            tloge("write file failed: %d\n", errno);
            ret = -1;
            goto OUT;
        }
        rcount = read(fromFd, buf, MAXBSIZE);
    }

    if (rcount < 0) {
        tloge("read file failed: %d\n", errno);
        ret = -1;
        goto OUT;
    }

    /* fsync memory from kernel to disk */
    ret = fsync(toFd);
    if (ret != 0) {
        tloge("CopyFile:fsync file failed: %d\n", errno);
        goto OUT;
    }

OUT:
    free(buf);
    return ret;
}

static int32_t CopyFile(const char *fromPath, const char *toPath)
{
    struct stat fromStat;
    char realFromPath[PATH_MAX] = { 0 };
    char realToPath[PATH_MAX]   = { 0 };

    uint32_t rRet = GetRealFilePath(fromPath, realFromPath, sizeof(realFromPath));
    if (rRet != 0) {
        tloge("get real from path failed. err=%u\n", rRet);
        return -1;
    }

    rRet = GetRealFilePath(toPath, realToPath, sizeof(realToPath));
    if (rRet != 0) {
        tloge("get real to path failed. err=%u\n", rRet);
        return -1;
    }

    int32_t fromFd = open(realFromPath, O_RDONLY, 0);
    if (fromFd == -1) {
        tloge("open file failed: %d\n", errno);
        return -1;
    }

    int32_t ret = fstat(fromFd, &fromStat);
    if (ret == -1) {
        tloge("stat file failed: %d\n", errno);
        close(fromFd);
        return ret;
    }

    int32_t toFd = open(realToPath, O_WRONLY | O_TRUNC | O_CREAT, fromStat.st_mode);
    if (toFd == -1) {
        tloge("stat file failed: %d\n", errno);
        close(fromFd);
        return -1;
    }

    ret = DoCopy(fromFd, toFd);
    if (ret != 0) {
        tloge("do copy failed\n");
    } else {
        ChownSecStorageDataToSystem((char *)realToPath, true);
    }

    close(fromFd);
    close(toFd);
    return ret;
}

static void CopyWork(struct SecStorageType *transControl)
{
    int32_t ret;
    char fromPath[FILE_NAME_MAX_BUF] = { 0 };
    char toPath[FILE_NAME_MAX_BUF]   = { 0 };

    SetCurrentUserId(transControl->userId);
    SetCurrentStorageId(transControl->storageId);

    int32_t joinRet1 = JoinFileName((char *)(transControl->args.cp.buffer), fromPath, sizeof(fromPath));
    int32_t joinRet2 = JoinFileName((char *)(transControl->args.cp.buffer) + transControl->args.cp.fromPathLen, toPath,
                                    sizeof(toPath));
    if (joinRet1 == 0 && joinRet2 == 0) {
        ret = CopyFile(fromPath, toPath);
        if (ret != 0) {
            tloge("copy file failed: %d\n", errno);
            transControl->error = (uint32_t)errno;
        } else {
            tlogv("copy file success\n");
        }
        transControl->ret = ret;
    } else {
        transControl->ret = -1;
    }
}

static void FileInfoWork(struct SecStorageType *transControl)
{
    int32_t ret;
    struct OpenedFile *selFile = NULL;
    struct stat statBuff;

    tlogv("sec storage : file info\n");

    transControl->args.info.fileLen = transControl->args.info.curPos = 0;

    if (FindOpenFile(transControl->args.info.fd, &selFile) != 0) {
        ret = fstat(transControl->args.info.fd, &statBuff);
        if (ret == 0) {
            transControl->args.info.fileLen = (uint32_t)statBuff.st_size;
            transControl->args.info.curPos  = (uint32_t)ftell(selFile->file);
        } else {
            tloge("fstat file failed: %d\n", errno);
            transControl->error = (uint32_t)errno;
        }
        transControl->ret = ret;
    } else {
        transControl->ret   = -1;
        transControl->error = EBADF;
    }
}

static void FileAccessWork(struct SecStorageType *transControl)
{
    int32_t ret;
    char nameBuff[FILE_NAME_MAX_BUF] = { 0 };

    tlogv("sec storage : file access\n");

    if (transControl->cmd == SEC_ACCESS) {
        SetCurrentUserId(transControl->userId);
        SetCurrentStorageId(transControl->storageId);

        if (JoinFileName((char *)(transControl->args.access.name), nameBuff, sizeof(nameBuff)) == 0) {
            ret = access(nameBuff, transControl->args.access.mode);
            if (ret < 0) {
                tloge("access file mode %d failed: %d\n", transControl->args.access.mode, errno);
            }
            transControl->ret   = ret;
            transControl->error = (uint32_t)errno;
        } else {
            transControl->ret = -1;
        }
    } else {
        ret = access((char *)(transControl->args.access.name), transControl->args.access.mode);
        if (ret < 0) {
            tloge("access2 file mode %d failed: %d\n", transControl->args.access.mode, errno);
        }
        transControl->ret   = ret;
        transControl->error = (uint32_t)errno;
    }
}

static void FsyncWork(struct SecStorageType *transControl)
{
    int32_t ret;
    int32_t fd                 = -1;
    struct OpenedFile *selFile = NULL;

    tlogv("sec storage : file fsync\n");

    /* opened file */
    if (transControl->args.fsync.fd != 0 && FindOpenFile(transControl->args.fsync.fd, &selFile) != 0) {
        /* first,flush memory from user to kernel */
        ret = fflush(selFile->file);
        if (ret != 0) {
            tloge("fsync:fflush file failed: %d\n", errno);
            transControl->ret   = -1;
            transControl->error = (uint32_t)errno;
            return;
        }

        /* second,fsync memory from kernel to disk */
        fd  = fileno(selFile->file);
        ret = fsync(fd);
        if (ret != 0) {
            tloge("fsync:fsync file failed: %d\n", errno);
            transControl->ret   = -1;
            transControl->error = (uint32_t)errno;
            return;
        }

        transControl->ret = 0;
        tlogv("fsync file(%d) success\n", transControl->args.fsync.fd);
    } else {
        tloge("can't find opened file(fileno = %d)\n", transControl->args.fsync.fd);
        transControl->ret   = -1;
        transControl->error = EBADF;
    }
}

#define KBYTE 1024
static void DiskUsageWork(struct SecStorageType *transControl)
{
    struct statfs st;
    uint32_t dataRemain;
    uint32_t secStorageRemain;
    char nameBuff[FILE_NAME_MAX_BUF] = { 0 };

    tlogv("sec storage : disk usage\n");
    if (GetTransientDir(nameBuff, FILE_NAME_MAX_BUF) != 0)
        goto ERROR;
    if (statfs((const char*)nameBuff, &st) < 0) {
        tloge("statfs /secStorageData failed, err=%d\n", errno);
        goto ERROR;
    }
    dataRemain = (long)st.f_bfree * (long)st.f_bsize / KBYTE;

    if (GetPersistentDir(nameBuff, FILE_NAME_MAX_BUF) != 0) {
        tloge("get persistent dir error.\n");
        goto ERROR;
    }
    if (strncat_s(nameBuff, FILE_NAME_MAX_BUF, SFS_PARTITION_PERSISTENT, strlen(SFS_PARTITION_PERSISTENT)) != EOK) {
        tloge("strncat_s error.\n");
        goto ERROR;
    }
    if (statfs((const char*)nameBuff, &st) < 0) {
        tloge("statfs /secStorage failed, err=%d\n", errno);
        goto ERROR;
    }
    secStorageRemain = (long)st.f_bfree * (long)st.f_bsize / KBYTE;

    transControl->ret                       = 0;
    transControl->args.diskUsage.data       = dataRemain;
    transControl->args.diskUsage.secStorage = secStorageRemain;
    return;

ERROR:
    transControl->ret   = -1;
    transControl->error = (uint32_t)errno;
}

static void DeleteAllWork(struct SecStorageType *transControl)
{
    int32_t ret;
    char path[FILE_NAME_MAX_BUF] = { 0 };
    char *pathIn                 = (char *)(transControl->args.deleteAll.path);
    SetCurrentUserId(transControl->userId);

    tlogv("sec storage : delete path, userid:%d\n", transControl->userId);

    ret = DoJoinFileName(pathIn, path, sizeof(path));
    if (ret != EOK) {
        tloge("join name failed %d\n", ret);
        transControl->ret = -1;
        return;
    }

    tlogv("sec storage : joint delete path\n");

    ret = UnlinkRecursive(path);
    if (ret != 0) {
        tloge("delete file failed: %d\n", errno);
        transControl->error = (uint32_t)errno;
    } else {
        tloge("delete file success\n");
    }
    transControl->ret = ret;
}

typedef void (*FsWorkFunc)(struct SecStorageType *transControl);

struct FsWorkTbl {
    enum FsCmdType cmd;
    FsWorkFunc fn;
};

static const struct FsWorkTbl g_fsWorkTbl[] = {
    { SEC_OPEN, OpenWork },           { SEC_CLOSE, CloseWork },
    { SEC_READ, ReadWork },           { SEC_WRITE, WriteWork },
    { SEC_SEEK, SeekWork },           { SEC_REMOVE, RemoveWork },
    { SEC_TRUNCATE, TruncateWork },   { SEC_RENAME, RenameWork },
    { SEC_CREATE, OpenWork },         { SEC_INFO, FileInfoWork },
    { SEC_ACCESS, FileAccessWork },   { SEC_ACCESS2, FileAccessWork },
    { SEC_FSYNC, FsyncWork },         { SEC_CP, CopyWork },
    { SEC_DISKUSAGE, DiskUsageWork }, { SEC_DELETE_ALL, DeleteAllWork },
};

void *FsWorkThread(void *control)
{
    struct SecStorageType *transControl = NULL;
    int32_t ret;
    int32_t fsFd;

    if (control == NULL) {
        return NULL;
    }
    transControl = control;

    fsFd = GetFsFd();
    if (fsFd == -1) {
        tloge("fs is not open\n");
        return NULL;
    }

    transControl->magic = AGENT_FS_ID;
    while (1) {
        tlogv("++ fs agent loop ++\n");
        ret = ioctl(fsFd, (int32_t)TC_NS_CLIENT_IOCTL_WAIT_EVENT, AGENT_FS_ID);
        if (ret != 0) {
            tloge("fs agent  wait event failed\n");
            break;
        }

        tlogv("fs agent wake up and working!!\n");

        if (IsUserDataReady() == 0) {
            transControl->ret = -1;
            tloge("do secure storage while userdata is not ready, skip!\n");
            goto FILE_WORK_DONE;
        }

        if ((transControl->cmd < SEC_MAX) && (g_fsWorkTbl[transControl->cmd].fn != NULL)) {
            g_fsWorkTbl[transControl->cmd].fn(transControl);
        } else {
            tloge("fs agent error cmd:transControl->cmd=%x\n", transControl->cmd);
        }

    FILE_WORK_DONE:

        __asm__ volatile("isb");
        __asm__ volatile("dsb sy");

        transControl->magic = AGENT_FS_ID;

        __asm__ volatile("isb");
        __asm__ volatile("dsb sy");

        ret = ioctl(fsFd, (int32_t)TC_NS_CLIENT_IOCTL_SEND_EVENT_RESPONSE, AGENT_FS_ID);
        if (ret != 0) {
            tloge("fs agent send reponse failed\n");
            break;
        }
        tlogv("-- fs agent loop --\n");
    }

    return NULL;
}

void SetFileNumLimit(void)
{
    struct rlimit rlim, rlimNew;

    int32_t rRet = getrlimit(RLIMIT_NOFILE, &rlim);
    if (rRet == 0) {
        rlimNew.rlim_cur = rlimNew.rlim_max = FILE_NUM_LIMIT_MAX;
        if (setrlimit(RLIMIT_NOFILE, &rlimNew) != 0) {
            rlimNew.rlim_cur = rlimNew.rlim_max = rlim.rlim_max;
            (void)setrlimit(RLIMIT_NOFILE, &rlimNew);
        }
    } else {
        tloge("getrlimit error is 0x%x, errno is %x", rRet, errno);
    }
}
