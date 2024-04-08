/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#define _GNU_SOURCE
#include <posix_proxy.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <fd_list.h>
#include <posix_data_handler.h>
#include <posix_ctrl_handler.h>
#include <cross_tasklet.h>
#include <common.h>
#include <portal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <securec.h>
#include <sys/wait.h>

#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

#define CTRL_TASKLET_BUFF_SIZE_KB (4 * KB)
#define CTRL_TASKLET_THREAD_CONCURRENCY 1

#define DEF_DATA_TASKLET_BUFF_SIZE 64
#define MIN_DATA_TASKLET_BUFF_SIZE 16
#define MAX_DATA_TASKLET_BUFF_SIZE 256 * 1024

#define DEF_DATA_TASKLET_BUFF_SIZE_KB ((DEF_DATA_TASKLET_BUFF_SIZE) * (KB))
#define MIN_DATA_TASKLET_BUFF_SIZE_KB ((MIN_DATA_TASKLET_BUFF_SIZE) * (KB))
#define MAX_DATA_TASKLET_BUFF_SIZE_KB ((MAX_DATA_TASKLET_BUFF_SIZE) * (KB))

#define DEF_DATA_TASKLET_THREAD_CONCURRENCY 8
#define MAX_DATA_TASKLET_THREAD_CONCURRENCY 64
#define MIN_DATA_TASKLET_THREAD_CONCURRENCY 1

struct CtrlTasklet {
    struct Xtasklet *tl;
    void *shmBuff;
    size_t shmSz;
    sem_t *destroySem;
};

struct DataTasklet {
    struct Xtasklet *tl;
    void *shmBuff;
    size_t shmSz;
    pthread_t fdListTimeoutT;      /* fdList timeout recycle thread */
    sem_t *fdListTimeoutTExitSem;  /* fdList timeout recycle thread exit sem */
};

struct PosixProxy {
    struct CtrlTasklet *ctrlTasklet;
    struct DataTasklet *dataTasklet;
    int devFd;   /* fd returned when open tvm device */
};

static struct PosixProxy *g_posix_proxy = NULL;

static unsigned int g_data_tasklet_thread_concurrency = DEF_DATA_TASKLET_THREAD_CONCURRENCY;
static unsigned int g_data_tasklet_buffer_align_sz = DEF_DATA_TASKLET_BUFF_SIZE_KB;

void SetDataTaskletThreadConcurrency(long concurrency)
{
    if (concurrency >= MIN_DATA_TASKLET_THREAD_CONCURRENCY && concurrency <= MAX_DATA_TASKLET_THREAD_CONCURRENCY) {
        g_data_tasklet_thread_concurrency = (unsigned int)concurrency;
        INFO("set posix proxy data tasklet concurrency %u\n", g_data_tasklet_thread_concurrency);
    } else {
        ERR("please set posix proxy data tasklet concurrency %u ~ %u\n",
            (unsigned int)MIN_DATA_TASKLET_THREAD_CONCURRENCY, (unsigned int)MAX_DATA_TASKLET_THREAD_CONCURRENCY);
    }
}

void SetDataTaskletBufferSize(long size)
{
    if (size < MIN_DATA_TASKLET_BUFF_SIZE || size > MAX_DATA_TASKLET_BUFF_SIZE) {
        ERR("please set tasklet buffer size %u ~ %u\n",
            (unsigned int)MIN_DATA_TASKLET_BUFF_SIZE_KB, (unsigned int)MAX_DATA_TASKLET_BUFF_SIZE_KB);
        return;
    }

    long BufferByteSize = size * KB;
    long pageSize = sysconf(_SC_PAGESIZE);
    INFO("page size is %ld\n", pageSize);
    if (pageSize <= 0) {
        ERR("get page size failed\n");
        return;
    }

    g_data_tasklet_buffer_align_sz = ALIGN_UP(BufferByteSize, pageSize);
    INFO("set align tasklet buffer size %u\n", g_data_tasklet_buffer_align_sz);
}

static int GetTaskletBuffer(size_t sz, void **retAlignBuff, uint32_t *retAlignSz)
{
    if (retAlignBuff == NULL || retAlignSz == NULL) {
        ERR("bad paramters\n");
        return -EINVAL;
    }
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) {
        ERR("get page size failed\n");
        return -EFAULT;
    }

    *retAlignSz = ALIGN_UP(sz, pageSize);
    *retAlignBuff = aligned_alloc(pageSize, *retAlignSz);
    if (*retAlignBuff == NULL) {
        ERR("malloc page size buf failed\n");
        return -ENOMEM;
    }
    return 0;
}

static int CreatCtrlTasklet(void *shm, size_t shmSz, struct CtrlTasklet **retCtrlTasklet)
{
    int ret = 0;
    struct Xtasklet *ctrlExecutor = NULL;
    struct XtaskletCreateProps props = {
        .shm = shm, .shmSz = shmSz, .concurrency = CTRL_TASKLET_THREAD_CONCURRENCY,
        .fn = PosixCtrlTaskletCallHandler, .priv = NULL
    };

    ret = XtaskletCreate(&props, &ctrlExecutor);
    if (ret != 0) {
        ERR("create ctrl tasklet executor failed\n");
        return ret;
    }

    struct CtrlTasklet *ctrlTasklet = calloc(1, sizeof(struct CtrlTasklet));
    if (ctrlTasklet == NULL) {
        ERR("has no enough memory for ctrlTasklet\n");
        ret = -ENOMEM;
        goto free_tasklet;
    }

    ctrlTasklet->tl = ctrlExecutor;
    ctrlTasklet->shmBuff = shm;
    ctrlTasklet->shmSz = shmSz;
    *retCtrlTasklet = ctrlTasklet;
    goto end;

free_tasklet:
    XtaskletDestroy(ctrlExecutor);
end:
    return ret;
}

static void FreeCtrlTasklet(struct CtrlTasklet *ctrlTasklet)
{
    if (ctrlTasklet == NULL)
        return;

    if (ctrlTasklet->tl != NULL)
        XtaskletDestroy(ctrlTasklet->tl);

    if (ctrlTasklet->shmBuff != NULL)
        free(ctrlTasklet->shmBuff);

    if (ctrlTasklet->destroySem != NULL) {
        (void)sem_destroy(ctrlTasklet->destroySem);
        free(ctrlTasklet->destroySem);
    }

    free(ctrlTasklet);
}

static int PosixProxyCreatCtrlTasklet(int devFd, sem_t *ctrlTaskletDestroySem)
{
    if (devFd < 0 || ctrlTaskletDestroySem == NULL) {
        ERR("bad parameters for sem\n");
        return -EINVAL;
    }

    if (g_posix_proxy != NULL) {
        close(g_posix_proxy->devFd);
        free(g_posix_proxy);
        g_posix_proxy = NULL;
    }

    struct PosixProxyIoctlArgs args = { .shmType = CTRL_TASKLET_BUFF, .bufferSize = 0, .buffer = NULL};
    int ret = GetTaskletBuffer(CTRL_TASKLET_BUFF_SIZE_KB, &args.buffer, &args.bufferSize);
    if (ret != 0) {
        ERR("get ctrl tasklet buffer failed\n");
        return ret;
    }

    ret = madvise(args.buffer, args.bufferSize, MADV_DONTFORK);
    if (ret != 0) {
        ERR("madvise ctrl tasklet shm buff failed\n");
        free(args.buffer);
        return ret;
    }

    struct CtrlTasklet *ctrlTasklet = NULL;
    ret = CreatCtrlTasklet(args.buffer, args.bufferSize, &ctrlTasklet);
    if (ret != 0) {
        ERR("create ctrl tasklet failed\n");
        free(args.buffer);
        return ret;
    }

    ret = PosixProxyRegisterTaskletRequest(devFd, &args);
    if (ret != 0) {
        ERR("register ctrl tasklet request failed\n");
        goto free_ctrlTasklet;
    }

    g_posix_proxy = (struct PosixProxy *)calloc(1, sizeof(struct PosixProxy));
    if (g_posix_proxy == NULL) {
        ERR("has no enough memory for global posix proxy info\n");
        ret = -ENOMEM;
        goto free_ctrlTasklet;
    }

    ctrlTasklet->destroySem = ctrlTaskletDestroySem;
    g_posix_proxy->ctrlTasklet = ctrlTasklet;
    g_posix_proxy->devFd = devFd;
    goto end;

free_ctrlTasklet:
    FreeCtrlTasklet(ctrlTasklet);
end:
    return ret;
}

int PosixProxyInit(void)
{
    int ret = 0;
    (void)signal(SIGPIPE, SIG_IGN);

    int devFd = PosixProxyInitDev();
    if (devFd < 0) {
        ERR("posix proxy init failed\n");
        return devFd;
    }

    sem_t *ctrlFlowDestroySem = (sem_t *)calloc(1, sizeof(sem_t));
    if (ctrlFlowDestroySem == NULL) {
        ERR("has no enough memory for ctrlFlowDestroySem, err: %s\n", strerror(errno));
        ret = -ENOMEM;
        goto free_fd;
    }

    ret = sem_init(ctrlFlowDestroySem, 0, 0);
    if (ret < 0) {
        ERR("ctrlFlowDestroySem init failed, errno: %d, err: %s\n", errno, strerror(errno));
        goto free_exit_sem;
    }

    ret = PosixProxyCreatCtrlTasklet(devFd, ctrlFlowDestroySem);
    if (ret != 0) {
        ERR("create posix proxy ctrl tasklet failed\n");
        goto destroy_exit_sem;
    }
    return ret;

destroy_exit_sem:
    (void)sem_destroy(ctrlFlowDestroySem);
free_exit_sem:
    (void)free(ctrlFlowDestroySem);
free_fd:
    PosixProxyExitDev(devFd);
    return ret;
}

static void FreeDataTasklet(struct DataTasklet *dataTasklet);

void PosixProxyDestroy(void)
{
    if (g_posix_proxy == NULL)
        return;

    FreeCtrlTasklet(g_posix_proxy->ctrlTasklet);
    FreeDataTasklet(g_posix_proxy->dataTasklet);
    PosixProxyExitDev(g_posix_proxy->devFd);

    free(g_posix_proxy);
    g_posix_proxy = NULL;

    pid_t childPid = 0;
    int status = -1;
    while ((childPid = wait(&status)) != -1) {
        INFO("Child process %d exited with status %d\n", childPid, WEXITSTATUS(status));
    }
}

struct ChildPosixProxyArg {
    sem_t *ctrlTaskletCreatSem;
    int *shmResAddr;
};

static int ChildPosixProxyFunc(void *arg)
{
    int ret = -1;
    struct ChildPosixProxyArg *childArg = (struct ChildPosixProxyArg *)arg;

    ret = PosixProxyInit();
    if (ret != 0) {
        ERR("child init posix proxy failed\n");
        goto child_end;
    } else {
        *(childArg->shmResAddr) = ret;
        (void)sem_post(childArg->ctrlTaskletCreatSem);
    }

    sem_wait(g_posix_proxy->ctrlTasklet->destroySem);
    PosixProxyDestroy();
    return ret;

child_end:
    *childArg->shmResAddr = ret;
    (void)sem_post(childArg->ctrlTaskletCreatSem);
    return ret;
}

#define SEM_NAME_PREFIX  "/ctrlTaskletCreatSem"
#define SHM_SIZE   1024
#define SHM_ATTR   0666
#define STACK_SIZE (8 * 1024 * 1024)
#define CHILD_CREAT_POSIX_PROXY_TIMEOUT 3   /* 3s */

static int CreateChildWaitResult(struct ChildPosixProxyArg *args, int *child_pid)
{
    int ret = 0;

    void *stack = calloc(STACK_SIZE, 1);
    if (stack == NULL) {
        ERR("calloc for child stack fialed, %s\n", strerror(errno));
        ret = -errno;
        return ret;
    }

    *child_pid = clone(ChildPosixProxyFunc, stack + STACK_SIZE, SIGCHLD, args);
    if (*child_pid == -1) {
        ERR("clone failed, %s\n", strerror(errno));
        ret = -errno;
        goto free_stack;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += CHILD_CREAT_POSIX_PROXY_TIMEOUT;
    if (sem_timedwait(args->ctrlTaskletCreatSem, &ts) != 0) {
        ERR("child create ctrl tasklet is timeout\n");
    }

    ret = *args->shmResAddr;

free_stack:
    (void)free(stack);
    return ret;
}

int PosixProxyRegisterCtrlTasklet(void)
{
    int ret = 0;
    int child_pid = -1;
    char sem_name[PATH_MAX] = {0};

    if (sprintf_s(sem_name, PATH_MAX, "%s_%d", SEM_NAME_PREFIX, getpid()) < 0) {
        ERR("sem name acquire failed\n");
        return -EINVAL;
    }

    sem_t *ctrlTaskletCreatSem = sem_open(sem_name, O_CREAT, 0600, 0);
    if (ctrlTaskletCreatSem == SEM_FAILED) {
        ERR("ctrlTaskletCreatSem open failed, %s\n", strerror(errno));
        ret = -errno;
        goto end;
    }

    int shmId = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | SHM_ATTR);
    if (shmId == -1) {
        ERR("shmget failed, %s\n", strerror(errno));
        ret = -errno;
        goto destroy_sem;
    }

    int *shmResAddr = (int *)shmat(shmId, NULL, 0);
    if (shmResAddr == NULL) {
        ERR("shmat failed, %s\n", strerror(errno));
        ret = -errno;
        goto destroy_shmId;
    }

    *shmResAddr = -1;
    struct ChildPosixProxyArg args = { .ctrlTaskletCreatSem = ctrlTaskletCreatSem, .shmResAddr = shmResAddr };
    ret = CreateChildWaitResult(&args, &child_pid);
    (void)shmdt(shmResAddr);

destroy_shmId:
    (void)shmctl(shmId, IPC_RMID, NULL);
destroy_sem:
    (void)sem_close(ctrlTaskletCreatSem);
    (void)sem_unlink(sem_name);
end:
    return (ret != 0 ? ret : child_pid);
}

#define PKG_TIMEOUT_THREAD_SLEEP_S 10

static void *FdListPkgTimeoutThreadWork(void *arg)
{
    struct DataTasklet *dataTasklet = (struct DataTasklet *)arg;
    while (true) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += PKG_TIMEOUT_THREAD_SLEEP_S;

        if (sem_timedwait(dataTasklet->fdListTimeoutTExitSem, &ts) == 0) {
            break;
        } else {
            if (dataTasklet->tl != NULL) {
                FdListDelTimeoutPkg((struct FdList *)dataTasklet->tl->priv);
            }
        }
    }

    return NULL;
}

static void FdListTimeoutThreadDestroy(struct DataTasklet *dataTasklet)
{
    if (sem_post(dataTasklet->fdListTimeoutTExitSem) != 0) {
        ERR("fdListTimeoutThread_exit_sem post failed, errno: %d, err: %s\n", errno, strerror(errno));
    }
    (void)pthread_join(dataTasklet->fdListTimeoutT, NULL);
    (void)sem_destroy(dataTasklet->fdListTimeoutTExitSem);
    (void)free(dataTasklet->fdListTimeoutTExitSem);
    dataTasklet->fdListTimeoutTExitSem = NULL;
}

static int FdListTimeoutThreadCreat(struct DataTasklet *dataTasklet)
{
    int ret = 0;
    dataTasklet->fdListTimeoutTExitSem = (sem_t *)calloc(1, sizeof(sem_t));
    if (dataTasklet->fdListTimeoutTExitSem == NULL) {
        ERR("has no enough memory for fdListTimeoutThread_exit_sem\n");
        return -ENOMEM;
    }

    ret = sem_init(dataTasklet->fdListTimeoutTExitSem, 0, 0);
    if (ret < 0) {
        ERR("fdListTimeoutThread_exit_sem init failed, errno: %d, err: %s\n", errno, strerror(errno));
        free(dataTasklet->fdListTimeoutTExitSem);
        return ret;
    }

    ret = pthread_create(&dataTasklet->fdListTimeoutT, NULL, FdListPkgTimeoutThreadWork, (void *)dataTasklet);
    if (ret != 0) {
        ERR("create fdList's pkg timeout recycling pthread failed: %s\n", strerror(ret));
        goto destroy_exit_sem;
    }
    return ret;

destroy_exit_sem:
    (void)sem_destroy(dataTasklet->fdListTimeoutTExitSem);
    (void)free(dataTasklet->fdListTimeoutTExitSem);
    dataTasklet->fdListTimeoutTExitSem = NULL;
    return ret;
}

static int CreatDataTasklet(void *shm, size_t shmSz, struct DataTasklet **retdataTasklet)
{
    int ret = 0;
    struct FdList *fdList = NULL;

    ret = FdListInit(&fdList);
    if (ret != 0) {
        ERR("init fd list failed\n");
        return ret;
    }

    struct Xtasklet *dataExecutor = NULL;
    struct XtaskletCreateProps props = {
        .shm = shm, .shmSz = shmSz, .concurrency = g_data_tasklet_thread_concurrency,
        .fn = PosixDataTaskletCallHandler, .priv = fdList
    };
    ret = XtaskletCreate(&props, &dataExecutor);
    if (ret != 0) {
        ERR("create data tasklet executor failed\n");
        goto free_fdList;
    }

    struct DataTasklet *dataTasklet = calloc(1, sizeof(struct DataTasklet));
    if (dataTasklet == NULL) {
        ERR("has no enough memory for dataTasklet\n");
        ret = -ENOMEM;
        goto free_executor;
    }

    ret = FdListTimeoutThreadCreat(dataTasklet);
    if (ret != 0) {
        ERR("create fdList timeout thread for dataTasklet failed\n");
        goto free_dataTasklet;
    }

    dataTasklet->tl = dataExecutor;
    dataTasklet->shmBuff = shm;
    dataTasklet->shmSz = shmSz;
    *retdataTasklet = dataTasklet;
    goto end;

free_dataTasklet:
    free(dataTasklet);
free_executor:
    XtaskletDestroy(dataExecutor);
free_fdList:
    FdListDestroy(fdList);
end:
    return ret;
}

static void FreeDataTasklet(struct DataTasklet *dataTasklet)
{
    if (dataTasklet == NULL)
        return;
    
    FdListTimeoutThreadDestroy(dataTasklet);

    if (dataTasklet->tl != NULL) {
        FdListDestroy((struct FdList *)dataTasklet->tl->priv);
        XtaskletDestroy(dataTasklet->tl);
    }

    if (dataTasklet->shmBuff != NULL) {
        (void)memset_s(dataTasklet->shmBuff, dataTasklet->shmSz, 0, dataTasklet->shmSz);
        free(dataTasklet->shmBuff);
    }

    free(dataTasklet);
}

int PosixProxyRegisterDataTasklet(void)
{
    if (g_posix_proxy == NULL) {
        ERR("global tasklet info is NULL\n");
        return -EFAULT;
    }

    FreeDataTasklet(g_posix_proxy->dataTasklet);
    g_posix_proxy->dataTasklet = NULL;

    struct PosixProxyIoctlArgs args = { .shmType = DATA_TASKLET_BUFF, .bufferSize = 0, .buffer = NULL};
    int ret = GetTaskletBuffer(g_data_tasklet_buffer_align_sz, &args.buffer, &args.bufferSize);
    if (ret != 0) {
        ERR("get data tasklet buffer failed\n");
        return ret;
    }

    ret = madvise(args.buffer, args.bufferSize, MADV_DONTFORK);
    if (ret != 0) {
        ERR("madvise data tasklet shm buff failed\n");
        free(args.buffer);
        return ret;
    }

    struct DataTasklet *dataTasklet = NULL;
    ret = CreatDataTasklet(args.buffer, args.bufferSize, &dataTasklet);
    if (ret != 0) {
        ERR("create data flow failed\n");
        free(args.buffer);
        return ret;
    }

    ret = PosixProxyRegisterTaskletRequest(g_posix_proxy->devFd, &args);
    if (ret != 0) {
        ERR("register data tasklet request failed\n");
        goto free_dataTasklet;
    }
    g_posix_proxy->dataTasklet = dataTasklet;
    goto end;

free_dataTasklet:
    FreeDataTasklet(dataTasklet);
end:
    return ret;
}

int PosixProxyUnregisterAllTasklet(void)
{
    if (g_posix_proxy == NULL) {
        ERR("global tasklet info is NULL\n");
        return -EFAULT;
    }

    FreeDataTasklet(g_posix_proxy->dataTasklet);
    g_posix_proxy->dataTasklet = NULL;
    sem_post(g_posix_proxy->ctrlTasklet->destroySem);

    return 0;
}