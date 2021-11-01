/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * iTrustee licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef READ_KTRACE_H
#define READ_KTRACE_H

#include <stdint.h>

/* additional */
#define PAGE_SIZE            4096
#define CONFIG_MAX_NUM_NODES 8
#define PACKED               __attribute__((packed))

/* Kernel event tracing :sched,irq and smc */
#define SMC_ORI_IN  0
#define SMC_ORI_OUT 1

enum EvKtype { EV_MIN, EV_KSCHED, EV_KSMC, EV_KIRQ, EV_KSIRQ, EV_MAX };

struct EvKinfoSched {
    uint16_t fromPidTid;
    uint16_t toPidTid;
} PACKED;

struct EvKinfoSmc {
    uint32_t smcCmdOriReason;
} PACKED;

struct EvKinfoIrq {
    uint16_t irqNo;
    union {
        uint16_t irqState;
        uint16_t interruptedPidTid;
    };
} PACKED;

/* 32bit plus 32bit is 8B */
struct Kevent {
    uint32_t id : 24;
    uint32_t type : 8;
    union {
        struct EvKinfoSched infoSched;
        struct EvKinfoSmc infoSmc;
        struct EvKinfoIrq infoIrq;
    };
} PACKED;

struct TraceHeader {
    int32_t idx;
    int32_t head;
};

#define KTRACE_BUFFSIZE_ALL     (PAGE_SIZE * 4)
#define MAX_KTRACE_ID           0x00ffffff
#define KTRACE_BUFFSIZE_PER_CPU (KTRACE_BUFFSIZE_ALL / CONFIG_MAX_NUM_NODES)

struct KtracePage {
    struct TraceHeader header;
    struct Kevent events[0];
};

#define SMC_ORI(smcCmdOriReason)    (((smcCmdOriReason) >> 16) & 0xf)
#define SMC_REASON(smcCmdOriReason) (((smcCmdOriReason) >> 20) & 0xf)
#define SMC_CMD(smcCmdOriReason)    ((smcCmdOriReason) & 0xff00ffff)

#define TCB_TID(pid_tid) ((pid_tid) & 0xff)
#define TCB_PID(pid_tid) (((pid_tid) >> 8) & 0xff)

#ifdef TEE_KTRACE_DUMP
void EvKprint(int cpuId);
void EvPrintKall(void);
void InitKevData(char *buffer, uint32_t sz);
#endif

#endif /* READ_KTRACE_H */
