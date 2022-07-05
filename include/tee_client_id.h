/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2013-2022. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef _TEE_CLIENT_ID_H_
#define _TEE_CLIENT_ID_H_

#define TEE_SERVICE_STORAGE \
{ \
    0x02020202, \
    0x0202, \
    0x0202, \
    { \
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02 \
    } \
}

#define TEE_SERVICE_CRYPTO \
{ \
    0x04040404, \
    0x0404, \
    0x0404, \
    { \
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 \
    } \
}

#define TEE_SERVICE_EFUSE \
{ \
    0x05050505, \
    0x0505, \
    0x0505, \
    { \
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 \
    } \
}

#define TEE_SERVICE_HDCP \
{ \
    0x06060606, \
    0x0606, \
    0x0606, \
    { \
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06 \
    } \
}

#define TEE_SERVICE_KEYMASTER \
{ \
    0x07070707, \
    0x0707, \
    0x0707, \
    { \
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07 \
    } \
}

#define TEE_SERVICE_SECBOOT \
{ \
    0x08080808, \
    0x0808, \
    0x0808, \
    { \
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08 \
    } \
}

#define TEE_SERVICE_GATEKEEPER \
{ \
    0x0B0B0B0B, \
    0x0B0B, \
    0x0B0B, \
    { \
        0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B \
    } \
}

enum SVC_SECBOOT_CMD_ID {
    SECBOOT_CMD_ID_INVALID = 0x0,      /* Secboot Task invalid cmd */
    SECBOOT_CMD_ID_COPY_VRL,           /* Secboot Task copy VRL */
    SECBOOT_CMD_ID_COPY_DATA,          /* Secboot Task copy data */
    SECBOOT_CMD_ID_VERIFY_DATA,        /* Secboot Task verify data */
    SECBOOT_CMD_ID_RESET_IMAGE,        /* Secboot Task reset SoC */
    SECBOOT_CMD_ID_COPY_VRL_TYPE,      /* Secboot Task copy VRL and send SoC Type */
    SECBOOT_CMD_ID_COPY_DATA_TYPE,     /* Secboot Task copy data and send SoC Type */
    SECBOOT_CMD_ID_VERIFY_DATA_TYPE,   /* Secboot Task verify data and send SoC Type,
                                        * unreset SoC after verify success */
    SECBOOT_CMD_ID_VERIFY_DATA_TYPE_LOCAL, /* Secboot Task verify data and send SoC Type local,
                                            * unreset SoC after verify success */
};

/* img type supported by service secboot */
enum SVC_SECBOOT_IMG_TYPE {
    MODEM,
    HIFI,
    DSP,
    SOC_MAX
};

enum SVC_STORAGE_CMD_ID {
    STORAGE_CMD_ID_INVALID = 0x10,        /* invalid cmd */
    STORAGE_CMD_ID_OPEN,                  /* open file */
    STORAGE_CMD_ID_CLOSE,                 /* close file */
    STORAGE_CMD_ID_CLOSEALL,              /* close all files */
    STORAGE_CMD_ID_READ,                  /* read file */
    STORAGE_CMD_ID_WRITE,                 /* write file */
    STORAGE_CMD_ID_SEEK,                  /* seek file */
    STORAGE_CMD_ID_TELL,                  /* reset file position */
    STORAGE_CMD_ID_TRUNCATE,              /* truncate file */
    STORAGE_CMD_ID_REMOVE,                /* remove file */
    STORAGE_CMD_ID_FINFO,                 /* get file info */
    STORAGE_CMD_ID_FSYNC,                 /* sync file to storage device */
    STORAGE_CMD_ID_UNKNOWN = 0x7FFFFFFE,  /* unknown cmd */
    STORAGE_CMD_ID_MAX = 0x7FFFFFFF,
};

enum SVC_CRYPT_CMD_ID {
    CRYPT_CMD_ID_INVALID = 0x10,
    CRYPT_CMD_ID_ENCRYPT,
    CRYPT_CMD_ID_DECRYPT,
    CRYPT_CMD_ID_MD5,
    CRYPT_CMD_ID_SHA1,
    CRYPT_CMD_ID_SHA224,
    CRYPT_CMD_ID_SHA256,
    CRYPT_CMD_ID_SHA384,
    CRYPT_CMD_ID_SHA512,
    CRYPT_CMD_ID_HMAC_MD5,
    CRYPT_CMD_ID_HMAC_SHA1,
    CRYPT_CMD_ID_HMAC_SHA224,
    CRYPT_CMD_ID_HMAC_SHA256,
    CRYPT_CMD_ID_HMAC_SHA384,
    CRYPT_CMD_ID_HMAC_SHA512,
    CRYPT_CMD_ID_CIPHER_AES_CBC,
    CRYPT_CMD_ID_CIPHER_AES_CBC_CTS,
    CRYPT_CMD_ID_CIPHER_AES_ECB,
    CRYPT_CMD_ID_CIPHER_AES_CTR,
    CRYPT_CMD_ID_CIPHER_AES_CBC_MAC,
    CRYPT_CMD_ID_CIPHER_AES_XCBC_MAC,
    CRYPT_CMD_ID_CIPHER_AES_CMAC,
    CRYPT_CMD_ID_CIPHER_AES_CCM,
    CRYPT_CMD_ID_CIPHER_AES_XTS,
    CRYPT_CMD_ID_CIPHER_DES_ECB,
    CRYPT_CMD_ID_CIPHER_DES_CBC,
    CRYPT_CMD_ID_CIPHER_DES3_ECB,
    CRYPT_CMD_ID_CIPHER_DES3_CBC,
    CRYPT_CMD_ID_CIPHER_RND,
    CRYPT_CMD_ID_CIPHER_DK,
    CRYPT_CMD_ID_RSAES_PKCS1_V1_5,
    CRYPT_CMD_ID_RSAES_PKCS1_OAEP_MGF1_SHA1,
    CRYPT_CMD_ID_RSAES_PKCS1_OAEP_MGF1_SHA224,
    CRYPT_CMD_ID_RSAES_PKCS1_OAEP_MGF1_SHA256,
    CRYPT_CMD_ID_RSAES_PKCS1_OAEP_MGF1_SHA384,
    CRYPT_CMD_ID_RSAES_PKCS1_OAEP_MGF1_SHA512,
    CRYPT_CMD_ID_RSA_NOPAD,
    CRYPT_CMD_ID_RSASSA_PKCS1_V1_5_MD5,
    CRYPT_CMD_ID_RSASSA_PKCS1_V1_5_SHA1,
    CRYPT_CMD_ID_RSASSA_PKCS1_V1_5_SHA224,
    CRYPT_CMD_ID_RSASSA_PKCS1_V1_5_SHA256,
    CRYPT_CMD_ID_RSASSA_PKCS1_V1_5_SHA384,
    CRYPT_CMD_ID_RSASSA_PKCS1_V1_5_SHA512,
    CRYPT_CMD_ID_RSASSA_PKCS1_PSS_MGF1_SHA1,
    CRYPT_CMD_ID_RSASSA_PKCS1_PSS_MGF1_SHA224,
    CRYPT_CMD_ID_RSASSA_PKCS1_PSS_MGF1_SHA256,
    CRYPT_CMD_ID_RSASSA_PKCS1_PSS_MGF1_SHA384,
    CRYPT_CMD_ID_RSASSA_PKCS1_PSS_MGF1_SHA512,
    CRYPT_CMD_ID_DSA_SHA1,
    CRYPT_CMD_ID_UNKNOWN = 0x7FFFFFFE,
    CRYPT_CMD_ID_MAX = 0x7FFFFFFF
};

enum SVC_KEYMASTER_CMD_ID {
    KM_CMD_ID_INVALID = 0x0,
    KM_CMD_ID_CONFIGURE,
    KM_CMD_ID_GENERATE_KEY,
    KM_CMD_ID_GET_KEY_CHARACTER,
    KM_CMD_ID_IMPORT_KEY,
    KM_CMD_ID_EXPORT_KEY,
    KM_CMD_ID_ATTEST_KEY,
    KM_CMD_ID_UPGRADE,
    KM_CMD_ID_BEGIN,
    KM_CMD_ID_UPDATE,
    KM_CMD_ID_FINISH,
    KM_CMD_ID_ABORT,
    KM_CMD_ID_STORE_KB,
    KM_CMD_ID_VERIFY_KB,
    KM_CMD_ID_KB_EIMA_POLICY_SET = 0x1E,
    KM_CMD_ID_DELETE_KEY = 0x1F,
    KM_CMD_ID_QUERY_ATTESTATION_CERTS = 0x22,
    KM_CMD_ID_DELETE_ALL_ATTESTATION_CERTS = 0x23,
    KM_CMD_ID_QUERY_IDENTIFIERS = 0x24,
    KM_CMD_ID_DELETE_ALL_IDENTIFIERS = 0x25,
    KM_CMD_ID_STORE_KB_SP = 0x26,
};

enum SVC_GATEKEEPER_CMD_ID {
    GK_CMD_ID_INVALID = 0x0,
    GK_CMD_ID_ENROLL,
    GK_CMD_ID_VERIFY,
    GK_CMD_ID_DEL_USER,
    GK_CMD_ID_GET_RETRY_TIMES,
};

/* poll event type from normal to secure */
enum TUI_POLL_TYPE {
    TUI_POLL_CFG_OK,
    TUI_POLL_CFG_FAIL,
    TUI_POLL_TP,
    TUI_POLL_TICK,
    TUI_POLL_DELAYED_WORK,
    TUI_POLL_PAUSE_TUI,
    TUI_POLL_RESUME_TUI,
    /* For some reasons, we need a method to terminate TUI from no-secure
     * OS, for example the TUI CA maybe killed.
     */
    TUI_POLL_CANCEL,
    TUI_POLL_HANDLE_TUI,
    TUI_POLL_NAVI_H_TO_S, /* for navigator hide and show */
    TUI_POLL_NAVI_S_TO_H,
    TUI_POLL_SHS_0_TO_1,  /* for single hand mode switch */
    TUI_POLL_SHS_0_TO_2,
    TUI_POLL_SHS_1_TO_0,
    TUI_POLL_SHS_2_TO_0,
    TUI_POLL_ROTATION_0,  /* for retation switch */
    TUI_POLL_ROTATION_90,
    TUI_POLL_ROTATION_180,
    TUI_POLL_ROTATION_270,
    TUI_POLL_KEYBOARDTYPE_0,
    TUI_POLL_KEYBOARDTYPE_3,
    TUI_POLL_SEMITRANS,
    TUI_POLL_MAX
};
#endif
