/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2012-2023. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef _TC_NS_CLIENT_H_
#define _TC_NS_CLIENT_H_
#include "tee_client_type.h"
#define TC_DEBUG

#define INVALID_TYPE         0x00
#define TEECD_CONNECT        0x01
#ifndef ZERO_SIZE_PTR
#define ZERO_SIZE_PTR       ((void *)16)
#define ZERO_OR_NULL_PTR(x) ((unsigned long)(x) <= (unsigned long)ZERO_SIZE_PTR)
#endif

#define UUID_SIZE      16

#define TC_NS_CLIENT_IOC_MAGIC 't'
#define TC_NS_CLIENT_DEV       "tc_ns_client"
#define TC_NS_CLIENT_DEV_NAME  "/dev/tc_ns_client"
#define TC_TEECD_PRIVATE_DEV_NAME  "/dev/tc_private"
#define TC_NS_CVM_DEV_NAME  "/dev/tc_ns_cvm"

enum ConnectCmd {
    GET_FD,
    GET_TEEVERSION,
    SET_SYS_XML,
    GET_TEECD_VERSION,
};

typedef struct {
    unsigned int method;
    unsigned int mdata;
} TC_NS_ClientLogin;

typedef union {
    struct {
        unsigned int buffer;
        unsigned int buffer_h_addr;
        unsigned int offset;
        unsigned int h_offset;
        unsigned int size_addr;
        unsigned int size_h_addr;
    } memref;
    struct {
        unsigned int a_addr;
        unsigned int a_h_addr;
        unsigned int b_addr;
        unsigned int b_h_addr;
    } value;
} TC_NS_ClientParam;

typedef struct {
    unsigned int code;
    unsigned int origin;
} TC_NS_ClientReturn;

typedef struct {
    unsigned char uuid[UUID_SIZE];
    unsigned int session_id;
    unsigned int cmd_id;
    TC_NS_ClientReturn returns;
    TC_NS_ClientLogin login;
    TC_NS_ClientParam params[TEEC_PARAM_NUM];
    unsigned int paramTypes;
    bool started;
    unsigned int callingPid;
    unsigned int file_size;
    union {
        char *file_buffer;
        struct {
            uint32_t file_addr;
            uint32_t file_h_addr;
        } memref;
    };
} TC_NS_ClientContext;

typedef struct {
    uint32_t seconds;
    uint32_t millis;
} TC_NS_Time;

typedef struct {
    uint16_t tzdriver_version_major;
    uint16_t tzdriver_version_minor;
    uint32_t reserved[15];
} TC_NS_TEE_Info;

enum SecFileType {
    LOAD_TA = 0,
    LOAD_SERVICE,
    LOAD_LIB,
    LOAD_DYNAMIC_DRV,
    LOAD_PATCH,
    LOAD_TYPE_MAX
};

struct SecFileInfo {
    enum SecFileType fileType;
    uint32_t fileSize;
    int32_t secLoadErr;
};

struct SecLoadIoctlStruct {
    struct SecFileInfo secFileInfo;
    TEEC_UUID uuid;
    union {
        char *fileBuffer;
        struct {
            uint32_t file_addr;
            uint32_t file_h_addr;
        } memref;
    };
}__attribute__((packed));

#ifdef CROSS_DOMAIN_PERF
enum PosixProxyShmType {
    CTRL_TASKLET_BUFF = 1,
    DATA_TASKLET_BUFF
};

struct PosixProxyIoctlArgs {
    enum PosixProxyShmType shmType;
    uint32_t bufferSize;
    union {
        void *buffer;
        unsigned long long addr;
    };
};
#endif

struct AgentIoctlArgs {
    uint32_t id;
    uint32_t bufferSize;
    union {
        void *buffer;
        unsigned long long addr;
    };
};

#define TC_NS_CLIENT_IOCTL_SES_OPEN_REQ                   _IOW(TC_NS_CLIENT_IOC_MAGIC, 1, TC_NS_ClientContext)
#define TC_NS_CLIENT_IOCTL_SES_CLOSE_REQ                  _IOWR(TC_NS_CLIENT_IOC_MAGIC, 2, TC_NS_ClientContext)
#define TC_NS_CLIENT_IOCTL_SEND_CMD_REQ                   _IOWR(TC_NS_CLIENT_IOC_MAGIC, 3, TC_NS_ClientContext)
#define TC_NS_CLIENT_IOCTL_SHRD_MEM_RELEASE               _IOWR(TC_NS_CLIENT_IOC_MAGIC, 4, unsigned int)
#define TC_NS_CLIENT_IOCTL_WAIT_EVENT                     _IOWR(TC_NS_CLIENT_IOC_MAGIC, 5, unsigned int)
#define TC_NS_CLIENT_IOCTL_SEND_EVENT_RESPONSE            _IOWR(TC_NS_CLIENT_IOC_MAGIC, 6, unsigned int)
#define TC_NS_CLIENT_IOCTL_REGISTER_AGENT                 _IOWR(TC_NS_CLIENT_IOC_MAGIC, 7, struct AgentIoctlArgs)
#define TC_NS_CLIENT_IOCTL_UNREGISTER_AGENT               _IOWR(TC_NS_CLIENT_IOC_MAGIC, 8, unsigned int)
#define TC_NS_CLIENT_IOCTL_LOAD_APP_REQ                   _IOWR(TC_NS_CLIENT_IOC_MAGIC, 9, struct SecLoadIoctlStruct)
#define TC_NS_CLIENT_IOCTL_NEED_LOAD_APP                  _IOWR(TC_NS_CLIENT_IOC_MAGIC, 10, TC_NS_ClientContext)
#define TC_NS_CLIENT_IOCTL_LOAD_APP_EXCEPT                _IOWR(TC_NS_CLIENT_IOC_MAGIC, 11, unsigned int)
#define TC_NS_CLIENT_IOCTL_CANCEL_CMD_REQ                 _IOWR(TC_NS_CLIENT_IOC_MAGIC, 13, TC_NS_ClientContext)
#define TC_NS_CLIENT_IOCTL_LOGIN                          _IOWR(TC_NS_CLIENT_IOC_MAGIC, 14, int)
#define TC_NS_CLIENT_IOCTL_TST_CMD_REQ                    _IOWR(TC_NS_CLIENT_IOC_MAGIC, 15, int)
#define TC_NS_CLIENT_IOCTL_TUI_EVENT                      _IOWR(TC_NS_CLIENT_IOC_MAGIC, 16, int)
#define TC_NS_CLIENT_IOCTL_SYC_SYS_TIME                   _IOWR(TC_NS_CLIENT_IOC_MAGIC, 17, TC_NS_Time)
#define TC_NS_CLIENT_IOCTL_SET_NATIVE_IDENTITY            _IOWR(TC_NS_CLIENT_IOC_MAGIC, 18, int)
#define TC_NS_CLIENT_IOCTL_LOAD_TTF_FILE_AND_NOTCH_HEIGHT _IOWR(TC_NS_CLIENT_IOC_MAGIC, 19, unsigned int)
#define TC_NS_CLIENT_IOCTL_LATEINIT                       _IOWR(TC_NS_CLIENT_IOC_MAGIC, 20, unsigned int)
#define TC_NS_CLIENT_IOCTL_GET_TEE_VERSION                _IOWR(TC_NS_CLIENT_IOC_MAGIC, 21, unsigned int)
#ifdef CONFIG_CMS_SIGNATURE
#define TC_NS_CLIENT_IOCTL_UPDATE_TA_CRL                  _IOWR(TC_NS_CLIENT_IOC_MAGIC, 22, struct TC_NS_ClientCrl)
#endif
#ifdef CONFIG_TEE_TELEPORT_SUPPORT
#define TC_NS_CLIENT_IOCTL_PORTAL_REGISTER                _IOWR(TC_NS_CLIENT_IOC_MAGIC, 24, struct AgentIoctlArgs)
#define TC_NS_CLIENT_IOCTL_PORTAL_WORK                    _IOWR(TC_NS_CLIENT_IOC_MAGIC, 25, struct AgentIoctlArgs)

#ifdef CROSS_DOMAIN_PERF
#define TC_NS_CLIENT_IOCTL_POSIX_PROXY_REGISTER_TASKLET \
	_IOWR(TC_NS_CLIENT_IOC_MAGIC, 27, struct PosixProxyIoctlArgs)
#endif

#endif
#define TC_NS_CLIENT_IOCTL_GET_TEE_INFO                   _IOWR(TC_NS_CLIENT_IOC_MAGIC, 26, TC_NS_TEE_Info)
#define TC_NS_CLIENT_IOCTL_CHECK_CCOS                     _IOWR(TC_NS_CLIENT_IOC_MAGIC, 32, unsigned int)

TEEC_Result TEEC_CheckOperation(const TEEC_Operation *operation);
#endif
