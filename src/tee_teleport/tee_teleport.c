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

#include "tee_teleport.h"
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>

#ifdef CROSS_DOMAIN_PERF
#include "posix_proxy.h"
#endif

#include "dir.h"

enum CliArgType {
    ARG_CLEAN,
    ARG_GRPID,
    ARG_NSID_SET,
    ARG_CONF_RES,
    ARG_VMID,
    ARG_MEMORY,
    ARG_DISK_SIZE,
    ARG_CPUSET,
    ARG_CPUS,
    ARG_CONTAINER,
    ARG_CONF_CONT,
    ARG_GET_LOG,
    ARG_DEL_LOG
};

static const struct option g_toolOptions[] = {{"install",             required_argument, NULL, 's'},
                                              {"import",              required_argument, NULL, 'm'},
                                              {"type",                required_argument, NULL, 't'},
                                              {"create",              required_argument, NULL, 'c'},
                                              {"run",                 required_argument, NULL, 'r'},
                                              {"id",                  required_argument, NULL, 'i'},
                                              {"save",                required_argument, NULL, 'v'},
                                              {"parameter",           required_argument, NULL, 'p'},
                                              {"optimization",        required_argument, NULL, 'z'},
                                              {"destroy",             required_argument, NULL, 'e'},
                                              {"uninstall",           required_argument, NULL, 'u'},
                                              {"list",                required_argument, NULL, 'l'},
                                              {"help",                required_argument, NULL, 'h'},
                                              {"env",                 required_argument, NULL, 'f'},
                                              {"getlog",              no_argument, NULL, ARG_GET_LOG},
                                              {"deletelog",           no_argument, NULL, ARG_DEL_LOG},
                                              {"clean",               no_argument, NULL, ARG_CLEAN},
                                              {"grpid",               required_argument, NULL, ARG_GRPID},
                                              {"nsid",                required_argument, NULL, ARG_NSID_SET},
                                              {"config-container",    no_argument, NULL, ARG_CONF_CONT},
                                              {"containerid",         required_argument, NULL, ARG_CONTAINER},
                                              {"config-resource",     no_argument, NULL, ARG_CONF_RES},
                                              {"memory",              required_argument, NULL, ARG_MEMORY},
                                              {"cpuset-cpus",         required_argument, NULL, ARG_CPUSET},
                                              {"cpus",                required_argument, NULL, ARG_CPUS},
                                              {"disk-size",           required_argument, NULL, ARG_DISK_SIZE},
                                              {"vmid",                required_argument, NULL, ARG_VMID},
                                              {NULL, 0, NULL, 0}};

static int32_t PrintUsage(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    (void)args;
    (void)sessionID;
    printf("This is a tool for running elf, java or python apps in iTrustee.\n"
           "Usage:\n"
           "-s: install java runtime or python interpreter to iTrustee\n"
           "-m: install python third party lib to iTrustee\n"
           "-t: specify the installation type: python or java\n"
           "-c: create the main directory for the app in iTrustee\n"
           "-r: run elf file, python file, class file or jar file in iTrustee\n"
           "-i: specify the session id for the app\n"
           "-v: save the output file with a new name\n"
           "-p: set parameters for the application\n"
           "-z: optimization parameters include the memory size (-m)KB and \n"
           "    the number of concurrent threads (-j), separated by comma.\n"
           "    eg: -m512,-j12 .\n"
           "-e: destroy the directory of the app in iTrustee\n"
           "-u: uninstall java runtime or python interpreter to iTrustee\n"
           "-l: list third-party library installed in iTrustee\n"
           "-f: set running environment variable\n"
           "--getlog: get log file from the app\n"
           "--deletelog: delete log file from the app\n"
           "--memory: set memory size for iTrustee corresponds to ree container\n"
           "--disk-size: set tmpfs size for iTrustee corresponds to ree container\n"
           "--cpuset-cpus: set cpuset for iTrustee corresponds to ree container\n"
           "--cpus: set cpus usage limit for iTrustee corresponds to ree container\n"
           "--nsid: enter nsid and perform subsequent operations.\n"
           "--config-resource: specify nsid for iTrustee corresponds to ree container\n"
           "--config-container: writes information to a triplet on the tee side through nsid and containerid.\n"
           "--containerid: set container id for other actions\n"
           "--clean: clean entered nsid and containerid\n"
           "--vmid: set vmid for iTrustee corresponds to ree vm\n"
           "--grpid: pass grpid for iTrustee corresponds to ree container\n"
           "-h: print this help message\n");
    return 0;
}

static int32_t CheckPath(const char* filePath, char* trustPath)
{
    struct stat ss;
    if (realpath(filePath, trustPath) == NULL)
        return -EFAULT;

    if (lstat(trustPath, &ss) != 0) {
        printf("cannot read info of the file %s\n", trustPath);
        return -EFAULT;
    }

    if ((S_ISDIR(ss.st_mode) != true) && (S_ISREG(ss.st_mode) != true)) {
        printf("bad file status\n");
        return -EINVAL;
    }
    return access(trustPath, F_OK);
}

static enum TeeInstallUninstallType GetInstallUninstallType(const char *typename)
{
    if (strcmp(typename, "python") == 0)
        return PYTHON_INTERPRETER;
    if (strcmp(typename, "java") == 0)
        return JAVA_RUNTIME;
    if (strcmp(typename, "cfg") == 0)
        return CGROUP_CONFIGER;
    return INV_INSTALL_UNINSTALL_TYPE;
}

static int32_t DoInstall(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    int ret;
    char realPath[PATH_MAX] = { 0 };
    char dstPath[PATH_MAX] = { 0 };
    struct stat ss;
    enum TeeInstallUninstallType insType = GetInstallUninstallType(args->typeParam);
    if (!args->cmd[TP_TYPE] || insType == INV_INSTALL_UNINSTALL_TYPE) {
        printf("Please specify the installation type: python or java\n");
        return -EINVAL;
    }
    if (insType == JAVA_RUNTIME && args->cmd[TP_IMPORT]) {
        printf("only support python third party lib\n");
        return -EINVAL;
    }
    if (!args->cmd[TP_INSTALL] && args->cmd[TP_IMPORT])
        insType = PYTHON_THIRD_PARTY;
    const char *srcPath = (insType == PYTHON_THIRD_PARTY) ? args->importPath : args->installPath;
    if (CheckPath(srcPath, realPath) != 0) {
        printf("invalid file path\n");
        return -EINVAL;
    }

    if (lstat(realPath, &ss) != 0) {
        printf("cannot read file info\n");
        return -EFAULT;
    }
    if (S_ISREG(ss.st_mode) != true) {
        printf("please use .sec file\n");
        return -EINVAL;
    }

    ret = TeeScp(INPUT, realPath, dstPath, basename(realPath), 0);
    if (ret != 0) {
        printf("push %s failed\n", realPath);
        return -EFAULT;
    } else {
        printf("push %s successful\n", realPath);
    }

    ret = TeeInstall(insType, basename(realPath), &sessionID);
    if (ret != 0) {
        printf("Install %s failed\n", realPath);
        return -EFAULT;
    } else {
        printf("Install %s successful\n", realPath);
    }
    return 0;
}

static int32_t DoUninstall(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    (void)sessionID;
    if (!args->cmd[TP_TYPE] || GetInstallUninstallType(args->typeParam) == INV_INSTALL_UNINSTALL_TYPE) {
        printf("Please specify the uninstallation type: python or java\n");
        return -EINVAL;
    }
    int ret = TeeUninstall(GetInstallUninstallType(args->typeParam));
    if (ret != 0)
        printf("failed to uninstall %s\n", args->typeParam);
    else
        printf("successfully uninstall %s\n", args->typeParam);
    return ret;
}

static int32_t DoList(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    (void)sessionID;
    if (!args->cmd[TP_TYPE] || GetInstallUninstallType(args->typeParam) == INV_INSTALL_UNINSTALL_TYPE) {
        printf("Please specify the uninstallation type: python or java\n");
        return -EINVAL;
    }
    int ret = TeeList(GetInstallUninstallType(args->typeParam));
    if (ret != 0)
        printf("failed to list third-party library\n");

    return ret;
}

static int32_t DoDelete(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    (void)args;
    int ret = TeeDelete(sessionID);
    if (ret != 0) {
        printf("failed to delete log\n");
    } else {
        printf("successfully deleted log\n");
    }
    return ret;
}

static int32_t ParseStr(const char *str, int base, long *num, char **endPtr)
{
    errno = 0;
    long res = strtol(str, endPtr, base);
    if ((errno == ERANGE && (res == LONG_MAX || res == LONG_MIN))
            || (errno != 0 && res == 0) || *endPtr == str) {
        printf("failed to parse str %s\n", str);
        return -EINVAL;
    }

    *num = res;
    return 0;
}

static int32_t IsMemConfigValid(const char *memSize)
{
    if (memSize == NULL || strlen(memSize) == 0)
        return -EINVAL;
    
    char *endPtr = NULL;
    long num;
    int32_t ret = ParseStr(memSize, 10, &num, &endPtr);
    if (ret != 0) {
        printf("failed to parse mem config, mem size is %s\n", memSize);
        return ret;
    }

    if (endPtr == NULL) {
        printf("please add k/m/g after mem size!, memSize is %s\n", memSize);
        return -EINVAL;
    }

    if (*endPtr != 'K' && *endPtr != 'k' &&
        *endPtr != 'M' && *endPtr != 'm' &&
        *endPtr != 'G' && *endPtr != 'g') {
        printf("please add k/m/g after mem size!\n");
        return -EINVAL;
    }

    return 0;
}

/* containerMsg is a combination of containerId and namespace_id */
static int32_t CheckContainerId(const char *containerMsg)
{
    size_t idLen = strlen(containerMsg);
    /* check length */
    if (idLen != CONTAINER_ID_LEN) {
        printf("idLen = %d\n", (int)idLen);
        return -EINVAL;
    }

    for (size_t i = 0; i < CONTAINER_ID_LEN; i++) {
        if (!isxdigit(containerMsg[i])) {
            printf("not hex parameter\n");
            return -EINVAL;
        }
    }
    return 0;
}

static int32_t DoCleanContainerMsg(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    (void)sessionID;
    errno_t ret;
    struct TeePortalContainerType config = {0};

    if (CheckContainerId(args->containerMsg) != 0) {
        printf("bad container mesg!\n");
        return -EINVAL;
    }
    ret = memcpy_s(config.containerid, sizeof(config.containerid), args->containerMsg, CONTAINER_ID_LEN);
    if (ret != 0) {
        printf("memcpy_s failed, ret = %u", ret);
        return -EINVAL;
    }

    return TeeSendContainerMsg(&config, CONTAINER_STOP);
}

static int32_t DoCleanGroupId(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    (void)sessionID;
    int32_t ret = -1;
    printf("grp id is %s\n", args->grpId);

    long grpId = -1;
    char *endPtr = NULL;
    int base = 10;
    ret = ParseStr(args->grpId, base, &grpId, &endPtr);
    if (ret != 0 || *endPtr != '\0') {
        printf("failed to parse group id when clean!\n");
        return ret;
    }

    ret = TeeClean((int)grpId);
    if (ret != 0) {
        printf("failed to clean tee resource!\n");
    }

    return ret;
}

static int32_t DoClean(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    int32_t ret = 0;
    if (strlen(args->grpId) != 0) {
        ret = DoCleanGroupId(args, sessionID);
        if (ret != 0) {
            printf("failed to clean group id!\n");
            return ret;
        }
    }

    if (strlen(args->containerMsg) != 0) {
        ret = DoCleanContainerMsg(args, sessionID);
        if (ret != 0) {
            printf("failed to clean container msg!\n");
        }
    }
    return ret;
}

static int32_t DoRconfig(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    (void)sessionID;
    int32_t ret = -1;
    
    struct TeePortalRConfigType config = {0};
    char *endPtr = NULL;
    int base = 10;
    long nsid = 0;
    ret = ParseStr(args->nsId, base, &nsid, &endPtr);
    if (ret == -EINVAL || *endPtr != '\0') {
        printf("failed to parse nsid!, nsid is %ld\n", nsid);
        return ret;
    }

    endPtr = NULL;
    if (strcmp(args->cpus, "") != 0 && strcpy_s(config.cpus, PARAM_LEN, args->cpus) == 0) {
        printf("succeed to copy cpus config!\n");
    }

    if (IsMemConfigValid(args->mem) == 0 && strcpy_s(config.memSize, PARAM_LEN, args->mem) == 0) {
        printf("succeed to parse mem size config!\n");
    }

    if (IsMemConfigValid(args->diskSize) == 0 && strcpy_s(config.diskSize, PARAM_LEN, args->diskSize) == 0) {
        printf("succeed to parse disk size config!\n");
    }

    if (strcmp(args->cpuset, "") != 0 && strcpy_s(config.cpuset, PARAM_LEN, args->cpuset) == 0) {
        printf("succeed to parse cpuset config!\n");
    }
    
    ret = TeeRconfig(&config, nsid);
    printf("TeeRconfig ret is %d\n", ret);
    return ret;
}

#define SESSION_ID_SCANF_NUM 1
static int32_t ReadSessionID(const char *filePath, uint32_t *sessionID)
{
    char realPath[PATH_MAX] = { 0 };
    if (CheckPath(filePath, realPath) != 0)
        return -EINVAL;

    FILE *fp = fopen(realPath, "r");
    if (fp == NULL)
        return -EACCES;

    if (fscanf_s(fp, "%u", sessionID) != SESSION_ID_SCANF_NUM) {
        (void)fclose(fp);
        return -EFAULT;
    }
    (void)fclose(fp);
    return 0;
}

static int32_t WriteSessionID(const char *filePath, uint32_t sessionID)
{
    FILE *fp = fopen(filePath, "w");
    if (fp == NULL)
        return -EACCES;

    if (fprintf(fp, "%05u", sessionID) < 0) {
        printf("cannot write session id to file\n");
        (void)fclose(fp);
        return -EACCES;
    }

    (void)fclose(fp);
    return 0;
}

enum RunFileType {
    RUN_FILE_NOT_SUPPORTED,
    JAVA_FILE_CLASS,
    JAVA_FILE_JAR,
    PYTHON_FILE_PY_PYC,
    RAW_ELF_FILE,
};

static enum RunFileType GetRunFileType(const char *filename, char *target)
{
    char *ret = strrchr(filename, '.');
    if (ret == NULL)
        goto elf_type;

    if (strcmp(ret, ".class") == 0) {
        if (strncpy_s(target, PATH_MAX - 1, filename, (unsigned long)(ret - filename)) != EOK) {
            printf("failed to copy string!\n");
            return RUN_FILE_NOT_SUPPORTED;
        }
        return JAVA_FILE_CLASS;
    }

    if (strcmp(ret, ".jar") == 0) {
        if (strncpy_s(target, PATH_MAX - 1, filename, strlen(filename)) != EOK) {
            printf("failed to copy string!\n");
            return RUN_FILE_NOT_SUPPORTED;
        }
        return JAVA_FILE_JAR;
    }

    if (strcmp(ret, ".py") == 0 || strcmp(ret, ".pyc") == 0) {
        if (strncpy_s(target, PATH_MAX - 1, filename, strlen(filename)) != EOK) {
            printf("failed to copy string!\n");
            return RUN_FILE_NOT_SUPPORTED;
        }
        return PYTHON_FILE_PY_PYC;
    }
elf_type:
    if (strncpy_s(target, PATH_MAX - 1, filename, strlen(filename)) != EOK) {
        printf("failed to copy elf name!\n");
        return RUN_FILE_NOT_SUPPORTED;
    }
    return RAW_ELF_FILE;
}

#define GAP_TO_CLASS_PATH 2
static int32_t ParseParam(char *param, int32_t *num, char **result, int32_t maxNum)
{
    if (param == NULL)
        return 0;
    int32_t start = 0;
    int32_t tmplen;
    int32_t end = (int)(unsigned int)strlen(param);
    char *flag;
    /* parse classpath */
    bool cpFlag = false;

    *num = 0;
    while (start < end) {
        tmplen = 0;
        flag = param + start;
        // skip space
        while (isspace(*flag) != 0) {
            flag++;
            start++;
        }
        if (*flag != '\0') {
            if (*num >= maxNum)
                return -EFAULT;
            result[*num] = flag;
            (*num)++;
        }
        // find end
        while (isspace(flag[tmplen]) == 0 && flag[tmplen] != '\0')
            tmplen++;

        flag[tmplen] = '\0';

        /* judge current param is equal to "-classpath" */
        if (*num > 0 && (strcmp(result[(*num) - 1], "-classpath") == 0 || strcmp(result[(*num) - 1], "-cp") == 0)) {
            cpFlag = true;
            /* "-classpath" or "-cp" should not be stored as a normal param */
            (*num)--;
        }
        /* save the content of the classpath */
        if (cpFlag && *num > 0) {
            /* result - 2 == argv[RUN_PARAM_INDEX_ONE] == classpath */
            if (strcpy_s(*(result - GAP_TO_CLASS_PATH), PATH_MAX, result[(*num) - 1]) != EOK) {
                printf("failed to copy classpath to argv\n");
                return -EFAULT;
            }
            /* classpath content should not be stored as a normal param */
            (*num)--;
            cpFlag = false;
        }
        tmplen++;
        start += tmplen;
    }
    return 0;
}

#define RUN_PARAM_INDEX_ZERO  0
#define RUN_PARAM_INDEX_ONE   1
#define RUN_PARAM_INDEX_TWO   2
#define RUN_PARAM_INDEX_THREE 3
#define RUN_PARAM_INDEX_FOUR  4
static int32_t TeeRunJavaClass(const char *target, const struct TeeRunParam *runParam, char *param, int *retVal)
{
    int32_t paramNum = 0;
    char *argv[PARAM_NUM_MAX];
    char cmd[PATH_MAX] = { 0 };
    char className[PATH_MAX] = { 0 };
    char classPath[PATH_MAX] = { 0 };
    char tmp[PATH_MAX] = { 0 };
    (void)strcpy_s(cmd, PATH_MAX, "-cp");
    if (strcpy_s(tmp, PATH_MAX, target) != EOK || strcpy_s(className, PATH_MAX, basename(tmp)) != EOK ||
        strcpy_s(classPath, PATH_MAX, dirname(tmp)) != EOK) {
        printf("run java: copy class path error\n");
        return -EFAULT;
    }

    argv[RUN_PARAM_INDEX_ZERO] = cmd;
    argv[RUN_PARAM_INDEX_ONE]  = classPath;
    argv[RUN_PARAM_INDEX_TWO]  = className;
    if (ParseParam(param, &paramNum, argv + RUN_PARAM_INDEX_THREE, PARAM_NUM_MAX - RUN_PARAM_INDEX_FOUR) != 0) {
        printf("parse param error\n");
        return -EFAULT;
    }
    argv[paramNum + RUN_PARAM_INDEX_THREE] = NULL;
    return TeeRun(RUN_JAVA, paramNum + RUN_PARAM_INDEX_FOUR, argv, runParam, retVal);
}

static int32_t TeeRunJavaJar(const char *target, const struct TeeRunParam *runParam, char *param, int *retVal)
{
    int32_t paramNum = 0;
    char *argv[PARAM_NUM_MAX];
    char cmd[PATH_MAX] = { 0 };
    char tarpath[PATH_MAX] = { 0 };
    (void)strcpy_s(cmd, PATH_MAX, "-jar");
    if (memcpy_s(tarpath, PATH_MAX, target, PATH_MAX) != 0) {
        printf("failed to copy path in jar!\n");
        return -EFAULT;
    }

    argv[RUN_PARAM_INDEX_ZERO] = cmd;
    argv[RUN_PARAM_INDEX_ONE] = tarpath;
    if (ParseParam(param, &paramNum, argv + RUN_PARAM_INDEX_TWO, PARAM_NUM_MAX - RUN_PARAM_INDEX_THREE) != 0) {
        printf("parse param error\n");
        return -EFAULT;
    }
    argv[paramNum + RUN_PARAM_INDEX_TWO] = NULL;
    return TeeRun(RUN_JAVA, paramNum + RUN_PARAM_INDEX_THREE, argv, runParam, retVal);
}

static int32_t TeeRunElf(const char *target, const struct TeeRunParam *runParam, char *param, int *retVal)
{
    int32_t paramNum = 0;
    char *argv[PARAM_NUM_MAX];
    char tarpath[PATH_MAX] = { 0 };
    if (memcpy_s(tarpath, PATH_MAX, target, PATH_MAX) != 0) {
        printf("failed to copy path in elf!\n");
        return -EFAULT;
    }

    argv[RUN_PARAM_INDEX_ZERO] = tarpath;
    if (ParseParam(param, &paramNum, argv + RUN_PARAM_INDEX_ONE, PARAM_NUM_MAX - RUN_PARAM_INDEX_TWO) != 0) {
        printf("parse elf param error\n");
        return -EFAULT;
    }

    argv[paramNum + RUN_PARAM_INDEX_ONE] = NULL;
    return TeeRun(RUN_ELF, paramNum + RUN_PARAM_INDEX_TWO, argv, runParam, retVal);
}

static int32_t TeeRunPython(const char *target, const struct TeeRunParam *runParam, char *param, int *retVal)
{
    int32_t paramNum = 0;
    char *argv[PARAM_NUM_MAX];
    char tarpath[PATH_MAX] = { 0 };
    if (memcpy_s(tarpath, PATH_MAX, target, PATH_MAX) != 0) {
        printf("failed to copy path in python!\n");
        return -EFAULT;
    }

    argv[RUN_PARAM_INDEX_ZERO] = tarpath;
    if (ParseParam(param, &paramNum, argv + RUN_PARAM_INDEX_ONE, PARAM_NUM_MAX - RUN_PARAM_INDEX_TWO) != 0) {
        printf("parse param error\n");
        return -EFAULT;
    }
    argv[paramNum + RUN_PARAM_INDEX_ONE] = NULL;
    return TeeRun(RUN_PYTHON, paramNum + RUN_PARAM_INDEX_TWO, argv, runParam, retVal);
}

static int IsLegalParam(const char *paramVal)
{
    char filter[] = "|;&$<>`\\!\n";
    for (size_t i = 0; i < strlen(paramVal); i++) {
        for (size_t j = 0; j < strlen(filter); j++) {
            if (paramVal[i] == filter[j])
                return -EINVAL;
        }
    }
    return 0;
}

#define MIN_ENV_PARAM_LEN strlen("sep=:;x")
#define ENV_PARAM_SEP_LEN strlen("sep=")
static int IsLegalEnvParam(const char *paramVal)
{
    size_t paramLen = strlen(paramVal);
    if (paramLen < MIN_ENV_PARAM_LEN || paramLen > PORTAL_RUN_ARGS_MAXSIZE) {
        printf("invalid env param length.\n");
        return -EINVAL;
    }
    if (strncmp(paramVal, "sep=", ENV_PARAM_SEP_LEN) != 0) {
        printf("invalid env param prefix.\n");
        return -EINVAL;
    }
    return 0;
}

static int32_t CheckTeePath(const char *path)
{
    size_t pathLen = strlen(path);
    /* check space */
    for (size_t i = 0; i < pathLen; i++) {
        if (isspace(path[i]))
            return -EINVAL;
    }
    /* check '/' */
    if (path[0] == '/')
        return -EINVAL;

    /* check '..' */
    if (pathLen > 1) {
        if (path[0] == '.' && path[1] == '.')
            return -EINVAL;
    }
    return 0;
}

static int32_t CheckIDSavePath(const char *idPath)
{
    char idDirPath[PATH_MAX] = { 0 };
    char realPath[PATH_MAX] = { 0 };
    char fullPath[PATH_MAX] = { 0 };
    size_t pathLen = strlen(idPath);
    /* check space */
    for (size_t i = 0; i < pathLen; i++) {
        if (isspace(idPath[i]))
            return -EINVAL;
	}
    if (pathLen == 0 || idPath[pathLen - 1] == '/')    /* check last char */
        return -EINVAL;
    if (realpath(idPath, realPath) != NULL) { /* if file exists, returns error */
        printf("file %s exists, please clean\n", idPath);
        return -EINVAL;
    }
    if (errno != ENOENT) { /* returns error except the file does not exist */
        printf("bad file status %s\n", idPath);
        return -EINVAL;
    }
    (void)memcpy_s(idDirPath, PATH_MAX, idPath, PATH_MAX);

    char *pdir = dirname(idDirPath);
    if (strcmp(pdir, ".") != 0) {
        if (pdir[0] != '/') {
            if (getcwd(fullPath, PATH_MAX) == NULL) {
                printf("cannot get current dir\n");
                return -EFAULT;
            }

            if (snprintf_s(fullPath, PATH_MAX, PATH_MAX - 1, "%s/%s", fullPath, pdir) < 0) {
                printf("generate file path failed\n");
                return -EFAULT;
            }
        } else {
            if (strcpy_s(fullPath, PATH_MAX, pdir) != EOK) {
                printf("copy file path failed\n");
                return -EFAULT;
            }
        }

        if (MkdirIteration(fullPath) != 0) {
            printf("mkdir failed\n");
            return -EFAULT;
        }
    }
    return 0;
}

static int32_t DoOutput(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    char filename[PATH_MAX] = { 0 };
    char dstPath[PATH_MAX] = { 0 };
    const char *reePath;

    if (args->cmd[TP_SAVE]) {
        reePath = args->savePath;
    } else {
        reePath = LOG_NAME;
    }
    int ret = TeeScp(OUTPUT, reePath, dstPath, filename, sessionID);
    if (ret != 0)
        printf("failed to pull log\n");
    else
        printf("successfully pulled log\n");

    return ret;
}

#define NUMBER_BASE10 10
#define PARAM_PRE_OFFSET 2
static void DoOptimization(const struct TeeTeleportArgs *args)
{
    /* Optimization parameters include the memory size (-m)KB and the number of concurrent threads (-j),
       separated by comma. eg: -m512,-j12
     */
    char optimization[PARAM_LEN_MAX] = { 0 };
    char *context = NULL;
    char *token = NULL;
    const char *sep = ",";
    char *stop = NULL;

    long m = 0;
    long j = 0;

    if (strcpy_s(optimization, PARAM_LEN_MAX, args->optimization) != EOK) {
        printf("copy optimization params failed\n");
        return;
    }

    printf("args->optimization %s\n", optimization);

    token = strtok_s(optimization, sep, &context);
    while (token != NULL) {
        if (token[0] == '-' && token[1] != '\0') {
            switch (token[1]) {
                case 'm':
                    m = strtol(token + PARAM_PRE_OFFSET, &stop, NUMBER_BASE10);
                    break;
                case 'j':
                    j = strtol(token + PARAM_PRE_OFFSET, &stop, NUMBER_BASE10);
                    break;
                default:
                    printf("invalid option1: %s\n", token);
                    break;
            }
        }
        token = strtok_s(NULL, sep, &context);
    }

    printf("optimization -m %ld, -j %ld \n", m, j);

#ifdef CROSS_DOMAIN_PERF
    SetDataTaskletThreadConcurrency(j);
    SetDataTaskletBufferSize(m);
#endif
}

#define SIGNALED_RET (-1)
#define STOPPED_RET (-2)
static int ExcuteTeeRun(const struct TeeTeleportArgs *args, struct TeeRunParam *runParam, char *paramVal)
{
    int ret;
    int retVal;
    char target[PATH_MAX] = { 0 };
    if (args == NULL || runParam == NULL || runParam->sessionID == 0) {
        printf("Bad params\n");
        return -EINVAL;
    }

    switch (GetRunFileType(args->runPath, target)) {
        case JAVA_FILE_CLASS:
            ret = TeeRunJavaClass(target, runParam, paramVal, &retVal);
            break;
        case JAVA_FILE_JAR:
            ret = TeeRunJavaJar(target, runParam, paramVal, &retVal);
            break;
        case PYTHON_FILE_PY_PYC:
            ret = TeeRunPython(target, runParam, paramVal, &retVal);
            break;
        case RAW_ELF_FILE:
            ret = TeeRunElf(target, runParam, paramVal, &retVal);
            break;
        default:
            printf("bad file type!\n");
            ret = -EINVAL;
            break;
    }

    if (ret != 0) {
        printf("cannot run file\n");
        return -EFAULT;
    }

    if (retVal == SIGNALED_RET)
        printf("program terminated by signal, may crash\n");
    else if (retVal == STOPPED_RET)
        printf("program stopped by signal\n");
    else if (retVal == 0)
        printf("successfully run program returns 0\n");
    else
        printf("program error returns %d\n", retVal);

    return ret;
}

static int32_t DoRun(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    char *paramVal = NULL;
    char oriParam[PARAM_LEN_MAX] = { 0 };
    int ret;

    if (args->cmd[TP_OPTIMIZATION]) {
        DoOptimization(args);
    }

    if (args->cmd[TP_PARAM]) {
        if (IsLegalParam(args->paramVal) != 0) {
            printf("bad param detected\n");
            return -EINVAL;
        }
        (void)memcpy_s(oriParam, PARAM_LEN_MAX, args->paramVal, PARAM_LEN_MAX);
        paramVal = oriParam;
    }

    /* memcpy envParam from TeeTeleportArgs */
    char *envParam = NULL;
    char oriEnvParam[PARAM_LEN_MAX] = { 0 };
    if (args->cmd[TP_ENV]) {
        /* legal check */
        if (IsLegalEnvParam(args->envParam) != 0) {
            printf("bad env param detected\n");
            return -EINVAL;
        }
        (void)memcpy_s(oriEnvParam, PARAM_LEN_MAX, args->envParam, PARAM_LEN_MAX);
        envParam = oriEnvParam;
    }

    if (CheckTeePath(args->runPath) != 0) {
        printf("bad run path\n");
        return -EINVAL;
    }

    struct TeeRunParam runParam = {sessionID, envParam};
    ret = ExcuteTeeRun(args, &runParam, paramVal);
    if (ret != 0) {
        return ret;
    }

    if (args->cmd[TP_OUTPUT]) {
        if (DoOutput(args, sessionID) != 0) {
            printf("cannot output files");
            return -EFAULT;
        }
    }
    return 0;
}

static int32_t DoDestroy(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    int ret = TeeDestroy(sessionID);
    if (ret != 0) {
        printf("failed to destroy session %u\n", sessionID);
        return ret;
    }
    printf("successfully destroyed session %u\n", sessionID);
    return unlink(args->idPath);
}

static int32_t GetPathForCreate(const struct TeeTeleportArgs *args, char *realPath, char *filename, char *idPath)
{
    struct stat ss;
    bool fastRun = args->cmd[TP_RUN] && args->cmd[TP_DESTROY];
    if (CheckPath(args->createPath, realPath) != 0) {
        printf("invalid file path!\n");
        return -EINVAL;
    }

    if (lstat(realPath, &ss) != 0) {
        printf("cannot read file info\n");
        return -EFAULT;
    }
    if (S_ISREG(ss.st_mode)) {
        if (strcpy_s(filename, PATH_MAX, basename(realPath)) != EOK) {
            printf("get path: cannot copy filename\n");
            return -EFAULT;
        }
    } else {
        printf("bad file status, please use .sec file\n");
        return -EINVAL;
    }

    if (args->cmd[TP_SAVE])
        (void)memcpy_s(idPath, PATH_MAX, args->savePath, PATH_MAX);
    else
        (void)strcpy_s(idPath, PATH_MAX, "sessionID.txt");

    if (fastRun != true && CheckIDSavePath(idPath) != 0) {
        printf("invalid session id path\n");
        return -EINVAL;
    }
    return 0;
}

static int32_t DoCreate(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    int ret;
    char realPath[PATH_MAX] = { 0 };
    char dstPath[PATH_MAX] = { 0 };
    char filename[PATH_MAX] = { 0 };
    char idPath[PATH_MAX] = { 0 };
    bool fastRun = args->cmd[TP_RUN] && args->cmd[TP_DESTROY];  /* TP_CREATE is true */

    if (GetPathForCreate(args, realPath, filename, idPath) != 0)
        return -EINVAL;

    ret = TeeScp(INPUT, realPath, dstPath, filename, 0);
    if (ret != 0) {
        printf("push %s failed\n", realPath);
        return -EFAULT;
    } else {
        printf("push %s successful\n", realPath);
    }

    ret = TeeInstall(APPLICATION, basename(realPath), &sessionID);
    if (ret != 0) {
        printf("failed to install %s\n", realPath);
        return -EFAULT;
    } else {
        printf("successfully installed %s\n", realPath);
    }

    /* if fastRun is true, we do not write session id to file */
    if (fastRun) {
        ret = DoRun(args, sessionID);
        if (ret != 0)
            printf("cannot run\n");

        if (TeeDestroy(sessionID) != 0) {
            printf("cannot destroy session %u\n", sessionID);
            return -EFAULT;
        } else {
            printf("successfully destroyed session %u\n", sessionID);
        }
    } else if (WriteSessionID(idPath, sessionID) != 0) {
        printf("cannot write session ID to file\n");
        ret = -EFAULT;
        if (TeeDestroy(sessionID) != 0) {
            printf("cannot destroy session %u\n", sessionID);
            return -EFAULT;
        } else {
            printf("successfully destroyed session %u\n", sessionID);
        }
    }
    return ret;
}

static int32_t CheckParams(const struct TeeTeleportArgs *args, struct TeePortalContainerType *config)
{
    int32_t ret;
    char *endPtr = NULL;
    int base = 10; // 10 is the normal length of nsid
    long ns_id = 0;

    ret = ParseStr(args->nsId, base, &ns_id, &endPtr);
    if (ret == -EINVAL || *endPtr != '\0') {
        printf("failed to parse nsid! nsid is %d\n", config->nsid);
        return ret;
    }

    config->nsid = (uint32_t)ns_id;
    if (CheckContainerId(args->containerMsg) != 0) {
        printf("bad container mesg!\n");
        return -EINVAL;
    }

    ret = memcpy_s(config->containerid, sizeof(config->containerid), args->containerMsg, CONTAINER_ID_LEN);
    if (ret != 0) {
        printf("memcpy_s failed, ret = %u", ret);
        return -EINVAL;
    }

    return 0;
}

static int32_t DoContainer(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
    (void)sessionID;
    struct TeePortalContainerType config = { 0 };
    if (CheckParams(args, &config) != 0) {
        printf("bad container mesg!\n");
        return -EINVAL;
    }

    return TeeSendContainerMsg(&config, CONTAINER_OPEN);
}

static const struct TeeTeleportFunc g_teleportFuncTable[] = {
    {TP_HELP,       PrintUsage,  false},
    {TP_INSTALL,    DoInstall,   false},
    {TP_UNINSTALL,  DoUninstall, false},
    {TP_IMPORT,     DoInstall,   false},
    {TP_CREATE,     DoCreate,    false},
    {TP_LIST,       DoList,      false},
    {TP_CONF_CONT,  DoContainer, false},
    {TP_RUN,        DoRun,       true},
    {TP_OUTPUT,     DoOutput,    true},
    {TP_DELETE,     DoDelete,    true},
    {TP_DESTROY,    DoDestroy,   true},
    {TP_CLEAN,      DoClean,     false},
    {TP_CONF_RES,   DoRconfig,   false},
};

static const uint32_t g_teleportFuncNum = sizeof(g_teleportFuncTable) / sizeof(g_teleportFuncTable[0]);

static int32_t HandleUserCmd(struct TeeTeleportArgs *args)
{
    uint32_t sessionID = 0;
    if (args->cmd[TP_ID]) {
        if (ReadSessionID(args->idPath, &sessionID) != 0) {
            printf("read session id failed\n");
            return -EINVAL;
        }
    }

    for (uint32_t i = 0; i < g_teleportFuncNum; i++) {
        if (args->cmd[g_teleportFuncTable[i].type]) {
            if (g_teleportFuncTable[i].needId && args->cmd[TP_ID] == false) {
                printf("please specify session ID\n");
                return -EINVAL;
            }
            return g_teleportFuncTable[i].func(args, sessionID);
        }
    }
    printf("cannot handle user cmd\n");
    return -EINVAL;
}

static int32_t CopyArgs(bool *argkey, char *argvalue, uint32_t size)
{
    *argkey = true;
    if (argvalue == NULL)
        return 0;

    size_t optLen = strnlen(optarg, PATH_MAX);
    if (optLen == 0 || optLen >= PATH_MAX) {
        printf("opt arg is invalid\n");
        return -EINVAL;
	}

    if (strcpy_s(argvalue, size - 1, optarg) != EOK) {
        printf("strcpy_s failed\n");
        return -EFAULT;
    }
    return 0;
}

#define CASE_COPY_ARG(opt, argkey, argvalue, size) \
    case opt: \
        if (CopyArgs(argkey, argvalue, size) != 0) \
            return -EFAULT; \
        break;

static int32_t ParseArgs(int32_t argc, char **argv, struct TeeTeleportArgs *args)
{
    while (1) {
        int32_t optIndex = 0;
        int32_t opt = getopt_long(argc, argv, "s:m:t:c:r:i:v:p:z:f:k:w:heul", g_toolOptions, &optIndex);
        if (opt < 0)
            break;
        switch (opt) {
        CASE_COPY_ARG('s',             &args->cmd[TP_INSTALL],   args->installPath,     PATH_MAX);
        CASE_COPY_ARG('m',             &args->cmd[TP_IMPORT],    args->importPath,      PATH_MAX);
        CASE_COPY_ARG('t',             &args->cmd[TP_TYPE],      args->typeParam,       PARAM_LEN);
        CASE_COPY_ARG('c',             &args->cmd[TP_CREATE],    args->createPath,      PATH_MAX);
        CASE_COPY_ARG('r',             &args->cmd[TP_RUN],       args->runPath,         PATH_MAX);
        CASE_COPY_ARG('i',             &args->cmd[TP_ID],        args->idPath,          PATH_MAX);
        CASE_COPY_ARG('v',             &args->cmd[TP_SAVE],      args->savePath,        PATH_MAX);
        CASE_COPY_ARG('p',             &args->cmd[TP_PARAM],     args->paramVal,        PARAM_LEN_MAX);
        CASE_COPY_ARG('z',             &args->cmd[TP_OPTIMIZATION], args->optimization, PARAM_LEN_MAX);
        CASE_COPY_ARG('f',             &args->cmd[TP_ENV],       args->envParam,    PARAM_LEN_MAX);
        CASE_COPY_ARG('e',             &args->cmd[TP_DESTROY],   NULL,                  0);
        CASE_COPY_ARG('u',             &args->cmd[TP_UNINSTALL], NULL,                  0);
        CASE_COPY_ARG('l',             &args->cmd[TP_LIST],      NULL,                  0);
        CASE_COPY_ARG('h',             &args->cmd[TP_HELP],      NULL,                  0);
        CASE_COPY_ARG(ARG_GET_LOG,     &args->cmd[TP_OUTPUT],    NULL,                  0);
        CASE_COPY_ARG(ARG_DEL_LOG,     &args->cmd[TP_DELETE],    NULL,                  0);
        CASE_COPY_ARG(ARG_CLEAN,       &args->cmd[TP_CLEAN],     NULL,                  0);
        CASE_COPY_ARG(ARG_GRPID,       &args->cmd[TP_GRPID],     args->grpId,           PARAM_LEN);
        CASE_COPY_ARG(ARG_NSID_SET,    &args->cmd[TP_NSID_SET],  args->nsId,            PARAM_LEN);
        CASE_COPY_ARG(ARG_CONF_RES,    &args->cmd[TP_CONF_RES],  NULL,                  0);
        CASE_COPY_ARG(ARG_CONTAINER,   &args->cmd[TP_CONTAINER], args->containerMsg,    PARAM_LEN_MAX);
        CASE_COPY_ARG(ARG_CONF_CONT,   &args->cmd[TP_CONF_CONT], NULL,                  PARAM_LEN_MAX);
        CASE_COPY_ARG(ARG_MEMORY,      &args->cmd[TP_MEM],       args->mem,             PARAM_LEN);
        CASE_COPY_ARG(ARG_CPUSET,      &args->cmd[TP_CPUSET],    args->cpuset,          PARAM_LEN_MAX);
        CASE_COPY_ARG(ARG_CPUS,        &args->cmd[TP_CPUS],      args->cpus,            PARAM_LEN);
        CASE_COPY_ARG(ARG_DISK_SIZE,   &args->cmd[TP_DISK_SIZE], args->diskSize,        PARAM_LEN);
        default:
            printf("please use -h to see the usage.\n");
            return -EINVAL;
        }
    }
    return 0;
}

#define MIN_ARG_COUNT 2
int32_t main(int32_t argc, char *argv[])
{
    struct TeeTeleportArgs args = { 0 };
    int32_t ret = 0;

    if (argc < MIN_ARG_COUNT) {
        printf("please use \"tee_teleport -h\" for more help!\n");
        return ret;
    }

    if (ParseArgs(argc, argv, &args) != 0)
        return -EINVAL;

    if (InitPortal() != 0) {
        printf("cannot init portal\n");
        return -EFAULT;
    }

    ret = HandleUserCmd(&args);
    DestroyPortal();
    return ret;
}

