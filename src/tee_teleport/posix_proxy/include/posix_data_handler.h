/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: posix proxy handler definations
 */
#ifndef REE_POSIX_CALL_H
#define REE_POSIX_CALL_H

#include <stdint.h>
#include <stdlib.h>

struct PosixCall {
    uint32_t type;
    uint32_t func;
    int err;
    size_t argsSz;
    char args[0];
};

struct PosixProxyParam {
    void *args;
    size_t argsSz;
    uint32_t argsCnt;
    void *ctx;
};

typedef long (*PosixProxy)(struct PosixProxyParam *);

enum PosixCallTypes {
    POSIX_CALL_FILE         = 0,
    POSIX_CALL_NETWORK      = 1,
    POSIX_CALL_OTHER        = 2,
};

enum file_posix_call_fns {
    /* enum file-relative posix calls here */
    FILE_OPEN               = 1,
    FILE_OPENAT             = 2,
    FILE_READ               = 3,
    FILE_WRITE              = 4,
    FILE_CLOSE              = 5,
    FILE_ACCESS             = 6,
    FILE_FACCESSAT          = 7,
    FILE_LSEEK              = 8,
    FILE_CHDIR              = 9,
    FILE_FCHDIR             = 10,
    FILE_CHMOD              = 11,
    FILE_FCHMOD             = 12,
    FILE_FCHMODAT           = 13,
    FILE_STAT               = 14,
    FILE_FSTAT              = 15,
    FILE_STATFS             = 16,
    FILE_FSTATFS            = 17,
    FILE_LSTAT              = 18,
    FILE_FSTATAT            = 19,
    FILE_SYMLINK            = 20,
    FILE_SYMLINKAT          = 21,
    FILE_READLINK           = 22,
    FILE_READLINKAT         = 23,
    FILE_FSYNC              = 24,
    FILE_TRUNCATE           = 25,
    FILE_FTRUNCATE          = 26,
    FILE_RENAME             = 27,
    FILE_RENAMEAT           = 28,
    FILE_DUP2               = 29,
    FILE_MKDIR              = 30,
    FILE_UMASK              = 31,
    FILE_UNLINK             = 32,
    FILE_FCNTL              = 33,
    FILE_MMAP_UTIL          = 34,
    FILE_MSYNC_UTIL         = 35,
    FILE_SENDFILE           = 36,
    FILE_REMOVE             = 37,
    FILE_RMDIR              = 38,
    FILE_DUP                = 39,
    FILE_REALPATH           = 40,
};

enum NetworkPosixCallFns {
    /* enum network-relative posix calls here */
    NET_SOCKET              = 1,
    NET_CONNECT             = 2,
    NET_BIND                = 3,
    NET_LISTEN              = 4,
    NET_ACCEPT              = 5,
    NET_ACCEPT4             = 6,
    NET_SHUTDOWN            = 7,
    NET_GETSOCKNAME         = 8,
    NET_GETSOCKOPT          = 9,
    NET_SETSOCKOPT          = 10,
    NET_GETPEERNAME         = 11,
    NET_SENDTO              = 12,
    NET_RECVFROM            = 13,
    NET_SENDMSG             = 14,
    NET_RECVMSG             = 15,
    NET_GETADDRINFO         = 16,
    NET_GETADDRINFO_DOFETCH = 17,
    NET_FREEADDRINFO        = 18,
    NET_RES_INIT            = 19,
};

enum OtherPosixCallFns {
    /* enum other posix calls here */
    OTHER_PLACE_HOLDER      = 0,
    OTHER_EPOLL_CREATE1     = 1,
    OTHER_EPOLL_CTL         = 2,
    OTHER_EPOLL_PWAIT       = 3,
    OTHER_EVENTFD           = 4,
    OTHER_SELECT            = 5,
    OTHER_PKG_SEND          = 6,
    OTHER_PKG_RECV          = 7,
    OTHER_PKG_TERMINATE     = 8,
    OTHER_IOCTL             = 9,
    OTHER_POLL              = 10,
    OTHER_GETRLIMIT         = 11,
};

enum PosixCallArgCount {
    /* enum posix call arg count here */
    POSIX_CALL_ARG_COUNT_0       = 0,
    POSIX_CALL_ARG_COUNT_1       = 1,
    POSIX_CALL_ARG_COUNT_2       = 2,
    POSIX_CALL_ARG_COUNT_3       = 3,
    POSIX_CALL_ARG_COUNT_4       = 4,
    POSIX_CALL_ARG_COUNT_5       = 5,
    POSIX_CALL_ARG_COUNT_6       = 6,
    POSIX_CALL_ARG_COUNT_7       = 7,
};

struct PosixFunc {
    uint32_t argsCnt;
    PosixProxy funcPtr;
};

/* make a enum to posix proxy function */
#define POSIX_FUNC_ENUM(FUNC_NO, NAME, ARGS_CNT) \
    [(FUNC_NO)] = { .argsCnt = (ARGS_CNT), .funcPtr = (NAME) }

/* declare posix proxy functions */
#define POSIX_FUNCS_DECLARE(TYPE) \
struct PosixFunc *Get##TYPE##Functions(void); \
size_t Get##TYPE##FunctionsSize(void);

/* implement posix proxy functions */
#define POSIX_FUNCS_IMPL(TYPE, FUNC_IMPLS) \
struct PosixFunc *Get##TYPE##Functions(void) \
{ \
    return (struct PosixFunc *)(FUNC_IMPLS); \
} \
size_t Get##TYPE##FunctionsSize(void) \
{ \
    return sizeof(FUNC_IMPLS) / sizeof(struct PosixFunc); \
}

/* get posix proxy functions */
#define POSIX_FUNCS_GET(TYPE) \
    Get##TYPE##Functions()

#define POSIX_FUNCS_SIZE(TYPE) \
    Get##TYPE##FunctionsSize()

POSIX_FUNCS_DECLARE(POSIX_FILE)
POSIX_FUNCS_DECLARE(POSIX_NETWORK)
POSIX_FUNCS_DECLARE(POSIX_OTHER)

long PosixDataTaskletCallHandler(uint8_t *membuf, void *priv);

#endif
