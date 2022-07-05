/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef CAP_BIT_H
#define CAP_BIT_H

#ifndef CONFIG_BIG_ENDIAN
#define CAP_BITF1(f)          f;
#define CAP_BITF2(f, ...)     f; CAP_BITF1(__VA_ARGS__)
#define CAP_BITF3(f, ...)     f; CAP_BITF2(__VA_ARGS__)
#define CAP_BITF4(f, ...)     f; CAP_BITF3(__VA_ARGS__)
#define CAP_BITF5(f, ...)     f; CAP_BITF4(__VA_ARGS__)
#define CAP_BITF6(f, ...)     f; CAP_BITF5(__VA_ARGS__)
#define CAP_BITF7(f, ...)     f; CAP_BITF6(__VA_ARGS__)
#define CAP_BITF8(f, ...)     f; CAP_BITF7(__VA_ARGS__)
#define CAP_BITF9(f, ...)     f; CAP_BITF8(__VA_ARGS__)
#define CAP_BITF10(f, ...)    f; CAP_BITF9(__VA_ARGS__)
#define CAP_BITF11(f, ...)    f; CAP_BITF10(__VA_ARGS__)
#define CAP_BITF12(f, ...)    f; CAP_BITF11(__VA_ARGS__)
#define CAP_BITF13(f, ...)    f; CAP_BITF12(__VA_ARGS__)
#define CAP_BITF14(f, ...)    f; CAP_BITF13(__VA_ARGS__)
#define CAP_BITF15(f, ...)    f; CAP_BITF14(__VA_ARGS__)
#define CAP_BITF16(f, ...)    f; CAP_BITF15(__VA_ARGS__)
#define CAP_BITF17(f, ...)    f; CAP_BITF16(__VA_ARGS__)
#define CAP_BITF18(f, ...)    f; CAP_BITF17(__VA_ARGS__)
#define CAP_BITF19(f, ...)    f; CAP_BITF18(__VA_ARGS__)
#define CAP_BITF20(f, ...)    f; CAP_BITF19(__VA_ARGS__)
#define CAP_BITF21(f, ...)    f; CAP_BITF20(__VA_ARGS__)
#define CAP_BITF22(f, ...)    f; CAP_BITF21(__VA_ARGS__)
#define CAP_BITF23(f, ...)    f; CAP_BITF22(__VA_ARGS__)
#define CAP_BITF24(f, ...)    f; CAP_BITF23(__VA_ARGS__)
#define CAP_BITF25(f, ...)    f; CAP_BITF24(__VA_ARGS__)
#else
#define CAP_BITF1(f)          f;
#define CAP_BITF2(f, ...)     CAP_BITF1(__VA_ARGS__) f;
#define CAP_BITF3(f, ...)     CAP_BITF2(__VA_ARGS__) f;
#define CAP_BITF4(f, ...)     CAP_BITF3(__VA_ARGS__) f;
#define CAP_BITF5(f, ...)     CAP_BITF4(__VA_ARGS__) f;
#define CAP_BITF6(f, ...)     CAP_BITF5(__VA_ARGS__) f;
#define CAP_BITF7(f, ...)     CAP_BITF6(__VA_ARGS__) f;
#define CAP_BITF8(f, ...)     CAP_BITF7(__VA_ARGS__) f;
#define CAP_BITF9(f, ...)     CAP_BITF8(__VA_ARGS__) f;
#define CAP_BITF10(f, ...)    CAP_BITF9(__VA_ARGS__) f;
#define CAP_BITF11(f, ...)    CAP_BITF10(__VA_ARGS__) f;
#define CAP_BITF12(f, ...)    CAP_BITF11(__VA_ARGS__) f;
#define CAP_BITF13(f, ...)    CAP_BITF12(__VA_ARGS__) f;
#define CAP_BITF14(f, ...)    CAP_BITF13(__VA_ARGS__) f;
#define CAP_BITF15(f, ...)    CAP_BITF14(__VA_ARGS__) f;
#define CAP_BITF16(f, ...)    CAP_BITF15(__VA_ARGS__) f;
#define CAP_BITF17(f, ...)    CAP_BITF16(__VA_ARGS__) f;
#define CAP_BITF18(f, ...)    CAP_BITF17(__VA_ARGS__) f;
#define CAP_BITF19(f, ...)    CAP_BITF18(__VA_ARGS__) f;
#define CAP_BITF20(f, ...)    CAP_BITF19(__VA_ARGS__) f;
#define CAP_BITF21(f, ...)    CAP_BITF20(__VA_ARGS__) f;
#define CAP_BITF22(f, ...)    CAP_BITF21(__VA_ARGS__) f;
#define CAP_BITF23(f, ...)    CAP_BITF22(__VA_ARGS__) f;
#define CAP_BITF24(f, ...)    CAP_BITF23(__VA_ARGS__) f;
#define CAP_BITF25(f, ...)    CAP_BITF24(__VA_ARGS__) f;
#endif

#define __CAP_BITFN(n, ...) CAP_BITF##n(__VA_ARGS__)
#define CAP_BITFN(n, ...) __CAP_BITFN(n, __VA_ARGS__)
#define __VA_ARGS_NUM(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, n, ...) n
#define VA_ARGS_NUM(...) __VA_ARGS_NUM(__VA_ARGS__, 11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define CAP_BITF(...) CAP_BITFN(VA_ARGS_NUM(__VA_ARGS__), __VA_ARGS__)

#endif
