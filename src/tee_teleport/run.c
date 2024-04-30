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

#include "run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <securec.h>
#include "portal.h"
#include "scp.h"
#include "tee_client_type.h"
#include "tee_client_api.h"
#include "tee_sys_log.h"
#include "tc_ns_client.h"
#ifdef CROSS_DOMAIN_PERF
#include "posix_proxy.h"
#include <sys/prctl.h>
#endif

#define MAX_ARGV_LENGTH 4096

static int MakeArgs(int argc, char **argv, char *xargs, uint32_t *size)
{
    int i;
    uint32_t totalSize = 0;
    uint32_t argvLen   = 0;
    int argcValid = argc - 1;   // we do not care the last NULL
    for (i = 0; i < argcValid; i++) {
        if (argv[i]) {
            argvLen = (uint32_t)strnlen(argv[i], MAX_ARGV_LENGTH);
            if (argvLen == 0 || argvLen >= MAX_ARGV_LENGTH) {
                printf("argv[%u] is 0 or longer than %u\n", argvLen, MAX_ARGV_LENGTH);
                return -1;
            }
            totalSize += (argvLen + 1); // space + '\0'
        }
    }

    if (totalSize > *size || totalSize == 0) {
        printf("totalSize=%d, *size=%d\n", totalSize, *size);
        return -1;
    }

    (void)memset_s(xargs, *size, 0, *size);
    for (i = 0; i < argcValid; i++) {
        if (argv[i]) {
            if (strcat_s(xargs, *size, argv[i]) != EOK) {
                printf("xargs strcat argv failed\n");
                return -1;
            }
            if (strcat_s(xargs, *size, " ") != EOK) {
                printf("xargs strcat whitespace failed\n");
                return -1;
            }
        }
    }
    xargs[totalSize - 1] = '\0';    // last '\0'
    *size = totalSize;
    return 0;
}

static int ConfigPortal(struct TeePortalType *portal, int program, const struct TeeRunParam *runParam)
{
    if (runParam->envParam != NULL) {
        (void)memset_s(portal->args.run.envParam, PORTAL_RUN_ARGS_MAXSIZE, 0, PORTAL_RUN_ARGS_MAXSIZE);
        if (strcat_s(portal->args.run.envParam, PORTAL_RUN_ARGS_MAXSIZE, runParam->envParam) != EOK) {
            printf("strcat envParam to portal failed\n");
            return -EFAULT;
        }
    }
    portal->type = program;
    portal->sessionID = runParam->sessionID;
    return 0;
}

static int RunInitPortal(int program, int argc, char **argv, const struct TeeRunParam *runParam,
                        struct TeePortalType **retPortal)
{
    int ret = 0;
    uint32_t argSize = PORTAL_RUN_ARGS_MAXSIZE;
    uint32_t portalSize;
    struct TeePortalType *portal = NULL;

    ret = GetPortal((void**)&portal, &portalSize);
    if (ret != 0) {
        printf("get portal failed\n");
        return ret;
    }

    /* set current working directory */
    if (getcwd(portal->args.run.cwd, sizeof(portal->args.run.cwd)) == NULL) {
        ret = -errno;
        fprintf(stderr, "get current work dir failed: %s\n", strerror(-ret));
        return ret;
    }
    /* make args from argv to string */
    ret = MakeArgs(argc, argv, portal->args.run.xargs, &argSize);
    if (ret != 0) {
        printf("make args failed!\n");
        return ret;
    }
    portal->args.run.xargsSize = argSize;
    portal->reeUID = getuid();
    ret = ConfigPortal(portal, program, runParam);
    if (ret != 0) {
        printf("Config portal failed!\n");
        return ret;
    }
    *retPortal = portal;
    return ret;
}

int TeeRun(int program, int argc, char **argv, const struct TeeRunParam *runParam, int *retVal)
{
    if (argc <= 1 || argv == NULL || argv[0] == NULL || retVal == NULL) {
        printf("tee run check input failed\n");
        return -EINVAL;
    }
    int ret = 0;
    struct TeePortalType *portal = NULL;
    ret = RunInitPortal(program, argc, argv, runParam, &portal);
    if (ret != 0) {
        printf("init portal failed before run\n");
        return ret;
    }

#ifdef CROSS_DOMAIN_PERF
    ret = prctl(PR_SET_CHILD_SUBREAPER, 1);
    if (ret != 0) {
        tloge("set subreaper failed, errno %d, %s\n", errno, strerror(errno));
        return ret;
    }
    ret = PosixProxyInit();
    if (ret != 0) {
        printf("posix proxy init failed\n");
        return ret;
    } 
#endif
    ret = TriggerPortal();
    if (ret != 0) {
        printf("trigger portal failed\n");
        goto end;
    }
    *retVal = portal->ret;
end:
#ifdef CROSS_DOMAIN_PERF
    printf("trigger finished, posix proxy exit\n");
    PosixProxyDestroy();
#endif
    return ret;
}
