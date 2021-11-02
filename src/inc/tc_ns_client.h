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

#ifdef SECURITY_AUTH_ENHANCE
#define TOKEN_SAVE_LEN 24                /* 24byte */
#endif

#define UUID_SIZE      16

#define TC_NS_CLIENT_IOC_MAGIC 't'
#define TC_NS_CLIENT_DEV       "tc_ns_client"
#define TC_NS_CLIENT_DEV_NAME  "/dev/tc_ns_client"

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
#ifdef SECURITY_AUTH_ENHANCE
    union {
        struct {
            unsigned int teec_token_addr;
            unsigned int teec_token_h_addr;
        } token_addr;
        void *teec_token;
    } token;
    unsigned int token_len;
#endif
    unsigned int callingPid;
    unsigned int file_size;
    union {
        char *file_buffer;
        unsigned long long file_addr;
    };
} TC_NS_ClientContext;

typedef struct {
    uint32_t seconds;
    uint32_t millis;
} TC_NS_Time;

enum SecFileType {
    LOAD_TA = 0,
    LOAD_SERVICE,
    LOAD_LIB,
    LOAD_DYNAMIC_DRV,
};

struct SecLoadIoctlStruct {
    enum SecFileType fileType;
    TEEC_UUID uuid;
    uint32_t fileSize;
    union {
        char *fileBuffer;
        unsigned long long fileAddr;
    };
};

struct AgentIoctlArgs {
    uint32_t id;
    uint32_t bufferSize;
    union {
        void *buffer;
        unsigned long long addr;
    };
};

struct TC_NS_ClientCrl {
    union {
        uint8_t *buffer;
        unsigned long long addr;
    };
    uint32_t size;
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
#define TC_NS_CLIENT_IOCTL_UPDATE_TA_CRL                  _IOWR(TC_NS_CLIENT_IOC_MAGIC, 22, struct TC_NS_ClientCrl)

TEEC_Result TEEC_CheckOperation(const TEEC_Operation *operation);
#endif
