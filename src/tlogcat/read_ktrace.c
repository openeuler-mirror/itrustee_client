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

#include "read_ktrace.h"
#include <stdint.h>
#include <stdio.h>

#ifdef TEE_KTRACE_DUMP

static const char *g_smcOriToStr[] = {
    [SMC_ORI_IN]  = "IN",
    [SMC_ORI_OUT] = "OUT",
};

/* global kernel event id */
static struct KtracePage *g_ktraceBufInfo[CONFIG_MAX_NUM_NODES] = { NULL };
static int g_kevInited[CONFIG_MAX_NUM_NODES] = { 0 };
static int g_maxKtraceCnt = 0;

/*
 * print information for schedule.
 */
static void EvPrintKsched(const struct Kevent *ev, int cpuId)
{
    uint32_t fPid = (uint32_t)ev->infoSched.fromPidTid;
    uint32_t tPid = (uint32_t)ev->infoSched.toPidTid;
    printf("[cpu %d] [id %d] [SCH] from: %u+%u to: %u+%u\n", cpuId, (int)ev->id, TCB_PID(fPid), TCB_TID(fPid),
           TCB_PID(tPid), TCB_TID(tPid));
}

/*
 * print information for smc.
 */
static void EvPrintKsmc(const struct Kevent *ev, int cpuId)
{
    uint32_t rea = SMC_REASON(ev->infoSmc.smcCmdOriReason);
    uint32_t ori = SMC_ORI(ev->infoSmc.smcCmdOriReason);
    uint32_t cmd = SMC_CMD(ev->infoSmc.smcCmdOriReason);

    printf("[cpu %d] [id %d] [SMC] cmd: 0x%x reason: %u  orient: %s\n", cpuId, (int)ev->id, cmd, rea,
           g_smcOriToStr[ori & 1]);
}

/*
 * print information when IRQ happened in REE
 */
static void EvPrintKirq(const struct Kevent *ev, int cpuId)
{
    uint32_t pidTid = (uint32_t)ev->infoIrq.interruptedPidTid;
    printf("[cpu %d] [id %d] [IRQ] irqNo: 0x%x interrupted: %u+%u\n", cpuId, (int)ev->id,
           (uint32_t)ev->infoIrq.irqNo, TCB_PID(pidTid), TCB_TID(pidTid));
}

/*
 * print information when IRQ happened in TEE
 */
static void EvPrintKsirq(const struct Kevent *ev, int cpuId)
{
    printf("[cpu %d] [id %d] [FIQ] irqNo: %u state: 0x%x\n", cpuId, (int)ev->id, (uint32_t)ev->infoIrq.irqNo,
           (uint32_t)ev->infoIrq.irqState);
}

/*
 * print different information for different type.
 */
static void EvPrintKev(const struct Kevent *ev, int cpuId)
{
    switch (ev->type) {
        case EV_KSCHED:
            EvPrintKsched(ev, cpuId);
            break;
        case EV_KSMC:
            EvPrintKsmc(ev, cpuId);
            break;
        case EV_KIRQ:
            EvPrintKirq(ev, cpuId);
            break;
        case EV_KSIRQ:
            EvPrintKsirq(ev, cpuId);
            break;
        default:
            printf("invalid event type!\n");
            break;
    }
}

void EvKprint(int cpuId)
{
    uint32_t i;
    uint32_t idx;

    if (cpuId < 0 || cpuId >= CONFIG_MAX_NUM_NODES) {
        printf("invalid cpu id!\n");
        return;
    }

    if (!g_kevInited[cpuId]) {
        printf("%d cpu kernel event is not inited\n", cpuId);
        return;
    }

    i   = (uint32_t)((g_ktraceBufInfo[cpuId]->header.head + g_maxKtraceCnt - 1) % g_maxKtraceCnt);
    idx = (uint32_t)((g_ktraceBufInfo[cpuId]->header.idx + g_maxKtraceCnt - 1) % g_maxKtraceCnt);

    struct Kevent *ev = NULL;
    while (i != idx) {
        ev = &(g_ktraceBufInfo[cpuId]->events[i]);
        EvPrintKev(ev, cpuId);
        i = (i + 1) % (uint32_t)g_maxKtraceCnt;
    }
    ev = &(g_ktraceBufInfo[cpuId]->events[i]);
    EvPrintKev(ev, cpuId);
}

/*
 * print tracing information for every CPU.
 */
void EvPrintKall(void)
{
    int i;
    for (i = 0; i < CONFIG_MAX_NUM_NODES; i++) {
        printf("======= CPU [%d] tracing ========\n", i);
        EvKprint(i);
        printf("======= DONE CPU [%d] tracing ========\n", i);
    }
}

void InitKevData(char *buffer, uint32_t sz)
{
    int i = 0;
    if (buffer == NULL || sz != KTRACE_BUFFSIZE_ALL) {
        printf("data size must be SIZE_PER_NODE(%d)*NUM_NODES(%d)=%d\n", KTRACE_BUFFSIZE_PER_CPU, CONFIG_MAX_NUM_NODES,
               KTRACE_BUFFSIZE_ALL);
        return;
    }

    g_maxKtraceCnt = (KTRACE_BUFFSIZE_PER_CPU - sizeof(struct TraceHeader)) / sizeof(struct Kevent);
    for (; i < CONFIG_MAX_NUM_NODES; ++i) {
        g_ktraceBufInfo[i] = (struct KtracePage *)(buffer + (i * KTRACE_BUFFSIZE_PER_CPU));
        g_kevInited[i] = 1;
    }
}
#endif
