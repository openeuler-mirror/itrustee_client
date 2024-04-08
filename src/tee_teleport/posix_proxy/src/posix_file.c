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
#include <posix_data_handler.h>
#include <errno.h>
#include <unistd.h>
#include <securec.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/vfs.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <limits.h>
#include "cross_tasklet.h"
#include "serialize.h"
#include "posix_proxy.h"

static long FileOpenWork(struct PosixProxyParam *param)
{
    int fd = -1;
    char *pathname = NULL;
    uint64_t flags = 0;
    uint64_t mode = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &pathname, INTEGERTYPE,
                    &flags, INTEGERTYPE, &mode) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    DBG("start to call open, pathname %s\n", pathname);
    fd = open(pathname, flags, mode);

    return fd;
}

static long FileOpenatWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = -1;
    uint64_t flags = 0;
    uint64_t mode = 0;
    char *filename = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, POINTTYPE, &filename,
                   INTEGERTYPE, &flags, INTEGERTYPE, &mode) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    DBG("start to call openat, pathname %s\n", filename);
    ret = openat(fd, filename, flags, mode);

    return ret;
}

static long FileReadWork(struct PosixProxyParam *param)
{
    ssize_t ret = -1;
    uint64_t fd = -1;
    uint64_t count = 0;
    uint8_t *buf = NULL;
    uint8_t *tmpBuf = NULL;

    buf = param->args;
    if (DeSerialize(param->argsCnt, param->args, param->argsSz,
                    INTEGERTYPE, &fd, POINTTYPE, &tmpBuf, INTEGERTYPE, &count) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = read(fd, buf, count);

    return ret;
}

static long FileWriteWork(struct PosixProxyParam *param)
{
    ssize_t ret = -1;
    uint64_t fd = -1;
    uint64_t count = 0;
    uint8_t *buf = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd,
        POINTTYPE, &buf, INTEGERTYPE, &count) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = write(fd, buf, count);

    return ret;
}

static int g_reePreCloseFD = -1; /* File descriptor to which we dup other fd's before closing them for real */

static int g_reePosixProxyDevFD = -1; /* File descriptor of opened tvm driver, cannot be closed by tee app */

void PosixProxySetDevFD(int devFD)
{
    g_reePosixProxyDevFD = devFD;
}

static long FileCloseWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = -1;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    if ((int)fd == g_reePosixProxyDevFD || (int)fd == g_reePreCloseFD) {
        ret = -1;
        errno = EBADF;
    } else {
        ret = close(fd);
    }

    return ret;
}

static long FileAccessWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *filename = NULL;
    uint64_t amode = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &filename, INTEGERTYPE, &amode) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = access(filename, amode);

    return ret;
}

static long FileFaccessatWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *filename = NULL;
    uint64_t fd = -1;
    uint64_t amode = 0;
    uint64_t flag = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, POINTTYPE, &filename,
        INTEGERTYPE, &amode, INTEGERTYPE, &flag) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = faccessat(fd, filename, amode, flag);

    return ret;
}

static long FileLseekWork(struct PosixProxyParam *param)
{
    off_t ret = -1;
    uint64_t fd = -1;
    uint64_t offset = 0;
    uint64_t whence = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, INTEGERTYPE, &offset,
                    INTEGERTYPE, &whence) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = lseek(fd, offset, whence);

    return ret;
}

static long FileChdirWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *path = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &path) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = chdir(path);

    return ret;
}

static long FileFchdirWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = -1;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = fchdir(fd);

    return ret;
}

static long FileChmodWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *path = NULL;
    uint64_t mode = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &path, INTEGERTYPE, &mode) != 0 ) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = chmod(path, mode);

    return ret;
}

static long FileFchmodWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = -1;
    uint64_t mode = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, INTEGERTYPE, &mode) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = fchmod(fd, mode);

    return ret;
}

static long FileFchmodatWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *path = NULL;
    uint64_t fd = -1;
    uint64_t mode = 0;
    uint64_t flag = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, POINTTYPE, &path,
                    INTEGERTYPE, &mode, INTEGERTYPE, &flag) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = fchmodat(fd, path, mode, flag);

    return ret;
}

static long FileStatWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *path = NULL;
    struct stat *buf = NULL;

    /* should contain two arg, avoid to operate same memory */
    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &path, POINTTYPE, &buf) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = stat(path, buf);

    return ret;
}

static long FileFstatWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = -1;
    struct stat *statbuf = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, POINTTYPE, &statbuf) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = fstat(fd, statbuf);

    return ret;
}

static long FileStatfsWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *path = NULL;
    struct statfs *buf = NULL;

    /* should contain two arg, avoid to operate same memory */
    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &path, POINTTYPE, &buf) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = statfs(path, buf);

    return ret;
}

static long FileFstatfsWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = -1;
    struct statfs *buf = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, POINTTYPE, &buf) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = fstatfs(fd, buf);

    return ret;
}

static long FileLstatWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *path = NULL;
    struct stat *buf = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &path, POINTTYPE, &buf) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = lstat(path, buf);

    return ret;
}

static long FileFstatatWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = -1;
    uint64_t flag = 0;
    char *path = NULL;
    struct stat *st = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, POINTTYPE, &path,
                    POINTTYPE, &st, INTEGERTYPE, &flag) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = fstatat(fd, path, st, flag);

    return ret;
}

static long FileSymlinkWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *existing = NULL;
    char *new = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &existing, POINTTYPE, &new) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = symlink(existing, new);

    return ret;
}

static long FileSymlinkatWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *existing = NULL;
    char *new = NULL;
    uint64_t fd = -1;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &existing,
                    INTEGERTYPE, &fd, POINTTYPE, &new) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = symlinkat(existing, fd, new);

    return ret;
}

static long FileReadlinkWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t bufsize = 0;
    char *path = NULL;
    char *buf = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &path,
                    POINTTYPE, &buf, INTEGERTYPE, &bufsize) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = readlink(path, buf, bufsize);

    return ret;
}

static long FileReadlinkatWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t bufsize = 0;
    uint64_t fd = -1;
    char *path = NULL;
    char *buf = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd,
                    POINTTYPE, &path, POINTTYPE, &buf, INTEGERTYPE, &bufsize) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = readlinkat(fd, path, buf, bufsize);

    return ret;
}

static long FileFsyncWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = -1;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = fsync(fd);

    return ret;
}

static long FileTruncateWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *path = NULL;
    uint64_t length = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &path, INTEGERTYPE, &length) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = truncate(path, length);

    return ret;
}

static long FileFtruncateWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = -1;
    uint64_t length = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, INTEGERTYPE, &length) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = ftruncate(fd, length);

    return ret;
}

static long FileRenameWork(struct PosixProxyParam *param)
{
    int ret = -1;
    const char *old = NULL;
    const char *new = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &old, POINTTYPE, &new) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = rename(old, new);

    return ret;
}

static long FileRenameatWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *old = NULL;
    char *new = NULL;
    uint64_t oldfd = -1;
    uint64_t newfd = -1;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &oldfd, POINTTYPE, &old,
                    INTEGERTYPE, &newfd, POINTTYPE, &new) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = renameat(oldfd, old, newfd, new);

    return ret;
}

static pthread_mutex_t g_reePreCloseFDInitLock = PTHREAD_MUTEX_INITIALIZER;

static int getReePreCloseFD(void)
{
    if (g_reePreCloseFD == -1) {
        if (pthread_mutex_lock(&g_reePreCloseFDInitLock) != 0) {
            ERR("get mutex for g_reePreCloseFDInitLock failed\n");
            return -1;
        }

        if (g_reePreCloseFD == -1) {
            int sp[2];
            if (socketpair(PF_UNIX, SOCK_STREAM, 0, sp) < 0) {
                ERR("socketpair created failed\n");
                (void)pthread_mutex_unlock(&g_reePreCloseFDInitLock);
                return -1;
            }
            g_reePreCloseFD = sp[0];
            close(sp[1]);
        }
        (void)pthread_mutex_unlock(&g_reePreCloseFDInitLock);
    }

    return g_reePreCloseFD;
}

static long FileDup2Work(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t oldfd = -1;
    uint64_t newfd = -1;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &oldfd, INTEGERTYPE, &newfd) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    if ((int)oldfd == -1) {
        int preCloseFD = getReePreCloseFD();
        if (preCloseFD == -1) {
            errno = EIO;
            return -1;
        }
        ret = dup2(preCloseFD, newfd);
    } else if ((int)oldfd == g_reePosixProxyDevFD || (int)oldfd == g_reePreCloseFD ||
               (int)newfd == g_reePosixProxyDevFD || (int)newfd == g_reePreCloseFD) {
        ret = -1;
        errno = EPERM;
    } else {
        ret = dup2(oldfd, newfd);
    }

    return ret;
}

static long FileMkdirWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *pathname = NULL;
    uint64_t mode = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &pathname, INTEGERTYPE, &mode) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = mkdir(pathname, mode);

    return ret;
}

static long FileUmaskWork(struct PosixProxyParam *param)
{
    mode_t ret = -1;
    uint64_t mode = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &mode) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    ret = umask(mode);

    return ret;
}

static long FileUnlinkWork(struct PosixProxyParam *param)
{
    int ret = -1;
    char *path = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &path) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    ret = unlink(path);

    return ret;
}

static long FileFcntlWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = -1;
    uint64_t cmd = 0;
    uint8_t *arg = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd,
                    INTEGERTYPE, &cmd, POINTTYPE, &arg) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    if (cmd == F_SETLK || cmd == F_SETLKW || cmd == F_GETLK) {
        ret = fcntl(fd, cmd, (struct flock *)arg);
    } else {
        ret = fcntl(fd, cmd, *(unsigned long *)arg);
    }

    if (ret == 0 && cmd == F_GETLK) {
        memcpy_s(param->args, param->argsSz, arg, sizeof(struct flock));
    }

    return ret;
}

/* mmap_util get the file path, open new fd, lseek file offset */
static long FileMmapUtilWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t fd = 0, offset = 0;
    char *path = NULL;
    int newFd = -1;

    ret = DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd, INTEGERTYPE, &offset,
        POINTTYPE, &path);
    if (ret != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    char fdPath[PATH_MAX] = {0};
    if (sprintf_s(fdPath, PATH_MAX, "/proc/self/fd/%d", fd) <= 0) {
        ERR("concat proc fd path failed\n");
        errno = EINVAL;
        return -1;
    }

    path = (char *)param->args;
    (void)memset_s(path, param->argsSz, 0, param->argsSz);
    if (readlink(fdPath, path, PATH_MAX - 1) < 0) {
        ERR("readlink proc fd path failed\n");
        return -1;
    }

    newFd = open(path, O_RDONLY);
    if (newFd < 0) {
        ERR("open new fd failed\n");
        return -1;
    }

    if (lseek(newFd, offset, SEEK_SET) < 0) {
        ERR("lseek offset failed\n");
        close(newFd);
        return -1;
    }

    return newFd;
}

/*msync_util open new fd, lseek file offset */
static long FileMsyncUtilWork(struct PosixProxyParam *param)
{
    int ret = -1;
    uint64_t offset = 0;
    char *path = NULL;
    struct stat sb = {0};

    ret = DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &path, INTEGERTYPE, &offset);
    if (ret != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        return ret;
    }

    int newFd = open(path, O_RDWR);
    if (newFd < 0) {
        ERR("open new fd failed\n");
        return -1;
    }

    if (lseek(newFd, offset, SEEK_SET) < 0) {
        ERR("lseek offset failed\n");
        close(newFd);
        return -1;
    }

    if (fstat(newFd, &sb) == -1) {
        ERR("fstat get file size failed\n");
        close(newFd);
        return -1;
    }

    uint64_t file_size = sb.st_size;
    uint64_t left_size = file_size > offset ? file_size - offset : 0;
    /* when left size is 0, tee no need to back data to file, just return fake fd */
    if (left_size == 0) {
        close(newFd);
        return newFd;
    }

    if (Serialize(1, param->args, param->argsSz, INTEGERTYPE, left_size) != 0) {
        ERR("serialize file size failed\n");
        close(newFd);
        return -1;
    }

    return newFd;
}

static long FileSendfileWork(struct PosixProxyParam *param)
{
    ssize_t ret = -1;
    uint64_t outFd = -1;
    uint64_t inFd = -1;
    off_t *offset = NULL;
    uint64_t count = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &offset, INTEGERTYPE, &outFd,
        INTEGERTYPE, &inFd, INTEGERTYPE, &count) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        return -1;
    }

    ret = sendfile((int)outFd, (int)inFd, offset, (size_t)count);
    if (ret == -1) {
        DBG("failed to sendfile, errno = %d, err = %s\n", errno, strerror(errno));
        return -1;
    }

    return ret;
}

static long FileRemoveWork(struct PosixProxyParam *param)
{
    int ret = 0;
    char *pathname = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &pathname) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    DBG("start to call remove, pathname %s\n", pathname);
    ret = remove(pathname);
    if (ret != 0) {
        DBG("failed to call remove, errno = %d, err = %s\n", errno, strerror(errno));
    }

    return ret;
}

static long FileRmdirWork(struct PosixProxyParam *param)
{
    int ret = 0;
    char *pathname = NULL;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, POINTTYPE, &pathname) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    DBG("start to call rmdir, pathname %s\n", pathname);
    ret = rmdir(pathname);
    if (ret != 0) {
        DBG("failed to call rmdir, errno = %d, err = %s\n", errno, strerror(errno));
    }

    return ret;
}

static long FileDupWork(struct PosixProxyParam *param)
{
    int ret = 0;
    uint64_t fd = 0;

    if (DeSerialize(param->argsCnt, param->args, param->argsSz, INTEGERTYPE, &fd) != 0) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }
    if ((int)fd == g_reePosixProxyDevFD || (int)fd == g_reePreCloseFD) {
        ret = -1;
        errno = EPERM;
    } else {
        ret = dup(fd);
        if (ret < 0) {
            DBG("failed to call rmdir, errno = %d, err = %s\n", errno, strerror(errno));
        }
    }

    return ret;
}

static long FileRealpath(struct PosixProxyParam *param)
{
    int ret = -1;
    char *filename = NULL;
    char *resolved_path = NULL;

    ret = DeSerialize(param->argsCnt, param->args, param->argsSz,
        POINTTYPE, &filename, POINTTYPE, &resolved_path);
    if (ret != 0 || filename == NULL || resolved_path == NULL) {
        ERR("[%s] Deserialize failed\n", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    if (realpath(filename, resolved_path) == NULL) {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

static struct PosixFunc g_funcs[] = {
    /* enum file-related posix calls here */
    POSIX_FUNC_ENUM(FILE_OPEN, FileOpenWork, 3),
    POSIX_FUNC_ENUM(FILE_OPENAT, FileOpenatWork, 4),
    POSIX_FUNC_ENUM(FILE_WRITE, FileWriteWork, 3),
    POSIX_FUNC_ENUM(FILE_READ, FileReadWork, 3),
    POSIX_FUNC_ENUM(FILE_CLOSE, FileCloseWork, 1),
    POSIX_FUNC_ENUM(FILE_ACCESS, FileAccessWork, 2),
    POSIX_FUNC_ENUM(FILE_FACCESSAT, FileFaccessatWork, 4),
    POSIX_FUNC_ENUM(FILE_LSEEK, FileLseekWork, 3),
    POSIX_FUNC_ENUM(FILE_CHDIR, FileChdirWork, 1),
    POSIX_FUNC_ENUM(FILE_FCHDIR, FileFchdirWork, 1),
    POSIX_FUNC_ENUM(FILE_CHMOD, FileChmodWork, 2),
    POSIX_FUNC_ENUM(FILE_FCHMOD, FileFchmodWork, 2),
    POSIX_FUNC_ENUM(FILE_FCHMODAT, FileFchmodatWork, 4),
    POSIX_FUNC_ENUM(FILE_STAT, FileStatWork, 2),
    POSIX_FUNC_ENUM(FILE_FSTAT, FileFstatWork, 2),
    POSIX_FUNC_ENUM(FILE_STATFS, FileStatfsWork, 2),
    POSIX_FUNC_ENUM(FILE_FSTATFS, FileFstatfsWork, 2),
    POSIX_FUNC_ENUM(FILE_LSTAT, FileLstatWork, 2),
    POSIX_FUNC_ENUM(FILE_FSTATAT, FileFstatatWork, 4),
    POSIX_FUNC_ENUM(FILE_SYMLINK, FileSymlinkWork, 2),
    POSIX_FUNC_ENUM(FILE_SYMLINKAT, FileSymlinkatWork, 3),
    POSIX_FUNC_ENUM(FILE_READLINK, FileReadlinkWork, 3),
    POSIX_FUNC_ENUM(FILE_READLINKAT, FileReadlinkatWork, 4),
    POSIX_FUNC_ENUM(FILE_FSYNC, FileFsyncWork, 1),
    POSIX_FUNC_ENUM(FILE_TRUNCATE, FileTruncateWork, 2),
    POSIX_FUNC_ENUM(FILE_FTRUNCATE, FileFtruncateWork, 2),
    POSIX_FUNC_ENUM(FILE_RENAME, FileRenameWork, 2),
    POSIX_FUNC_ENUM(FILE_RENAMEAT, FileRenameatWork, 4),
    POSIX_FUNC_ENUM(FILE_DUP2, FileDup2Work, 2),
    POSIX_FUNC_ENUM(FILE_MKDIR, FileMkdirWork, 2),
    POSIX_FUNC_ENUM(FILE_UMASK, FileUmaskWork, 1),
    POSIX_FUNC_ENUM(FILE_UNLINK, FileUnlinkWork, 1),
    POSIX_FUNC_ENUM(FILE_FCNTL, FileFcntlWork, 3),
    POSIX_FUNC_ENUM(FILE_MMAP_UTIL, FileMmapUtilWork, 3),
    POSIX_FUNC_ENUM(FILE_MSYNC_UTIL, FileMsyncUtilWork, 2),
    POSIX_FUNC_ENUM(FILE_SENDFILE, FileSendfileWork, 4),
    POSIX_FUNC_ENUM(FILE_REMOVE, FileRemoveWork, 1),
    POSIX_FUNC_ENUM(FILE_RMDIR, FileRmdirWork, 1),
    POSIX_FUNC_ENUM(FILE_DUP, FileDupWork, 1),
    POSIX_FUNC_ENUM(FILE_REALPATH, FileRealpath, 2),
};

POSIX_FUNCS_IMPL(POSIX_FILE, g_funcs)
