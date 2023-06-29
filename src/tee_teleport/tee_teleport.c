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

#include "dir.h"

static const struct option g_toolOptions[] = {{"install",   required_argument, NULL, 's'},
                                              {"import",    required_argument, NULL, 'm'},
                                              {"type",      required_argument, NULL, 't'},
                                              {"create",    required_argument, NULL, 'c'},
                                              {"run",       required_argument, NULL, 'r'},
                                              {"id",        required_argument, NULL, 'i'},
                                              {"input",     required_argument, NULL, 'n'},
                                              {"output",    required_argument, NULL, 'o'},
											  {"rename",    required_argument, NULL, 'a'},
											  {"save",      required_argument, NULL, 'v'},
											  {"parameter", required_argument, NULL, 'p'},
											  {"delete",    required_argument, NULL, 'd'},
											  {"query",     required_argument, NULL, 'q'},
											  {"destroy",   required_argument, NULL, 'e'},
											  {"uninstall", required_argument, NULL, 'u'},
											  {"list",      required_argument, NULL, 'l'},
											  {"help",      required_argument, NULL, 'h'},
											  {NULL, 0, NULL, 0}};

static int32_t PrintUsage(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
	(void)args;
	(void)sessionID;
	printf("This is a tool for running java or python apps in iTrustee.\n"
           "Usage:\n"
		   "-s: install java runtime or python interpreter to iTrustee\n"
		   "-m: install python third party lib to iTrustee\n"
		   "-t: specify the installation type: python or java\n"
		   "-c: create the main directory for the app in iTrustee\n"
		   "-r: run python file, class file or jar file in iTrustee\n"
		   "-i: specify the session id for the app\n"
		   "-n: input files for the app\n"
		   "-a: rename the input file for the app\n"
		   "-o: get output file from the app\n"
		   "-v: save the output file with a new name\n"
		   "-p: set parameters for the application\n"
		   "-d: delete a file or dir in the app path\n"
		   "-q: query a file or dir in the app path\n"
		   "-e: destroy the directory of the app in iTrustee\n"
		   "-u: uninstall java runtime or python interpreter to iTrustee\n"
		   "-l: list third-party library installed in iTrustee\n"
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
	if (strcmp(typename, "python_third") == 0)
		return PYTHON_THIRD_PARTY;
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
		return -EFAULT
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

	if (fprintf(fp, "%07u", sessionID) < 0) {
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
};

static enum RunFileType GetRunFileType(const char *filename, char *target)
{
	char *ret = strrchr(filename, '.');
	if (ret == NULL)
		return RUN_FILE_NOT_SUPPORTED;

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
	return RUN_FILE_NOT_SUPPORTED;
}

static int32_t ParseParam(char *param, int32_t *num, char **result, int32_t maxNum)
{
	if (param == NULL)
		return 0;
	int32_t start = 0;
	int32_t tmplen;
	int32_t end = (int)(unsigned int)strlen(param);
	char *flag;

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
		tmplen++;
		start += tmplen;
	}
	return 0;
}

#define RUN_PARAM_INDEX_ZERO  0
#define RUN_PARAM_INDEX_ONE   1
#define RUN_PARAM_INDEX_TWO   2
#define RUN_PARAM_INDEX_THREE 3
#define RUN_PARAM INDEX_FOUR  4
static int32_t TeeRunJavaClass(const char *target, uint32_t sessionID, char *param, int *retVal)
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
	return TeeRun(RUN_JAVA, paramNum + RUN_PARAM_INDEX_FOUR, argv, sessionID, retVal);
}

static int32_t TeeRunJavaJar(const char *target, uint32_t sessionID, char *param, int *retVal)
{
	int32_t paramNum = 0;
	char *argv[PARAM_NUM_MAX];
	char cmd[PATH_MAX] = { 0 };
	char tarpath[PATH_MAX] = { 0 };
	(void)strcpy_s(cmd, PATH_MAX, "-jar");
    if (memcpy_s(target, PATH_MAX, target, PATH_MAX) != 0) {
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
	return TeeRun(RUN_JAVA, paramNum + RUN_PARAM_INDEX_THREE, argv, sessionID, retVal);
}

static int32_t TeeRunPython(const char *target, uint32_t sessionID, char *param, int *retVal)
{
	int32_t paramNum = 0;
	char *argv[PARAM_NUM_MAX];
	char tarpath[PATH_MAX] = { 0 };
	if (memcpy_s(tarpath, PATH_MAX, target, PATH_MAX) != 0) {
		printf("failed to copy path in jar!\n");
		return -EFAULT;
	}

	argv[RUN_PARAM_INDEX_ZERO] = tarpath;
	if (ParseParam(param, &paramNum, argv + RUN_PARAM_INDEX_ONE, PARAM_NUM_MAX - RUN_PARAM_INDEX_TWO) != 0) {
		printf("parse param error\n");
		return -EFAULT;
	}
	argv[paramNum + RUN_PARAM_INDEX_ONE] = NULL;
	return TeeRun(RUN_PYTHON, paramNum + RUN_PARAM_INDEX_TWO, argv, sessionID, retVal);
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

static int32_t RenameInputFiles(const struct TeeTeleportArgs *args, char *filename, char *dstPath, struct stat ss)
{
	char renameDir[PATH_MAX] = { 0 };
	char oriPath[PATH_MAX] = { 0 };
	errno_t rc;
	if (!args->cmd[TP_RENAME]) {
		return 0;
	}
	(void)memcpy_s(oriPath, PATH_MAX, args->renamePath, PATH_MAX);
	if (CheckTeePath(args->renamePath) != 0) {
		printf("Invalid rename param!\n");
		return -EINVAL;
	}
	rc = strcpy_s(filename, PATH_MAX, basename(oriPath));
	if (rc != EOK) {
		printf("rename input file: copy filename failed %d\n", rc);
		return -EFAULT;
	}
	char *dirp = strdup(args->renamePath);
	if (dirp == NULL) {
		printf("strdup failed\n");
		return -EFAULT;
	}
	if (S_ISDIR(ss.st_mode))
		rc = strcpy_s(renameDir, PATH_MAX, args->renamePath);
	else if (strcmp(dirname(dirp), ".") == 0)
		renameDir[0] = 0; /* rc is EOK in this branch */
	else
		rc = strcpy_s(renameDir, PATH_MAX, dirname(oriPath));
	if (rc != EOK) {
		printf("rename input file: copy rename dir failed %d\n", rc);
		free(dirp);
		return -EFAULT;
	}

	free(dirp);

	if (renameDir[0] == 0) {
		(void)memset_s(dstPath, PATH_MAX, 0, PATH_MAX);
	} else {
		if (memcpy_s(dstPath, PATH_MAX, renameDir, PATH_MAX) < 0) {
			printf("memcpy_s failed\n");
			return -EFAULT;
		}
	}
	return 0;
}

static int32_t DoInput(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
	int ret;
	struct stat ss;
	char realPath[PATH_MAX] = { 0 };
	char dstPath[PATH_MAX] = { 0 };
	char filename[PATH_MAX] = { 0 };

	if (CheckPath(args->inputPath, realPath) != 0) {
		printf("invalid file or directory!\n");
		return -EINVAL;
	}

	if (lstat(realPath, &ss) != 0) {
		printf("cannot read file info\n");
		return -EFAULT;
	}
	if (S_ISDIR(ss.st_mode)) {
		if (memcpy_s(dstPath, PATH_MAX, basename(realPath), strlen(basename(realPath))) != EOK) {
			printf("memcpy_s failed\n");
			return -EFAULT;
		}
	}

	errno_t rc = strcpy_s(filename, PATH_MAX, basename(realPath));
	if (rc != EOK) {
		printf("do input: cannot copy filename %d\n", rc);
		return -EFAULT;
	}

	if (RenameInputFiles(args, filename, dstPath, ss) != 0) {
		printf("cannot rename files\n");
		return -EFAULT;
	}
	ret = TeeScp(INPUT, realPath, dstPath, filename, sessionID);
	if (ret != 0)
		printf("failed to push %s\n", realPath);
	else
		printf("successfully pushed %s\n", realPath);
	return ret;
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
	const char *reePath;
	if (CheckTeePath(args->outputPath) != 0) {
		printf("bad output path\n");
		return -EINVAL;
	}

	if (args->cmd[TP_SAVE]) {
		reePath = args->savePath;
	} else {
		if (strcpy_s(filename, PATH_MAX, args->outputPath) != EOK) {
			printf("strcpy_s failed\n");
			return -EFAULT;
		}
		reePath = basename(filename);
	}
	int ret = TeeScp(OUTPUT, reePath, args->outputPath, basename(filename), sessionID);
	if (ret != 0)
		printf("failed to pull %s\n", args->outputPath);
	else
		printf("successfully pulled %s\n", args->outputPath);

	return ret;
}

static int32_t DoRun(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
	char *paramVal = NULL;
	char oriParam[PARAM_LEN_MAX] = { 0 };
	int ret;
	int retVal;
	if (args->cmd[TP_PARAM]) {
		if (IsLegalParam(args->paramVal) != 0) {
			printf("bad param detected\n");
			return -EINVAL;
		}
		(void)memcpy_s(oriParam, PARAM_LEN_MAX, args->paramVal, PARAM_LEN_MAX);
		paramVal = oriParam;
	}

	if (sessionID == 0) {
		printf("Bad session id\n");
		return -EINVAL;
	}

	if (CheckTeePath(args->runPath) != 0) {
		printf("bad run path\n");
		return -EINVAL
	}

	if (args->cmd[TP_INPUT]) {
		if (DoInput(args, sessionID) != 0) {
			printf("cannot input files");
			return -EFAULT;
		}
	}

	char target[PATH_MAX] = { 0 };
	switch (GetRunFileType(args->runPath, target)) {
	case JAVA_FILE_CLASS:
		ret = TeeRunJavaClass(target, sessionID, paramVal, &retVal);
		break;
	case JAVA_FILE_JAR:
		ret = TeeRunJavaJar(target, sessionID, paramVal, &retVal);
		break;
	case PYTHON_FILE_PY_PYC:
		ret = TeeRunPython(target, sessionID, paramVal, &retVal);
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

	printf("successfully run file, program returns %d\n", retVal);

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

static int32_t DoDelete(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
	if (CheckTeePath(args->deletePath) != 0) {
		printf("bad delete path\n");
		return -EINVAL;
	}
	int ret = TeeDelete(args->deletePath, sessionID);
	if (ret != 0) {
		printf("failed to delete %s\n", args->deletePath);
	} else {
		printf("successfully deleted %s\n", args->deletePath);
	}
	return ret;
}

#define FILE_NOT_FOUNED_RET 1
static int32_t DoQuery(const struct TeeTeleportArgs *args, uint32_t sessionID)
{
	bool exist;
	if (CheckTeePath(args->queryPath) != 0) {
		printf("bad query path\n");
		return -EINVAL;
	}
	int ret = TeeQuery(args->queryPath, sessionID, &exist);
	if (ret != 0) {
		printf("failed to query file\n");
		return -EFAULT;
	} else if (exist) {
		printf("file %s exists\n", args->queryPath);
		return 0;
	} else {
		printf("file %s does not exist\n", args->queryPath);
		return FILE_NOT_FOUNED_RET;
	}
}

static const struct TeeTeleportFunc g_teleportFuncTable[] = {
    {TP_HELP,       PrintUsage,  false},
	{TP_INSTALL,    DoInstall,   false},
	{TP_UNINSTALL,  DoUninstall, false},
	{TP_IMPORT,     DoInstall,   false},
	{TP_CREATE,     DoCreate,    false},
	{TP_LIST,       DoList,      false},
	{TP_RUN,        DoRun,       true},
	{TP_INPUT,      DoInput,     true},
	{TP_OUTPUT,     DoOutput,    true},
	{TP_DELETE,     DoDelete,    true},
	{TP_QUERY,      DoQuery,     true},
	{TP_DESTROY,    DoDestroy,   true},
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

static int32_t CopyArgs(bool *argkey, char *argvalue)
{
	*argkey = true;
	if (argvalue == NULL)
		return 0;

	size_t optLen = strnlen(optarg, PATH_MAX);
	if (optLen == 0 || optLen >= PATH_MAX) {
		printf("opt arg is invalid\n");
		return -EINVAL;
	}

	if (strcpy_s(argvalue, PATH_MAX, optarg) != EOK) {
		printf("strcpy_s failed\n");
		return -EFAULT;
	}
	return 0;
}

#define CASE_COPY_ARG(opt, argkey, argvalue) \
	case opt: \
        if (CopyArgs(argkey, argvalue) != 0) \
            return -EFAULT;
        break;

static int32_t ParseArgs(int32_t argc, char **argv, struct TeeTeleportArgs *args)
{
	while(1) {
		int32_t optIndex = 0;
		int32_t opt = getopt_long(argc, argv, "s:m:t:c:r:i:n:o:a:v:p:d:q:euhl", g_toolOptions, &optIndex);
		if (opt < 0)
			break;
		switch (opt) {
		CASE_COPY_ARG('s', &args->cmd[TP_INSTALL],   args->installPath);
		CASE_COPY_ARG('m', &args->cmd[TP_IMPORT],    args->importPath);
		CASE_COPY_ARG('t', &args->cmd[TP_TYPE],      args->typeParam);
		CASE_COPY_ARG('c', &args->cmd[TP_CREATE],    args->createPath);
		CASE_COPY_ARG('r', &args->cmd[TP_RUN],       args->runPath);
		CASE_COPY_ARG('i', &args->cmd[TP_ID],        args->idPath);
		CASE_COPY_ARG('n', &args->cmd[TP_INPUT],     args->inputPath);
		CASE_COPY_ARG('o', &args->cmd[TP_OUTPUT],    args->outputPath);
		CASE_COPY_ARG('a', &args->cmd[TP_RENAME],    args->renamePath);
		CASE_COPY_ARG('v', &args->cmd[TP_SAVE],      args->savePath);
		CASE_COPY_ARG('p', &args->cmd[TP_PARAM],     args->paramVal);
		CASE_COPY_ARG('d', &args->cmd[TP_DELETE],    args->deletePath);
		CASE_COPY_ARG('q', &args->cmd[TP_QUERY],     args->queryPath);
		CASE_COPY_ARG('e', &args->cmd[TP_DESTROY],   NULL);
		CASE_COPY_ARG('u', &args->cmd[TP_UNINSTALL], NULL);
		CASE_COPY_ARG('l', &args->cmd[TP_LIST],      NULL);
		CASE_COPY_ARG('h', &args->cmd[TP_HELP],      NULL);
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

