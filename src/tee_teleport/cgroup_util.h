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

#ifndef CGROUP_UTIL_H
#define CGROUP_UTIL_H

#define ONLINE_CPU_MAX_LEN 1024 /* 256 cores as maximum specification */
#define PARAM_LEN          64

struct TeePortalRConfigType {
    long vmid;
    char cpus[PARAM_LEN];
    char memSize[PARAM_LEN];
    char cpuset[PARAM_LEN];
    char diskSize[PARAM_LEN];
    char onlineCpus[ONLINE_CPU_MAX_LEN];
};

#endif
