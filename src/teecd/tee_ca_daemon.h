/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2012-2021. All rights reserved.
 * iTrustee licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef LIBTEEC_TEE_CA_DAEMON_H
#define LIBTEEC_TEE_CA_DAEMON_H
#include "tee_auth_common.h"
#include <sys/stat.h>
#include <sys/socket.h>

#define TC_NS_SOCKET_NAME        "#tc_ns_socket"
#define TC_NS_SOCKET_NAME_SYSTEM "#tc_ns_socket_system"

#define USER_DATA_DIR "/var/itrustee/sec_storage_data/"
#define SFS_PARTITION_PERSISTENT "var/itrustee/sec_storage_data/"
#define SFS_PARTITION_USER_SYMLINK "sec_storage_data_users/"

#define SEC_STORAGE_DATA_USERS  USER_DATA_DIR"sec_storage_data_users/"
#define SEC_STORAGE_DATA_USER_0 USER_DATA_DIR"sec_storage_data_users/0"
#define SEC_STORAGE_DATA_DIR    USER_DATA_DIR"sec_storage_data/"

#define TRANS_BUFF_SIZE (4 * 1024) /* agent transfer share buffer size */

#define ROOT_DIR "/"
#define SEC_STORAGE_ROOT_DIR      ROOT_DIR SFS_PARTITION_PERSISTENT

/* 0600 only root can read and write sec_storage folder */
#define ROOT_DIR_PERM                   (S_IRUSR | S_IWUSR)
#define SFS_PARTITION_TRANSIENT         "sec_storage_data/"
#define SFS_PARTITION_TRANSIENT_PRIVATE "sec_storage_data/_private"
#define SFS_PARTITION_TRANSIENT_PERSO   "sec_storage_data/_perso"

#define FILE_NAME_INVALID_STR "../" // file name path must not contain ../

#define SEC_STORAGE_DATA_CE         "/data/vendor_ce/"
#define SEC_STORAGE_DATA_ROOT_DIR   ROOT_DIR SFS_PARTITION_TRANSIENT
#define TEE_OBJECT_STORAGE_CE       0x80000002

typedef struct {
    int magic;
    int status;
    int userid;
    int reserved;
} RecvMsg;

#define MU_MSG_MAGIC 0xff00ff00

enum MuMsgStatus {
    MU_MSG_STAT_NEW_USER    = 0x01,
    MU_MSG_STAT_RM_USER     = 0x02,
    MU_MSG_STAT_SWITCH_USER = 0x03,
};

enum MuMsgUserid {
    MU_MSG_USERID_OWNER = 0x0,
    MU_MSG_USERID_MAX   = 0xFF,
};

enum ConnectCmd {
    GET_FD,
    GET_TEEVERSION,
    SET_SYS_XML,
};

void *CaServerWorkThread(void *dummy);
int GetTEEVersion(void);

#endif
