/*******************************************************
 * Copyright (c) 2019, ArrayFire
 * All rights reserved.
 *
 * This file is distributed under 3-clause BSD license.
 * The complete license agreement can be obtained at:
 * http://arrayfire.com/licenses/BSD-3-Clause
 ********************************************************/

#pragma once

#if defined(__NVCC__) || defined(__CUDACC_RTC__)

// MSVC sets __cplusplus to 199711L for all versions unless you specify
// the new \Zc:__cplusplus flag in Visual Studio 2017. This is not possible
// in older versions of MSVC so we updated it here for the cuda_fp16 header
// because otherwise it does not define the default constructor for __half
// as default and that prevents the __half struct to be used in a constexpr
// expression
#if defined(_MSC_VER) && __cplusplus == 199711L
#undef __cplusplus
#define __cplusplus 201402L
#define AF_CPLUSPLUS_CHANGED
#endif

#include <cuda_fp16.h>

#ifdef AF_CPLUSPLUS_CHANGED
#undef __cplusplus
#undef AF_CPLUSPLUS_CHANGED
#define __cplusplus 199711L
#endif
#endif

#ifdef AF_ONEAPI
#include <sycl/sycl.hpp>
#endif

#include <backend.hpp>

#ifdef __CUDACC_RTC__

#if defined(__cpp_if_constexpr) || __cplusplus >= 201606L
#define AF_IF_CONSTEXPR if constexpr
#else
#define AF_IF_CONSTEXPR if
#endif

namespace std {
enum float_round_style {
    round_indeterminate       = -1,
    round_toward_zero         = 0,
    round_to_nearest          = 1,
    round_toward_infinity     = 2,
    round_toward_neg_infinity = 3
};

template<bool B, class T = void>
struct enable_if {};

template<class T>
struct enable_if<true, T> {
    typedef T type;
};

template<bool B, class T = void>
using enable_if_t = typename enable_if<B, T>::type;

template<class T, class U>
struct is_same {
    static constexpr bool value = false;
};

template<class T>
struct is_same<T, T> {
    static constexpr bool value = true;
};

template<class T, class U>
constexpr bool is_same_v = is_same<T, U>::value;

}  //  namespace std

using uint16_t = unsigned short;
// we do not include the af/compilers header in nvrtc compilations so
// we are defining the AF_CONSTEXPR expression here
#define AF_CONSTEXPR constexpr
#else
#include <af/compilers.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <string>
#include <type_traits>

#include <limits>

#endif

namespace arrayfire {
namespace common {

#if defined(__CUDA_ARCH__)
using native_half_t = __half;
#elif defined(AF_ONEAPI)
using native_half_t = sycl::half;
#else
using native_half_t = uint16_t;
#endif

#ifdef __CUDACC_RTC__
template<std::float_round_style R = std::round_to_nearest>
AF_CONSTEXPR __DH__ native_half_t float2half_impl(float value) {
    return __float2half_rn(value);
}

template<std::float_round_style R = std::round_to_nearest>
AF_CONSTEXPR __DH__ native_half_t float2half_impl(double value) {
    return __float2half_rn(value);
}

AF_CONSTEXPR
__DH__ inline float half2float_impl(native_half_t value) noexcept {
    return __half2float(value);
}

template<typename T>
AF_CONSTEXPR __DH__ native_half_t int2half_impl(T value) noexcept;

template<>
AF_CONSTEXPR __DH__ native_half_t int2half_impl(int value) noexcept {
    return __int2half_rn(value);
}

template<>
AF_CONSTEXPR __DH__ native_half_t int2half_impl(unsigned value) noexcept {
    return __uint2half_rn(value);
}

template<>
AF_CONSTEXPR __DH__ native_half_t int2half_impl(long long value) noexcept {
    return __ll2half_rn(value);
}

template<>
AF_CONSTEXPR __DH__ native_half_t
int2half_impl(unsigned long long value) noexcept {
    return __ull2half_rn(value);
}

template<>
AF_CONSTEXPR __DH__ native_half_t int2half_impl(short value) noexcept {
    return __short2half_rn(value);
}
template<>
AF_CONSTEXPR __DH__ native_half_t int2half_impl(unsigned short value) noexcept {
    return __ushort2half_rn(value);
}

template<>
AF_CONSTEXPR __DH__ native_half_t int2half_impl(char value) noexcept {
    return __ull2half_rn(value);
}
template<>
AF_CONSTEXPR __DH__ native_half_t int2half_impl(signed char value) noexcept {
    return __ull2half_rn(value);
}
template<>
AF_CONSTEXPR __DH__ native_half_t int2half_impl(unsigned char value) noexcept {
    return __ull2half_rn(value);
}

#elif defined(AF_ONEAPI)

template<std::float_round_style R = std::round_to_nearest>
AF_CONSTEXPR native_half_t float2half_impl(float value) {
    return static_cast<native_half_t>(value);
}

template<std::float_round_style R = std::round_to_nearest>
AF_CONSTEXPR native_half_t float2half_impl(double value) {
    return static_cast<native_half_t>(value);
}

inline float half2float_impl(native_half_t value) noexcept {
    return static_cast<float>(value);
}

template<std::float_round_style R, bool S, typename T>
AF_CONSTEXPR native_half_t int2half_impl(T value) noexcept {
    return static_cast<native_half_t>(value);
}

#else

/// Convert integer to half-precision floating point.
///
/// \tparam R rounding mode to use, `std::round_indeterminate` for fastest
///           rounding
/// \tparam S `true` if value negative, `false` else
/// \tparam T type to convert (builtin integer type)
///
/// \param value non-negative integral value
///
/// \return binary representation of half-precision value
template<std::float_round_style R, bool S, typename T>
AF_CONSTEXPR __DH__ native_half_t int2half_impl(T value) noexcept {
    static_assert(std::is_integral<T>::value,
                  "int to half conversion only supports builtin integer types");
    if (S) value = -value;
    uint16_t bits = S << 15;
    if (value > 0xFFFF) {
        AF_IF_CONSTEXPR(R == std::round_toward_infinity)
        bits |= (0x7C00 - S);
        else AF_IF_CONSTEXPR(R == std::round_toward_neg_infinity) bits |=
            (0x7BFF + S);
        else bits |= (0x7BFF + (R != std::round_toward_zero));
    } else if (value) {
        uint32_t m = value, exp = 24;
        for (; m < 0x400; m <<= 1, --exp)
            ;
        for (; m > 0x7FF; m >>= 1, ++exp)
            ;
        bits |= (exp << 10) + m;
        if (exp > 24) {
            AF_IF_CONSTEXPR(R == std::round_to_nearest)
            bits += (value >> (exp - 25)) & 1
#if HALF_ROUND_TIES_TO_EVEN
                    & (((((1 << (exp - 25)) - 1) & value) != 0) | bits)
#endif
                ;
            else AF_IF_CONSTEXPR(R == std::round_toward_infinity) bits +=
                ((value & ((1 << (exp - 24)) - 1)) != 0) & !S;
            else AF_IF_CONSTEXPR(R == std::round_toward_neg_infinity) bits +=
                ((value & ((1 << (exp - 24)) - 1)) != 0) & S;
        }
    }
    return bits;
}

/// Convert IEEE single-precision to half-precision.
/// Credit for this goes to [Jeroen van der
/// Zijp](ftp://ftp.fox-toolkit.org/pub/fasthalffloatconversion.pdf).
/// \tparam R rounding mode to use, `std::round_indeterminate` for fastest
/// rounding
///
/// \param value single-precision value
/// \return binary representation of half-precision value
template<std::float_round_style R = std::round_to_nearest>
__DH__ native_half_t float2half_impl(float value) noexcept {
    alignas(std::max(alignof(uint32_t), alignof(float))) float _value = value;
    uint32_t bits = *reinterpret_cast<uint32_t*>(&_value);

    constexpr uint16_t base_table[512] = {
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0002, 0x0004, 0x0008, 0x0010,
        0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0400, 0x0800, 0x0C00, 0x1000,
        0x1400, 0x1800, 0x1C00, 0x2000, 0x2400, 0x2800, 0x2C00, 0x3000, 0x3400,
        0x3800, 0x3C00, 0x4000, 0x4400, 0x4800, 0x4C00, 0x5000, 0x5400, 0x5800,
        0x5C00, 0x6000, 0x6400, 0x6800, 0x6C00, 0x7000, 0x7400, 0x7800, 0x7C00,
        0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
        0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
        0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
        0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
        0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
        0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
        0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
        0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
        0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
        0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
        0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
        0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
        0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
        0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
        0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
        0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
        0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
        0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
        0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
        0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
        0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
        0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
        0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
        0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8001,
        0x8002, 0x8004, 0x8008, 0x8010, 0x8020, 0x8040, 0x8080, 0x8100, 0x8200,
        0x8400, 0x8800, 0x8C00, 0x9000, 0x9400, 0x9800, 0x9C00, 0xA000, 0xA400,
        0xA800, 0xAC00, 0xB000, 0xB400, 0xB800, 0xBC00, 0xC000, 0xC400, 0xC800,
        0xCC00, 0xD000, 0xD400, 0xD800, 0xDC00, 0xE000, 0xE400, 0xE800, 0xEC00,
        0xF000, 0xF400, 0xF800, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00,
        0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00,
        0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00,
        0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00,
        0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00,
        0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00,
        0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00,
        0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00,
        0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00,
        0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00,
        0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00,
        0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00,
        0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00, 0xFC00};

    constexpr uint8_t shift_table[512] = {
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 23, 22, 21, 20, 19,
        18, 17, 16, 15, 14, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
        13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 13, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 23,
        22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 13, 13, 13, 13, 13, 13, 13, 13,
        13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
        13, 13, 13, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 13};
    alignas(std::max(alignof(uint16_t), alignof(native_half_t)))
        uint16_t hbits =
            base_table[bits >> 23] +
            static_cast<uint16_t>((bits & 0x7FFFFF) >> shift_table[bits >> 23]);
    AF_IF_CONSTEXPR(R == std::round_to_nearest)
    hbits +=
        (((bits & 0x7FFFFF) >> (shift_table[bits >> 23] - 1)) |
         (((bits >> 23) & 0xFF) == 102)) &
        ((hbits & 0x7C00) != 0x7C00)
#if HALF_ROUND_TIES_TO_EVEN
        & (((((static_cast<uint32>(1) << (shift_table[bits >> 23] - 1)) - 1) &
             bits) != 0) |
           hbits)
#endif
        ;
    else AF_IF_CONSTEXPR(R == std::round_toward_zero) hbits -=
        ((hbits & 0x7FFF) == 0x7C00) & ~shift_table[bits >> 23];
    else AF_IF_CONSTEXPR(R == std::round_toward_infinity) hbits +=
        ((((bits & 0x7FFFFF &
            ((static_cast<uint32_t>(1) << (shift_table[bits >> 23])) - 1)) !=
           0) |
          (((bits >> 23) <= 102) & ((bits >> 23) != 0))) &
         (hbits < 0x7C00)) -
        ((hbits == 0xFC00) & ((bits >> 23) != 511));
    else AF_IF_CONSTEXPR(R == std::round_toward_neg_infinity) hbits +=
        ((((bits & 0x7FFFFF &
            ((static_cast<uint32_t>(1) << (shift_table[bits >> 23])) - 1)) !=
           0) |
          (((bits >> 23) <= 358) & ((bits >> 23) != 256))) &
         (hbits < 0xFC00) & (hbits >> 15)) -
        ((hbits == 0x7C00) & ((bits >> 23) != 255));

    return *reinterpret_cast<native_half_t*>(&hbits);
}

/// Convert IEEE double-precision to half-precision.
///
/// \tparam R rounding mode to use, `std::round_indeterminate` for fastest
///           rounding
/// \param value double-precision value
///
/// \return binary representation of half-precision value
template<std::float_round_style R>
__DH__ native_half_t float2half_impl(double value) {
    alignas(std::max(alignof(uint64_t), alignof(double))) double _value = value;
    uint64_t bits = *reinterpret_cast<uint64_t*>(&_value);
    uint32_t hi = bits >> 32, lo = bits & 0xFFFFFFFF;
    alignas(std::max(alignof(uint16_t), alignof(native_half_t)))
        uint16_t hbits = (hi >> 16) & 0x8000;
    hi &= 0x7FFFFFFF;
    int exp = hi >> 20;
    if (exp == 2047)
        return hbits | 0x7C00 |
               (0x3FF & -static_cast<unsigned>((bits & 0xFFFFFFFFFFFFF) != 0));
    if (exp > 1038) {
        AF_IF_CONSTEXPR(R == std::round_toward_infinity)
        return hbits | (0x7C00 - (hbits >> 15));
        AF_IF_CONSTEXPR(R == std::round_toward_neg_infinity)
        return hbits | (0x7BFF + (hbits >> 15));
        return hbits | (0x7BFF + (R != std::round_toward_zero));
    }
    int g = 0, s = lo != 0;
    if (exp > 1008) {
        g = (hi >> 9) & 1;
        s |= (hi & 0x1FF) != 0;
        hbits |= ((exp - 1008) << 10) | ((hi >> 10) & 0x3FF);
    } else if (exp > 997) {
        int i = 1018 - exp;
        hi    = (hi & 0xFFFFF) | 0x100000;
        g     = (hi >> i) & 1;
        s |= (hi & ((1L << i) - 1)) != 0;
        hbits |= hi >> (i + 1);
    } else {
        s |= hi != 0;
    }
    AF_IF_CONSTEXPR(R == std::round_to_nearest)
#if HALF_ROUND_TIES_TO_EVEN
    hbits += g & (s | hbits);
#else
    hbits += g;
#endif
    else AF_IF_CONSTEXPR(R == std::round_toward_infinity) hbits +=
        ~(hbits >> 15) & (s | g);
    else AF_IF_CONSTEXPR(R == std::round_toward_neg_infinity) hbits +=
        (hbits >> 15) & (g | s);

    return *reinterpret_cast<native_half_t*>(&hbits);
}

__DH__ inline float half2float_impl(native_half_t value) noexcept {
    // return _cvtsh_ss(data.data_);
    constexpr uint32_t mantissa_table[2048] = {
        0x00000000, 0x33800000, 0x34000000, 0x34400000, 0x34800000, 0x34A00000,
        0x34C00000, 0x34E00000, 0x35000000, 0x35100000, 0x35200000, 0x35300000,
        0x35400000, 0x35500000, 0x35600000, 0x35700000, 0x35800000, 0x35880000,
        0x35900000, 0x35980000, 0x35A00000, 0x35A80000, 0x35B00000, 0x35B80000,
        0x35C00000, 0x35C80000, 0x35D00000, 0x35D80000, 0x35E00000, 0x35E80000,
        0x35F00000, 0x35F80000, 0x36000000, 0x36040000, 0x36080000, 0x360C0000,
        0x36100000, 0x36140000, 0x36180000, 0x361C0000, 0x36200000, 0x36240000,
        0x36280000, 0x362C0000, 0x36300000, 0x36340000, 0x36380000, 0x363C0000,
        0x36400000, 0x36440000, 0x36480000, 0x364C0000, 0x36500000, 0x36540000,
        0x36580000, 0x365C0000, 0x36600000, 0x36640000, 0x36680000, 0x366C0000,
        0x36700000, 0x36740000, 0x36780000, 0x367C0000, 0x36800000, 0x36820000,
        0x36840000, 0x36860000, 0x36880000, 0x368A0000, 0x368C0000, 0x368E0000,
        0x36900000, 0x36920000, 0x36940000, 0x36960000, 0x36980000, 0x369A0000,
        0x369C0000, 0x369E0000, 0x36A00000, 0x36A20000, 0x36A40000, 0x36A60000,
        0x36A80000, 0x36AA0000, 0x36AC0000, 0x36AE0000, 0x36B00000, 0x36B20000,
        0x36B40000, 0x36B60000, 0x36B80000, 0x36BA0000, 0x36BC0000, 0x36BE0000,
        0x36C00000, 0x36C20000, 0x36C40000, 0x36C60000, 0x36C80000, 0x36CA0000,
        0x36CC0000, 0x36CE0000, 0x36D00000, 0x36D20000, 0x36D40000, 0x36D60000,
        0x36D80000, 0x36DA0000, 0x36DC0000, 0x36DE0000, 0x36E00000, 0x36E20000,
        0x36E40000, 0x36E60000, 0x36E80000, 0x36EA0000, 0x36EC0000, 0x36EE0000,
        0x36F00000, 0x36F20000, 0x36F40000, 0x36F60000, 0x36F80000, 0x36FA0000,
        0x36FC0000, 0x36FE0000, 0x37000000, 0x37010000, 0x37020000, 0x37030000,
        0x37040000, 0x37050000, 0x37060000, 0x37070000, 0x37080000, 0x37090000,
        0x370A0000, 0x370B0000, 0x370C0000, 0x370D0000, 0x370E0000, 0x370F0000,
        0x37100000, 0x37110000, 0x37120000, 0x37130000, 0x37140000, 0x37150000,
        0x37160000, 0x37170000, 0x37180000, 0x37190000, 0x371A0000, 0x371B0000,
        0x371C0000, 0x371D0000, 0x371E0000, 0x371F0000, 0x37200000, 0x37210000,
        0x37220000, 0x37230000, 0x37240000, 0x37250000, 0x37260000, 0x37270000,
        0x37280000, 0x37290000, 0x372A0000, 0x372B0000, 0x372C0000, 0x372D0000,
        0x372E0000, 0x372F0000, 0x37300000, 0x37310000, 0x37320000, 0x37330000,
        0x37340000, 0x37350000, 0x37360000, 0x37370000, 0x37380000, 0x37390000,
        0x373A0000, 0x373B0000, 0x373C0000, 0x373D0000, 0x373E0000, 0x373F0000,
        0x37400000, 0x37410000, 0x37420000, 0x37430000, 0x37440000, 0x37450000,
        0x37460000, 0x37470000, 0x37480000, 0x37490000, 0x374A0000, 0x374B0000,
        0x374C0000, 0x374D0000, 0x374E0000, 0x374F0000, 0x37500000, 0x37510000,
        0x37520000, 0x37530000, 0x37540000, 0x37550000, 0x37560000, 0x37570000,
        0x37580000, 0x37590000, 0x375A0000, 0x375B0000, 0x375C0000, 0x375D0000,
        0x375E0000, 0x375F0000, 0x37600000, 0x37610000, 0x37620000, 0x37630000,
        0x37640000, 0x37650000, 0x37660000, 0x37670000, 0x37680000, 0x37690000,
        0x376A0000, 0x376B0000, 0x376C0000, 0x376D0000, 0x376E0000, 0x376F0000,
        0x37700000, 0x37710000, 0x37720000, 0x37730000, 0x37740000, 0x37750000,
        0x37760000, 0x37770000, 0x37780000, 0x37790000, 0x377A0000, 0x377B0000,
        0x377C0000, 0x377D0000, 0x377E0000, 0x377F0000, 0x37800000, 0x37808000,
        0x37810000, 0x37818000, 0x37820000, 0x37828000, 0x37830000, 0x37838000,
        0x37840000, 0x37848000, 0x37850000, 0x37858000, 0x37860000, 0x37868000,
        0x37870000, 0x37878000, 0x37880000, 0x37888000, 0x37890000, 0x37898000,
        0x378A0000, 0x378A8000, 0x378B0000, 0x378B8000, 0x378C0000, 0x378C8000,
        0x378D0000, 0x378D8000, 0x378E0000, 0x378E8000, 0x378F0000, 0x378F8000,
        0x37900000, 0x37908000, 0x37910000, 0x37918000, 0x37920000, 0x37928000,
        0x37930000, 0x37938000, 0x37940000, 0x37948000, 0x37950000, 0x37958000,
        0x37960000, 0x37968000, 0x37970000, 0x37978000, 0x37980000, 0x37988000,
        0x37990000, 0x37998000, 0x379A0000, 0x379A8000, 0x379B0000, 0x379B8000,
        0x379C0000, 0x379C8000, 0x379D0000, 0x379D8000, 0x379E0000, 0x379E8000,
        0x379F0000, 0x379F8000, 0x37A00000, 0x37A08000, 0x37A10000, 0x37A18000,
        0x37A20000, 0x37A28000, 0x37A30000, 0x37A38000, 0x37A40000, 0x37A48000,
        0x37A50000, 0x37A58000, 0x37A60000, 0x37A68000, 0x37A70000, 0x37A78000,
        0x37A80000, 0x37A88000, 0x37A90000, 0x37A98000, 0x37AA0000, 0x37AA8000,
        0x37AB0000, 0x37AB8000, 0x37AC0000, 0x37AC8000, 0x37AD0000, 0x37AD8000,
        0x37AE0000, 0x37AE8000, 0x37AF0000, 0x37AF8000, 0x37B00000, 0x37B08000,
        0x37B10000, 0x37B18000, 0x37B20000, 0x37B28000, 0x37B30000, 0x37B38000,
        0x37B40000, 0x37B48000, 0x37B50000, 0x37B58000, 0x37B60000, 0x37B68000,
        0x37B70000, 0x37B78000, 0x37B80000, 0x37B88000, 0x37B90000, 0x37B98000,
        0x37BA0000, 0x37BA8000, 0x37BB0000, 0x37BB8000, 0x37BC0000, 0x37BC8000,
        0x37BD0000, 0x37BD8000, 0x37BE0000, 0x37BE8000, 0x37BF0000, 0x37BF8000,
        0x37C00000, 0x37C08000, 0x37C10000, 0x37C18000, 0x37C20000, 0x37C28000,
        0x37C30000, 0x37C38000, 0x37C40000, 0x37C48000, 0x37C50000, 0x37C58000,
        0x37C60000, 0x37C68000, 0x37C70000, 0x37C78000, 0x37C80000, 0x37C88000,
        0x37C90000, 0x37C98000, 0x37CA0000, 0x37CA8000, 0x37CB0000, 0x37CB8000,
        0x37CC0000, 0x37CC8000, 0x37CD0000, 0x37CD8000, 0x37CE0000, 0x37CE8000,
        0x37CF0000, 0x37CF8000, 0x37D00000, 0x37D08000, 0x37D10000, 0x37D18000,
        0x37D20000, 0x37D28000, 0x37D30000, 0x37D38000, 0x37D40000, 0x37D48000,
        0x37D50000, 0x37D58000, 0x37D60000, 0x37D68000, 0x37D70000, 0x37D78000,
        0x37D80000, 0x37D88000, 0x37D90000, 0x37D98000, 0x37DA0000, 0x37DA8000,
        0x37DB0000, 0x37DB8000, 0x37DC0000, 0x37DC8000, 0x37DD0000, 0x37DD8000,
        0x37DE0000, 0x37DE8000, 0x37DF0000, 0x37DF8000, 0x37E00000, 0x37E08000,
        0x37E10000, 0x37E18000, 0x37E20000, 0x37E28000, 0x37E30000, 0x37E38000,
        0x37E40000, 0x37E48000, 0x37E50000, 0x37E58000, 0x37E60000, 0x37E68000,
        0x37E70000, 0x37E78000, 0x37E80000, 0x37E88000, 0x37E90000, 0x37E98000,
        0x37EA0000, 0x37EA8000, 0x37EB0000, 0x37EB8000, 0x37EC0000, 0x37EC8000,
        0x37ED0000, 0x37ED8000, 0x37EE0000, 0x37EE8000, 0x37EF0000, 0x37EF8000,
        0x37F00000, 0x37F08000, 0x37F10000, 0x37F18000, 0x37F20000, 0x37F28000,
        0x37F30000, 0x37F38000, 0x37F40000, 0x37F48000, 0x37F50000, 0x37F58000,
        0x37F60000, 0x37F68000, 0x37F70000, 0x37F78000, 0x37F80000, 0x37F88000,
        0x37F90000, 0x37F98000, 0x37FA0000, 0x37FA8000, 0x37FB0000, 0x37FB8000,
        0x37FC0000, 0x37FC8000, 0x37FD0000, 0x37FD8000, 0x37FE0000, 0x37FE8000,
        0x37FF0000, 0x37FF8000, 0x38000000, 0x38004000, 0x38008000, 0x3800C000,
        0x38010000, 0x38014000, 0x38018000, 0x3801C000, 0x38020000, 0x38024000,
        0x38028000, 0x3802C000, 0x38030000, 0x38034000, 0x38038000, 0x3803C000,
        0x38040000, 0x38044000, 0x38048000, 0x3804C000, 0x38050000, 0x38054000,
        0x38058000, 0x3805C000, 0x38060000, 0x38064000, 0x38068000, 0x3806C000,
        0x38070000, 0x38074000, 0x38078000, 0x3807C000, 0x38080000, 0x38084000,
        0x38088000, 0x3808C000, 0x38090000, 0x38094000, 0x38098000, 0x3809C000,
        0x380A0000, 0x380A4000, 0x380A8000, 0x380AC000, 0x380B0000, 0x380B4000,
        0x380B8000, 0x380BC000, 0x380C0000, 0x380C4000, 0x380C8000, 0x380CC000,
        0x380D0000, 0x380D4000, 0x380D8000, 0x380DC000, 0x380E0000, 0x380E4000,
        0x380E8000, 0x380EC000, 0x380F0000, 0x380F4000, 0x380F8000, 0x380FC000,
        0x38100000, 0x38104000, 0x38108000, 0x3810C000, 0x38110000, 0x38114000,
        0x38118000, 0x3811C000, 0x38120000, 0x38124000, 0x38128000, 0x3812C000,
        0x38130000, 0x38134000, 0x38138000, 0x3813C000, 0x38140000, 0x38144000,
        0x38148000, 0x3814C000, 0x38150000, 0x38154000, 0x38158000, 0x3815C000,
        0x38160000, 0x38164000, 0x38168000, 0x3816C000, 0x38170000, 0x38174000,
        0x38178000, 0x3817C000, 0x38180000, 0x38184000, 0x38188000, 0x3818C000,
        0x38190000, 0x38194000, 0x38198000, 0x3819C000, 0x381A0000, 0x381A4000,
        0x381A8000, 0x381AC000, 0x381B0000, 0x381B4000, 0x381B8000, 0x381BC000,
        0x381C0000, 0x381C4000, 0x381C8000, 0x381CC000, 0x381D0000, 0x381D4000,
        0x381D8000, 0x381DC000, 0x381E0000, 0x381E4000, 0x381E8000, 0x381EC000,
        0x381F0000, 0x381F4000, 0x381F8000, 0x381FC000, 0x38200000, 0x38204000,
        0x38208000, 0x3820C000, 0x38210000, 0x38214000, 0x38218000, 0x3821C000,
        0x38220000, 0x38224000, 0x38228000, 0x3822C000, 0x38230000, 0x38234000,
        0x38238000, 0x3823C000, 0x38240000, 0x38244000, 0x38248000, 0x3824C000,
        0x38250000, 0x38254000, 0x38258000, 0x3825C000, 0x38260000, 0x38264000,
        0x38268000, 0x3826C000, 0x38270000, 0x38274000, 0x38278000, 0x3827C000,
        0x38280000, 0x38284000, 0x38288000, 0x3828C000, 0x38290000, 0x38294000,
        0x38298000, 0x3829C000, 0x382A0000, 0x382A4000, 0x382A8000, 0x382AC000,
        0x382B0000, 0x382B4000, 0x382B8000, 0x382BC000, 0x382C0000, 0x382C4000,
        0x382C8000, 0x382CC000, 0x382D0000, 0x382D4000, 0x382D8000, 0x382DC000,
        0x382E0000, 0x382E4000, 0x382E8000, 0x382EC000, 0x382F0000, 0x382F4000,
        0x382F8000, 0x382FC000, 0x38300000, 0x38304000, 0x38308000, 0x3830C000,
        0x38310000, 0x38314000, 0x38318000, 0x3831C000, 0x38320000, 0x38324000,
        0x38328000, 0x3832C000, 0x38330000, 0x38334000, 0x38338000, 0x3833C000,
        0x38340000, 0x38344000, 0x38348000, 0x3834C000, 0x38350000, 0x38354000,
        0x38358000, 0x3835C000, 0x38360000, 0x38364000, 0x38368000, 0x3836C000,
        0x38370000, 0x38374000, 0x38378000, 0x3837C000, 0x38380000, 0x38384000,
        0x38388000, 0x3838C000, 0x38390000, 0x38394000, 0x38398000, 0x3839C000,
        0x383A0000, 0x383A4000, 0x383A8000, 0x383AC000, 0x383B0000, 0x383B4000,
        0x383B8000, 0x383BC000, 0x383C0000, 0x383C4000, 0x383C8000, 0x383CC000,
        0x383D0000, 0x383D4000, 0x383D8000, 0x383DC000, 0x383E0000, 0x383E4000,
        0x383E8000, 0x383EC000, 0x383F0000, 0x383F4000, 0x383F8000, 0x383FC000,
        0x38400000, 0x38404000, 0x38408000, 0x3840C000, 0x38410000, 0x38414000,
        0x38418000, 0x3841C000, 0x38420000, 0x38424000, 0x38428000, 0x3842C000,
        0x38430000, 0x38434000, 0x38438000, 0x3843C000, 0x38440000, 0x38444000,
        0x38448000, 0x3844C000, 0x38450000, 0x38454000, 0x38458000, 0x3845C000,
        0x38460000, 0x38464000, 0x38468000, 0x3846C000, 0x38470000, 0x38474000,
        0x38478000, 0x3847C000, 0x38480000, 0x38484000, 0x38488000, 0x3848C000,
        0x38490000, 0x38494000, 0x38498000, 0x3849C000, 0x384A0000, 0x384A4000,
        0x384A8000, 0x384AC000, 0x384B0000, 0x384B4000, 0x384B8000, 0x384BC000,
        0x384C0000, 0x384C4000, 0x384C8000, 0x384CC000, 0x384D0000, 0x384D4000,
        0x384D8000, 0x384DC000, 0x384E0000, 0x384E4000, 0x384E8000, 0x384EC000,
        0x384F0000, 0x384F4000, 0x384F8000, 0x384FC000, 0x38500000, 0x38504000,
        0x38508000, 0x3850C000, 0x38510000, 0x38514000, 0x38518000, 0x3851C000,
        0x38520000, 0x38524000, 0x38528000, 0x3852C000, 0x38530000, 0x38534000,
        0x38538000, 0x3853C000, 0x38540000, 0x38544000, 0x38548000, 0x3854C000,
        0x38550000, 0x38554000, 0x38558000, 0x3855C000, 0x38560000, 0x38564000,
        0x38568000, 0x3856C000, 0x38570000, 0x38574000, 0x38578000, 0x3857C000,
        0x38580000, 0x38584000, 0x38588000, 0x3858C000, 0x38590000, 0x38594000,
        0x38598000, 0x3859C000, 0x385A0000, 0x385A4000, 0x385A8000, 0x385AC000,
        0x385B0000, 0x385B4000, 0x385B8000, 0x385BC000, 0x385C0000, 0x385C4000,
        0x385C8000, 0x385CC000, 0x385D0000, 0x385D4000, 0x385D8000, 0x385DC000,
        0x385E0000, 0x385E4000, 0x385E8000, 0x385EC000, 0x385F0000, 0x385F4000,
        0x385F8000, 0x385FC000, 0x38600000, 0x38604000, 0x38608000, 0x3860C000,
        0x38610000, 0x38614000, 0x38618000, 0x3861C000, 0x38620000, 0x38624000,
        0x38628000, 0x3862C000, 0x38630000, 0x38634000, 0x38638000, 0x3863C000,
        0x38640000, 0x38644000, 0x38648000, 0x3864C000, 0x38650000, 0x38654000,
        0x38658000, 0x3865C000, 0x38660000, 0x38664000, 0x38668000, 0x3866C000,
        0x38670000, 0x38674000, 0x38678000, 0x3867C000, 0x38680000, 0x38684000,
        0x38688000, 0x3868C000, 0x38690000, 0x38694000, 0x38698000, 0x3869C000,
        0x386A0000, 0x386A4000, 0x386A8000, 0x386AC000, 0x386B0000, 0x386B4000,
        0x386B8000, 0x386BC000, 0x386C0000, 0x386C4000, 0x386C8000, 0x386CC000,
        0x386D0000, 0x386D4000, 0x386D8000, 0x386DC000, 0x386E0000, 0x386E4000,
        0x386E8000, 0x386EC000, 0x386F0000, 0x386F4000, 0x386F8000, 0x386FC000,
        0x38700000, 0x38704000, 0x38708000, 0x3870C000, 0x38710000, 0x38714000,
        0x38718000, 0x3871C000, 0x38720000, 0x38724000, 0x38728000, 0x3872C000,
        0x38730000, 0x38734000, 0x38738000, 0x3873C000, 0x38740000, 0x38744000,
        0x38748000, 0x3874C000, 0x38750000, 0x38754000, 0x38758000, 0x3875C000,
        0x38760000, 0x38764000, 0x38768000, 0x3876C000, 0x38770000, 0x38774000,
        0x38778000, 0x3877C000, 0x38780000, 0x38784000, 0x38788000, 0x3878C000,
        0x38790000, 0x38794000, 0x38798000, 0x3879C000, 0x387A0000, 0x387A4000,
        0x387A8000, 0x387AC000, 0x387B0000, 0x387B4000, 0x387B8000, 0x387BC000,
        0x387C0000, 0x387C4000, 0x387C8000, 0x387CC000, 0x387D0000, 0x387D4000,
        0x387D8000, 0x387DC000, 0x387E0000, 0x387E4000, 0x387E8000, 0x387EC000,
        0x387F0000, 0x387F4000, 0x387F8000, 0x387FC000, 0x38000000, 0x38002000,
        0x38004000, 0x38006000, 0x38008000, 0x3800A000, 0x3800C000, 0x3800E000,
        0x38010000, 0x38012000, 0x38014000, 0x38016000, 0x38018000, 0x3801A000,
        0x3801C000, 0x3801E000, 0x38020000, 0x38022000, 0x38024000, 0x38026000,
        0x38028000, 0x3802A000, 0x3802C000, 0x3802E000, 0x38030000, 0x38032000,
        0x38034000, 0x38036000, 0x38038000, 0x3803A000, 0x3803C000, 0x3803E000,
        0x38040000, 0x38042000, 0x38044000, 0x38046000, 0x38048000, 0x3804A000,
        0x3804C000, 0x3804E000, 0x38050000, 0x38052000, 0x38054000, 0x38056000,
        0x38058000, 0x3805A000, 0x3805C000, 0x3805E000, 0x38060000, 0x38062000,
        0x38064000, 0x38066000, 0x38068000, 0x3806A000, 0x3806C000, 0x3806E000,
        0x38070000, 0x38072000, 0x38074000, 0x38076000, 0x38078000, 0x3807A000,
        0x3807C000, 0x3807E000, 0x38080000, 0x38082000, 0x38084000, 0x38086000,
        0x38088000, 0x3808A000, 0x3808C000, 0x3808E000, 0x38090000, 0x38092000,
        0x38094000, 0x38096000, 0x38098000, 0x3809A000, 0x3809C000, 0x3809E000,
        0x380A0000, 0x380A2000, 0x380A4000, 0x380A6000, 0x380A8000, 0x380AA000,
        0x380AC000, 0x380AE000, 0x380B0000, 0x380B2000, 0x380B4000, 0x380B6000,
        0x380B8000, 0x380BA000, 0x380BC000, 0x380BE000, 0x380C0000, 0x380C2000,
        0x380C4000, 0x380C6000, 0x380C8000, 0x380CA000, 0x380CC000, 0x380CE000,
        0x380D0000, 0x380D2000, 0x380D4000, 0x380D6000, 0x380D8000, 0x380DA000,
        0x380DC000, 0x380DE000, 0x380E0000, 0x380E2000, 0x380E4000, 0x380E6000,
        0x380E8000, 0x380EA000, 0x380EC000, 0x380EE000, 0x380F0000, 0x380F2000,
        0x380F4000, 0x380F6000, 0x380F8000, 0x380FA000, 0x380FC000, 0x380FE000,
        0x38100000, 0x38102000, 0x38104000, 0x38106000, 0x38108000, 0x3810A000,
        0x3810C000, 0x3810E000, 0x38110000, 0x38112000, 0x38114000, 0x38116000,
        0x38118000, 0x3811A000, 0x3811C000, 0x3811E000, 0x38120000, 0x38122000,
        0x38124000, 0x38126000, 0x38128000, 0x3812A000, 0x3812C000, 0x3812E000,
        0x38130000, 0x38132000, 0x38134000, 0x38136000, 0x38138000, 0x3813A000,
        0x3813C000, 0x3813E000, 0x38140000, 0x38142000, 0x38144000, 0x38146000,
        0x38148000, 0x3814A000, 0x3814C000, 0x3814E000, 0x38150000, 0x38152000,
        0x38154000, 0x38156000, 0x38158000, 0x3815A000, 0x3815C000, 0x3815E000,
        0x38160000, 0x38162000, 0x38164000, 0x38166000, 0x38168000, 0x3816A000,
        0x3816C000, 0x3816E000, 0x38170000, 0x38172000, 0x38174000, 0x38176000,
        0x38178000, 0x3817A000, 0x3817C000, 0x3817E000, 0x38180000, 0x38182000,
        0x38184000, 0x38186000, 0x38188000, 0x3818A000, 0x3818C000, 0x3818E000,
        0x38190000, 0x38192000, 0x38194000, 0x38196000, 0x38198000, 0x3819A000,
        0x3819C000, 0x3819E000, 0x381A0000, 0x381A2000, 0x381A4000, 0x381A6000,
        0x381A8000, 0x381AA000, 0x381AC000, 0x381AE000, 0x381B0000, 0x381B2000,
        0x381B4000, 0x381B6000, 0x381B8000, 0x381BA000, 0x381BC000, 0x381BE000,
        0x381C0000, 0x381C2000, 0x381C4000, 0x381C6000, 0x381C8000, 0x381CA000,
        0x381CC000, 0x381CE000, 0x381D0000, 0x381D2000, 0x381D4000, 0x381D6000,
        0x381D8000, 0x381DA000, 0x381DC000, 0x381DE000, 0x381E0000, 0x381E2000,
        0x381E4000, 0x381E6000, 0x381E8000, 0x381EA000, 0x381EC000, 0x381EE000,
        0x381F0000, 0x381F2000, 0x381F4000, 0x381F6000, 0x381F8000, 0x381FA000,
        0x381FC000, 0x381FE000, 0x38200000, 0x38202000, 0x38204000, 0x38206000,
        0x38208000, 0x3820A000, 0x3820C000, 0x3820E000, 0x38210000, 0x38212000,
        0x38214000, 0x38216000, 0x38218000, 0x3821A000, 0x3821C000, 0x3821E000,
        0x38220000, 0x38222000, 0x38224000, 0x38226000, 0x38228000, 0x3822A000,
        0x3822C000, 0x3822E000, 0x38230000, 0x38232000, 0x38234000, 0x38236000,
        0x38238000, 0x3823A000, 0x3823C000, 0x3823E000, 0x38240000, 0x38242000,
        0x38244000, 0x38246000, 0x38248000, 0x3824A000, 0x3824C000, 0x3824E000,
        0x38250000, 0x38252000, 0x38254000, 0x38256000, 0x38258000, 0x3825A000,
        0x3825C000, 0x3825E000, 0x38260000, 0x38262000, 0x38264000, 0x38266000,
        0x38268000, 0x3826A000, 0x3826C000, 0x3826E000, 0x38270000, 0x38272000,
        0x38274000, 0x38276000, 0x38278000, 0x3827A000, 0x3827C000, 0x3827E000,
        0x38280000, 0x38282000, 0x38284000, 0x38286000, 0x38288000, 0x3828A000,
        0x3828C000, 0x3828E000, 0x38290000, 0x38292000, 0x38294000, 0x38296000,
        0x38298000, 0x3829A000, 0x3829C000, 0x3829E000, 0x382A0000, 0x382A2000,
        0x382A4000, 0x382A6000, 0x382A8000, 0x382AA000, 0x382AC000, 0x382AE000,
        0x382B0000, 0x382B2000, 0x382B4000, 0x382B6000, 0x382B8000, 0x382BA000,
        0x382BC000, 0x382BE000, 0x382C0000, 0x382C2000, 0x382C4000, 0x382C6000,
        0x382C8000, 0x382CA000, 0x382CC000, 0x382CE000, 0x382D0000, 0x382D2000,
        0x382D4000, 0x382D6000, 0x382D8000, 0x382DA000, 0x382DC000, 0x382DE000,
        0x382E0000, 0x382E2000, 0x382E4000, 0x382E6000, 0x382E8000, 0x382EA000,
        0x382EC000, 0x382EE000, 0x382F0000, 0x382F2000, 0x382F4000, 0x382F6000,
        0x382F8000, 0x382FA000, 0x382FC000, 0x382FE000, 0x38300000, 0x38302000,
        0x38304000, 0x38306000, 0x38308000, 0x3830A000, 0x3830C000, 0x3830E000,
        0x38310000, 0x38312000, 0x38314000, 0x38316000, 0x38318000, 0x3831A000,
        0x3831C000, 0x3831E000, 0x38320000, 0x38322000, 0x38324000, 0x38326000,
        0x38328000, 0x3832A000, 0x3832C000, 0x3832E000, 0x38330000, 0x38332000,
        0x38334000, 0x38336000, 0x38338000, 0x3833A000, 0x3833C000, 0x3833E000,
        0x38340000, 0x38342000, 0x38344000, 0x38346000, 0x38348000, 0x3834A000,
        0x3834C000, 0x3834E000, 0x38350000, 0x38352000, 0x38354000, 0x38356000,
        0x38358000, 0x3835A000, 0x3835C000, 0x3835E000, 0x38360000, 0x38362000,
        0x38364000, 0x38366000, 0x38368000, 0x3836A000, 0x3836C000, 0x3836E000,
        0x38370000, 0x38372000, 0x38374000, 0x38376000, 0x38378000, 0x3837A000,
        0x3837C000, 0x3837E000, 0x38380000, 0x38382000, 0x38384000, 0x38386000,
        0x38388000, 0x3838A000, 0x3838C000, 0x3838E000, 0x38390000, 0x38392000,
        0x38394000, 0x38396000, 0x38398000, 0x3839A000, 0x3839C000, 0x3839E000,
        0x383A0000, 0x383A2000, 0x383A4000, 0x383A6000, 0x383A8000, 0x383AA000,
        0x383AC000, 0x383AE000, 0x383B0000, 0x383B2000, 0x383B4000, 0x383B6000,
        0x383B8000, 0x383BA000, 0x383BC000, 0x383BE000, 0x383C0000, 0x383C2000,
        0x383C4000, 0x383C6000, 0x383C8000, 0x383CA000, 0x383CC000, 0x383CE000,
        0x383D0000, 0x383D2000, 0x383D4000, 0x383D6000, 0x383D8000, 0x383DA000,
        0x383DC000, 0x383DE000, 0x383E0000, 0x383E2000, 0x383E4000, 0x383E6000,
        0x383E8000, 0x383EA000, 0x383EC000, 0x383EE000, 0x383F0000, 0x383F2000,
        0x383F4000, 0x383F6000, 0x383F8000, 0x383FA000, 0x383FC000, 0x383FE000,
        0x38400000, 0x38402000, 0x38404000, 0x38406000, 0x38408000, 0x3840A000,
        0x3840C000, 0x3840E000, 0x38410000, 0x38412000, 0x38414000, 0x38416000,
        0x38418000, 0x3841A000, 0x3841C000, 0x3841E000, 0x38420000, 0x38422000,
        0x38424000, 0x38426000, 0x38428000, 0x3842A000, 0x3842C000, 0x3842E000,
        0x38430000, 0x38432000, 0x38434000, 0x38436000, 0x38438000, 0x3843A000,
        0x3843C000, 0x3843E000, 0x38440000, 0x38442000, 0x38444000, 0x38446000,
        0x38448000, 0x3844A000, 0x3844C000, 0x3844E000, 0x38450000, 0x38452000,
        0x38454000, 0x38456000, 0x38458000, 0x3845A000, 0x3845C000, 0x3845E000,
        0x38460000, 0x38462000, 0x38464000, 0x38466000, 0x38468000, 0x3846A000,
        0x3846C000, 0x3846E000, 0x38470000, 0x38472000, 0x38474000, 0x38476000,
        0x38478000, 0x3847A000, 0x3847C000, 0x3847E000, 0x38480000, 0x38482000,
        0x38484000, 0x38486000, 0x38488000, 0x3848A000, 0x3848C000, 0x3848E000,
        0x38490000, 0x38492000, 0x38494000, 0x38496000, 0x38498000, 0x3849A000,
        0x3849C000, 0x3849E000, 0x384A0000, 0x384A2000, 0x384A4000, 0x384A6000,
        0x384A8000, 0x384AA000, 0x384AC000, 0x384AE000, 0x384B0000, 0x384B2000,
        0x384B4000, 0x384B6000, 0x384B8000, 0x384BA000, 0x384BC000, 0x384BE000,
        0x384C0000, 0x384C2000, 0x384C4000, 0x384C6000, 0x384C8000, 0x384CA000,
        0x384CC000, 0x384CE000, 0x384D0000, 0x384D2000, 0x384D4000, 0x384D6000,
        0x384D8000, 0x384DA000, 0x384DC000, 0x384DE000, 0x384E0000, 0x384E2000,
        0x384E4000, 0x384E6000, 0x384E8000, 0x384EA000, 0x384EC000, 0x384EE000,
        0x384F0000, 0x384F2000, 0x384F4000, 0x384F6000, 0x384F8000, 0x384FA000,
        0x384FC000, 0x384FE000, 0x38500000, 0x38502000, 0x38504000, 0x38506000,
        0x38508000, 0x3850A000, 0x3850C000, 0x3850E000, 0x38510000, 0x38512000,
        0x38514000, 0x38516000, 0x38518000, 0x3851A000, 0x3851C000, 0x3851E000,
        0x38520000, 0x38522000, 0x38524000, 0x38526000, 0x38528000, 0x3852A000,
        0x3852C000, 0x3852E000, 0x38530000, 0x38532000, 0x38534000, 0x38536000,
        0x38538000, 0x3853A000, 0x3853C000, 0x3853E000, 0x38540000, 0x38542000,
        0x38544000, 0x38546000, 0x38548000, 0x3854A000, 0x3854C000, 0x3854E000,
        0x38550000, 0x38552000, 0x38554000, 0x38556000, 0x38558000, 0x3855A000,
        0x3855C000, 0x3855E000, 0x38560000, 0x38562000, 0x38564000, 0x38566000,
        0x38568000, 0x3856A000, 0x3856C000, 0x3856E000, 0x38570000, 0x38572000,
        0x38574000, 0x38576000, 0x38578000, 0x3857A000, 0x3857C000, 0x3857E000,
        0x38580000, 0x38582000, 0x38584000, 0x38586000, 0x38588000, 0x3858A000,
        0x3858C000, 0x3858E000, 0x38590000, 0x38592000, 0x38594000, 0x38596000,
        0x38598000, 0x3859A000, 0x3859C000, 0x3859E000, 0x385A0000, 0x385A2000,
        0x385A4000, 0x385A6000, 0x385A8000, 0x385AA000, 0x385AC000, 0x385AE000,
        0x385B0000, 0x385B2000, 0x385B4000, 0x385B6000, 0x385B8000, 0x385BA000,
        0x385BC000, 0x385BE000, 0x385C0000, 0x385C2000, 0x385C4000, 0x385C6000,
        0x385C8000, 0x385CA000, 0x385CC000, 0x385CE000, 0x385D0000, 0x385D2000,
        0x385D4000, 0x385D6000, 0x385D8000, 0x385DA000, 0x385DC000, 0x385DE000,
        0x385E0000, 0x385E2000, 0x385E4000, 0x385E6000, 0x385E8000, 0x385EA000,
        0x385EC000, 0x385EE000, 0x385F0000, 0x385F2000, 0x385F4000, 0x385F6000,
        0x385F8000, 0x385FA000, 0x385FC000, 0x385FE000, 0x38600000, 0x38602000,
        0x38604000, 0x38606000, 0x38608000, 0x3860A000, 0x3860C000, 0x3860E000,
        0x38610000, 0x38612000, 0x38614000, 0x38616000, 0x38618000, 0x3861A000,
        0x3861C000, 0x3861E000, 0x38620000, 0x38622000, 0x38624000, 0x38626000,
        0x38628000, 0x3862A000, 0x3862C000, 0x3862E000, 0x38630000, 0x38632000,
        0x38634000, 0x38636000, 0x38638000, 0x3863A000, 0x3863C000, 0x3863E000,
        0x38640000, 0x38642000, 0x38644000, 0x38646000, 0x38648000, 0x3864A000,
        0x3864C000, 0x3864E000, 0x38650000, 0x38652000, 0x38654000, 0x38656000,
        0x38658000, 0x3865A000, 0x3865C000, 0x3865E000, 0x38660000, 0x38662000,
        0x38664000, 0x38666000, 0x38668000, 0x3866A000, 0x3866C000, 0x3866E000,
        0x38670000, 0x38672000, 0x38674000, 0x38676000, 0x38678000, 0x3867A000,
        0x3867C000, 0x3867E000, 0x38680000, 0x38682000, 0x38684000, 0x38686000,
        0x38688000, 0x3868A000, 0x3868C000, 0x3868E000, 0x38690000, 0x38692000,
        0x38694000, 0x38696000, 0x38698000, 0x3869A000, 0x3869C000, 0x3869E000,
        0x386A0000, 0x386A2000, 0x386A4000, 0x386A6000, 0x386A8000, 0x386AA000,
        0x386AC000, 0x386AE000, 0x386B0000, 0x386B2000, 0x386B4000, 0x386B6000,
        0x386B8000, 0x386BA000, 0x386BC000, 0x386BE000, 0x386C0000, 0x386C2000,
        0x386C4000, 0x386C6000, 0x386C8000, 0x386CA000, 0x386CC000, 0x386CE000,
        0x386D0000, 0x386D2000, 0x386D4000, 0x386D6000, 0x386D8000, 0x386DA000,
        0x386DC000, 0x386DE000, 0x386E0000, 0x386E2000, 0x386E4000, 0x386E6000,
        0x386E8000, 0x386EA000, 0x386EC000, 0x386EE000, 0x386F0000, 0x386F2000,
        0x386F4000, 0x386F6000, 0x386F8000, 0x386FA000, 0x386FC000, 0x386FE000,
        0x38700000, 0x38702000, 0x38704000, 0x38706000, 0x38708000, 0x3870A000,
        0x3870C000, 0x3870E000, 0x38710000, 0x38712000, 0x38714000, 0x38716000,
        0x38718000, 0x3871A000, 0x3871C000, 0x3871E000, 0x38720000, 0x38722000,
        0x38724000, 0x38726000, 0x38728000, 0x3872A000, 0x3872C000, 0x3872E000,
        0x38730000, 0x38732000, 0x38734000, 0x38736000, 0x38738000, 0x3873A000,
        0x3873C000, 0x3873E000, 0x38740000, 0x38742000, 0x38744000, 0x38746000,
        0x38748000, 0x3874A000, 0x3874C000, 0x3874E000, 0x38750000, 0x38752000,
        0x38754000, 0x38756000, 0x38758000, 0x3875A000, 0x3875C000, 0x3875E000,
        0x38760000, 0x38762000, 0x38764000, 0x38766000, 0x38768000, 0x3876A000,
        0x3876C000, 0x3876E000, 0x38770000, 0x38772000, 0x38774000, 0x38776000,
        0x38778000, 0x3877A000, 0x3877C000, 0x3877E000, 0x38780000, 0x38782000,
        0x38784000, 0x38786000, 0x38788000, 0x3878A000, 0x3878C000, 0x3878E000,
        0x38790000, 0x38792000, 0x38794000, 0x38796000, 0x38798000, 0x3879A000,
        0x3879C000, 0x3879E000, 0x387A0000, 0x387A2000, 0x387A4000, 0x387A6000,
        0x387A8000, 0x387AA000, 0x387AC000, 0x387AE000, 0x387B0000, 0x387B2000,
        0x387B4000, 0x387B6000, 0x387B8000, 0x387BA000, 0x387BC000, 0x387BE000,
        0x387C0000, 0x387C2000, 0x387C4000, 0x387C6000, 0x387C8000, 0x387CA000,
        0x387CC000, 0x387CE000, 0x387D0000, 0x387D2000, 0x387D4000, 0x387D6000,
        0x387D8000, 0x387DA000, 0x387DC000, 0x387DE000, 0x387E0000, 0x387E2000,
        0x387E4000, 0x387E6000, 0x387E8000, 0x387EA000, 0x387EC000, 0x387EE000,
        0x387F0000, 0x387F2000, 0x387F4000, 0x387F6000, 0x387F8000, 0x387FA000,
        0x387FC000, 0x387FE000};

    constexpr uint32_t exponent_table[64] = {
        0x00000000, 0x00800000, 0x01000000, 0x01800000, 0x02000000, 0x02800000,
        0x03000000, 0x03800000, 0x04000000, 0x04800000, 0x05000000, 0x05800000,
        0x06000000, 0x06800000, 0x07000000, 0x07800000, 0x08000000, 0x08800000,
        0x09000000, 0x09800000, 0x0A000000, 0x0A800000, 0x0B000000, 0x0B800000,
        0x0C000000, 0x0C800000, 0x0D000000, 0x0D800000, 0x0E000000, 0x0E800000,
        0x0F000000, 0x47800000, 0x80000000, 0x80800000, 0x81000000, 0x81800000,
        0x82000000, 0x82800000, 0x83000000, 0x83800000, 0x84000000, 0x84800000,
        0x85000000, 0x85800000, 0x86000000, 0x86800000, 0x87000000, 0x87800000,
        0x88000000, 0x88800000, 0x89000000, 0x89800000, 0x8A000000, 0x8A800000,
        0x8B000000, 0x8B800000, 0x8C000000, 0x8C800000, 0x8D000000, 0x8D800000,
        0x8E000000, 0x8E800000, 0x8F000000, 0xC7800000};

    constexpr uint16_t offset_table[64] = {
        0,    1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
        1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
        1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 0,
        1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
        1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
        1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024};

    alignas(std::max(alignof(uint16_t), alignof(native_half_t)))
        native_half_t _value = value;
    uint16_t value_bits      = *reinterpret_cast<uint16_t*>(&_value);

    alignas(std::max(alignof(uint32_t), alignof(float))) uint32_t bits =
        mantissa_table[offset_table[value_bits >> 10] + (value_bits & 0x3FF)] +
        exponent_table[value_bits >> 10];
    return *reinterpret_cast<float*>(&bits);
}

#endif  // __CUDACC_RTC__

template<typename T, std::float_round_style R = std::round_to_nearest>
#ifdef __CUDA_ARCH__
AF_CONSTEXPR
#endif
    __DH__ native_half_t
    float2half(T val) {
    return float2half_impl<R>(val);
}

__DH__ inline float half2float(native_half_t value) noexcept {
    return half2float_impl(value);
}

#ifndef __CUDACC_RTC__
template<typename T, std::float_round_style R = std::round_to_nearest,
         typename std::enable_if_t<std::is_integral<T>::value &&
                                   std::is_signed<T>::value>* = nullptr>
AF_CONSTEXPR __DH__ native_half_t int2half(T value) noexcept {
    native_half_t out = (value < 0) ? int2half_impl<R, true, T>(value)
                                    : int2half_impl<R, false, T>(value);
    return out;
}
#endif

template<typename T, std::float_round_style R = std::round_to_nearest
#ifndef __CUDACC_RTC__
         ,
         typename std::enable_if_t<std::is_integral<T>::value &&
                                   std::is_unsigned<T>::value>* = nullptr
#endif
         >
AF_CONSTEXPR __DH__ native_half_t int2half(T value) noexcept {
#if defined(__CUDACC_RTC__)
    return int2half_impl(value);
#else
    return int2half_impl<R, false, T>(value);
#endif
}

/// Convert half-precision floating point to integer.
///
/// \tparam R rounding mode to use, `std::round_indeterminate` for fastest
///           rounding
/// \tparam E `true` for round to even, `false` for round away from
///           zero
/// \tparam T type to convert to (buitlin integer type with at least 16
///           bits precision, excluding any implicit sign bits) \param value
///           binary representation of half-precision value \return integral
///           value
/// \param value The value to convert to integer
template<std::float_round_style R, bool E, typename T>
AF_CONSTEXPR T half2int(native_half_t value) {
#ifdef __CUDA_ARCH__
    AF_IF_CONSTEXPR(std::is_same<T, short>::value ||
                    std::is_same<T, char>::value ||
                    std::is_same<T, signed char>::value ||
                    std::is_same<T, unsigned char>::value) {
        return __half2short_rn(value);
    }
    else AF_IF_CONSTEXPR(std::is_same<T, unsigned short>::value) {
        return __half2ushort_rn(value);
    }
    else AF_IF_CONSTEXPR(std::is_same<T, long long>::value) {
        return __half2ll_rn(value);
    }
    else AF_IF_CONSTEXPR(std::is_same<T, unsigned long long>::value) {
        return __half2ull_rn(value);
    }
    else AF_IF_CONSTEXPR(std::is_same<T, int>::value) {
        return __half2int_rn(value);
    }
    else {
        return __half2uint_rn(value);
    }
#elif defined(AF_ONEAPI)
    return static_cast<T>(value);
#else
    static_assert(std::is_integral<T>::value,
                  "half to int conversion only supports builtin integer types");
    unsigned int e = value & 0x7FFF;
    if (e >= 0x7C00)
        return (value & 0x8000) ? std::numeric_limits<T>::min()
                                : std::numeric_limits<T>::max();
    if (e < 0x3800) {
        AF_IF_CONSTEXPR(R == std::round_toward_infinity)
        return T(~(value >> 15) & (e != 0));
        else AF_IF_CONSTEXPR(R == std::round_toward_neg_infinity) return -T(
            value > 0x8000);
        return T();
    }
    unsigned int m = (value & 0x3FF) | 0x400;
    e >>= 10;
    if (e < 25) {
        AF_IF_CONSTEXPR(R == std::round_to_nearest)
        m += (1 << (24 - e)) - (~(m >> (25 - e)) & E);
        else AF_IF_CONSTEXPR(R == std::round_toward_infinity) m +=
            ((value >> 15) - 1) & ((1 << (25 - e)) - 1U);
        else AF_IF_CONSTEXPR(R == std::round_toward_neg_infinity) m +=
            -(value >> 15) & ((1 << (25 - e)) - 1U);
        m >>= 25 - e;
    } else
        m <<= e - 25;
    return (value & 0x8000) ? -static_cast<T>(m) : static_cast<T>(m);
#endif
}

namespace internal {
/// Tag type for binary construction.
struct binary_t {};

/// Tag for binary construction.
static constexpr binary_t binary = binary_t{};
}  // namespace internal

class half;

AF_CONSTEXPR __DH__ static inline bool operator==(
    arrayfire::common::half lhs, arrayfire::common::half rhs) noexcept;
AF_CONSTEXPR __DH__ static inline bool operator!=(
    arrayfire::common::half lhs, arrayfire::common::half rhs) noexcept;

__DH__ static inline bool operator<(arrayfire::common::half lhs,
                                    arrayfire::common::half rhs) noexcept;
__DH__ static inline bool operator<(arrayfire::common::half lhs,
                                    float rhs) noexcept;

AF_CONSTEXPR __DH__ static inline bool isinf(half val) noexcept;

/// Classification implementation.
/// \param arg value to classify
/// \retval true if not a number
/// \retval false else
AF_CONSTEXPR __DH__ static inline bool isnan(
    arrayfire::common::half val) noexcept;

class alignas(2) half {
    native_half_t data_ = native_half_t();

#if !defined(__NVCC__) && !defined(__CUDACC_RTC__)
    // NVCC on OSX performs a weird transformation where it removes the std::
    // namespace and complains that the std:: namespace is not there
    friend class std::numeric_limits<half>;
    friend struct std::hash<half>;
#endif

   public:
    AF_CONSTEXPR
    half() = default;

    /// Constructor.
    /// \param bits binary representation to set half to
    AF_CONSTEXPR __DH__ half(internal::binary_t, uint16_t bits) noexcept
        :
#if defined(__CUDA_ARCH__)
        data_(__ushort_as_half(bits))
#else
        data_(bits)
#endif
    {
#ifndef __CUDACC_RTC__
        static_assert(std::is_standard_layout<half>::value,
                      "half must be a standard layout type");
        static_assert(std::is_nothrow_move_assignable<half>::value,
                      "half is not move assignable");
        static_assert(std::is_nothrow_move_constructible<half>::value,
                      "half is not move constructible");
#endif
    }

    __DH__ explicit half(double value) noexcept
        : data_(float2half<double>(value)) {}

#if defined(__CUDA_ARCH__)
    AF_CONSTEXPR
#endif
    __DH__ explicit half(float value) noexcept
        : data_(float2half<float>(value)) {}

    template<typename T>
    AF_CONSTEXPR __DH__ explicit half(T value) noexcept
        : data_(int2half(value)) {}

#if defined(__CUDA_ARCH__)
    AF_CONSTEXPR
#endif
    __DH__ half& operator=(const double& value) noexcept {
        data_ = float2half(value);
        return *this;
    }

#if defined(__CUDA_ARCH__) || defined(AF_ONEAPI)
    AF_CONSTEXPR __DH__ explicit half(native_half_t value) noexcept
        : data_(value) {}

    AF_CONSTEXPR __DH__ half& operator=(native_half_t value) noexcept {
        // NOTE Assignment to ushort from native_half_t only works with device
        // code. using memcpy instead
        data_ = value;
        return *this;
    }
#endif

    __DH__ explicit operator float() const noexcept {
        return half2float(data_);
    }

    __DH__ explicit operator double() const noexcept {
        // TODO(umar): convert directly to double
        return half2float(data_);
    }

    AF_CONSTEXPR __DH__ explicit operator short() const noexcept {
        return half2int<std::round_indeterminate, true, short>(data_);
    }

    AF_CONSTEXPR __DH__ explicit operator long long() const noexcept {
        return half2int<std::round_indeterminate, true, long long int>(data_);
    }

    AF_CONSTEXPR __DH__ explicit operator int() const noexcept {
        return half2int<std::round_indeterminate, true, int>(data_);
    }

    AF_CONSTEXPR __DH__ explicit operator unsigned() const noexcept {
        return half2int<std::round_indeterminate, true, unsigned>(data_);
    }

    AF_CONSTEXPR __DH__ explicit operator unsigned short() const noexcept {
        return half2int<std::round_indeterminate, true, unsigned>(data_);
    }

    AF_CONSTEXPR __DH__ explicit operator unsigned long long() const noexcept {
        return half2int<std::round_indeterminate, true, unsigned>(data_);
    }

    AF_CONSTEXPR __DH__ explicit operator char() const noexcept {
        return half2int<std::round_indeterminate, true, char>(data_);
    }

    AF_CONSTEXPR __DH__ explicit operator signed char() const noexcept {
        return half2int<std::round_indeterminate, true, signed char>(data_);
    }

    AF_CONSTEXPR __DH__ explicit operator unsigned char() const noexcept {
        return half2int<std::round_indeterminate, true, unsigned char>(data_);
    }

#if defined(__CUDA_ARCH__) || defined(AF_ONEAPI)
    AF_CONSTEXPR __DH__ operator native_half_t() const noexcept {
        return data_;
    };
#endif

    friend AF_CONSTEXPR __DH__ bool operator==(half lhs, half rhs) noexcept;
    friend AF_CONSTEXPR __DH__ bool operator!=(half lhs, half rhs) noexcept;
    friend __DH__ bool operator<(arrayfire::common::half lhs,
                                 arrayfire::common::half rhs) noexcept;
    friend __DH__ bool operator<(arrayfire::common::half lhs,
                                 float rhs) noexcept;

    friend AF_CONSTEXPR __DH__ bool isinf(half val) noexcept;
    friend AF_CONSTEXPR __DH__ inline bool isnan(half val) noexcept;

    AF_CONSTEXPR __DH__ arrayfire::common::half operator-() const {
#if __CUDA_ARCH__ >= 530
        return arrayfire::common::half(__hneg(data_));
#elif defined(__CUDA_ARCH__)
        return arrayfire::common::half(-(__half2float(data_)));
#elif defined(AF_ONEAPI)
        return arrayfire::common::half(-data_);
#else
        return arrayfire::common::half(internal::binary, data_ ^ 0x8000);
#endif
    }

    AF_CONSTEXPR __DH__ arrayfire::common::half operator+() const {
        return *this;
    }

    AF_CONSTEXPR static half infinity() {
        half out;
#ifdef __CUDA_ARCH__
        out.data_ = __half_raw{0x7C00};
#elif defined(AF_ONEAPI)
        out.data_ = std::numeric_limits<sycl::half>::infinity();
#else
        out.data_ = 0x7C00;
#endif
        return out;
    }
};

AF_CONSTEXPR __DH__ static inline bool operator==(
    arrayfire::common::half lhs, arrayfire::common::half rhs) noexcept {
#if __CUDA_ARCH__ >= 530
    return __heq(lhs.data_, rhs.data_);
#elif defined(__CUDA_ARCH__)
    return __half2float(lhs.data_) == __half2float(rhs.data_);
#elif defined(AF_ONEAPI)
    return lhs.data_ == rhs.data_;
#else
    return (lhs.data_ == rhs.data_ || !((lhs.data_ | rhs.data_) & 0x7FFF)) &&
           !isnan(lhs);
#endif
}

AF_CONSTEXPR __DH__ static inline bool operator!=(
    arrayfire::common::half lhs, arrayfire::common::half rhs) noexcept {
#if __CUDA_ARCH__ >= 530
    return __hne(lhs.data_, rhs.data_);
#else
    return !(lhs == rhs);
#endif
}

__DH__ static inline bool operator<(arrayfire::common::half lhs,
                                    arrayfire::common::half rhs) noexcept {
#if __CUDA_ARCH__ >= 530
    return __hlt(lhs.data_, rhs.data_);
#elif defined(__CUDA_ARCH__)
    return __half2float(lhs.data_) < __half2float(rhs.data_);
#elif defined(AF_ONEAPI)
    return lhs.data_ < rhs.data_;
#else
    int xabs = lhs.data_ & 0x7FFF, yabs = rhs.data_ & 0x7FFF;
    return xabs <= 0x7C00 && yabs <= 0x7C00 &&
           (((xabs == lhs.data_) ? xabs : -xabs) <
            ((yabs == rhs.data_) ? yabs : -yabs));
#endif
}

__DH__ static inline bool operator<(arrayfire::common::half lhs,
                                    float rhs) noexcept {
#if defined(__CUDA_ARCH__)
    return __half2float(lhs.data_) < rhs;
#elif defined(AF_ONEAPI)
    return lhs.data_ < rhs;
#else
    return static_cast<float>(lhs) < rhs;
#endif
}

#ifndef __CUDA_ARCH__
std::ostream& operator<<(std::ostream& os, const half& val);

static inline std::string to_string(const half& val) {
    return std::to_string(static_cast<float>(val));
}

static inline std::string to_string(const half&& val) {
    return std::to_string(static_cast<float>(val));
}
#endif

}  // namespace common
}  // namespace arrayfire

#if !defined(__NVCC__) && !defined(__CUDACC_RTC__)
// #endif
/// Extensions to the C++ standard library.
namespace std {
/// Numeric limits for half-precision floats.
/// Because of the underlying single-precision implementation of many
/// operations, it inherits some properties from `std::numeric_limits<float>`.
template<>
class numeric_limits<arrayfire::common::half> : public numeric_limits<float> {
   public:
    /// Supports signed values.
    static constexpr bool is_signed = true;

    /// Is not exact.
    static constexpr bool is_exact = false;

    /// Doesn't provide modulo arithmetic.
    static constexpr bool is_modulo = false;

    /// IEEE conformant.
    static constexpr bool is_iec559 = true;

    /// Supports infinity.
    static constexpr bool has_infinity = true;

    /// Supports quiet NaNs.
    static constexpr bool has_quiet_NaN = true;

    /// Supports subnormal values.
    static constexpr float_denorm_style has_denorm = denorm_present;

    /// Rounding mode.
    /// Due to the mix of internal single-precision computations (using the
    /// rounding mode of the underlying single-precision implementation) with
    /// the rounding mode of the single-to-half conversions, the actual rounding
    /// mode might be `std::round_indeterminate` if the default half-precision
    /// rounding mode doesn't match the single-precision rounding mode.
    static constexpr float_round_style round_style =
        std::numeric_limits<float>::round_style;

    /// Significant digits.
    static constexpr int digits = 11;

    /// Significant decimal digits.
    static constexpr int digits10 = 3;

    /// Required decimal digits to represent all possible values.
    static constexpr int max_digits10 = 5;

    /// Number base.
    static constexpr int radix = 2;

    /// One more than smallest exponent.
    static constexpr int min_exponent = -13;

    /// Smallest normalized representable power of 10.
    static constexpr int min_exponent10 = -4;

    /// One more than largest exponent
    static constexpr int max_exponent = 16;

    /// Largest finitely representable power of 10.
    static constexpr int max_exponent10 = 4;

    /// Smallest positive normal value.
    static AF_CONSTEXPR __DH__ arrayfire::common::half min() noexcept {
        return arrayfire::common::half(arrayfire::common::internal::binary,
                                       0x0400);
    }

    /// Smallest finite value.
    static AF_CONSTEXPR __DH__ arrayfire::common::half lowest() noexcept {
        return arrayfire::common::half(arrayfire::common::internal::binary,
                                       0xFBFF);
    }

    /// Largest finite value.
    static AF_CONSTEXPR __DH__ arrayfire::common::half max() noexcept {
        return arrayfire::common::half(arrayfire::common::internal::binary,
                                       0x7BFF);
    }

    /// Difference between one and next representable value.
    static AF_CONSTEXPR __DH__ arrayfire::common::half epsilon() noexcept {
        return arrayfire::common::half(arrayfire::common::internal::binary,
                                       0x1400);
    }

    /// Maximum rounding error.
    static AF_CONSTEXPR __DH__ arrayfire::common::half round_error() noexcept {
        return arrayfire::common::half(
            arrayfire::common::internal::binary,
            (round_style == std::round_to_nearest) ? 0x3800 : 0x3C00);
    }

    /// Positive infinity.
    static AF_CONSTEXPR __DH__ arrayfire::common::half infinity() noexcept {
        return arrayfire::common::half(arrayfire::common::internal::binary,
                                       0x7C00);
    }

    /// Quiet NaN.
    static AF_CONSTEXPR __DH__ arrayfire::common::half quiet_NaN() noexcept {
        return arrayfire::common::half(arrayfire::common::internal::binary,
                                       0x7FFF);
    }

    /// Signalling NaN.
    static AF_CONSTEXPR __DH__ arrayfire::common::half
    signaling_NaN() noexcept {
        return arrayfire::common::half(arrayfire::common::internal::binary,
                                       0x7DFF);
    }

    /// Smallest positive subnormal value.
    static AF_CONSTEXPR __DH__ arrayfire::common::half denorm_min() noexcept {
        return arrayfire::common::half(arrayfire::common::internal::binary,
                                       0x0001);
    }
};

/// Hash function for half-precision floats.
/// This is only defined if C++11 `std::hash` is supported and enabled.
template<>
struct hash<
    arrayfire::common::half>  //: unary_function<arrayfire::common::half,size_t>
{
    /// Type of function argument.
    typedef arrayfire::common::half argument_type;

    /// Function return type.
    typedef size_t result_type;

    /// Compute hash function.
    /// \param arg half to hash
    /// \return hash value
    result_type operator()(argument_type arg) const {
        return std::hash<uint16_t>()(
            static_cast<unsigned>(arg.data_) &
            -(*reinterpret_cast<uint16_t*>(&arg.data_) != 0x8000));
    }
};

}  // namespace std
#endif

namespace arrayfire {
namespace common {
AF_CONSTEXPR __DH__ static bool isinf(half val) noexcept {
#if __CUDA_ARCH__ >= 530
    return __hisinf(val.data_);
#elif defined(__CUDA_ARCH__)
    return ::isinf(__half2float(val));
#else
    return val == half::infinity() || val == -half::infinity();
#endif
}

AF_CONSTEXPR __DH__ static inline bool isnan(half val) noexcept {
#if __CUDA_ARCH__ >= 530
    return __hisnan(val.data_);
#elif defined(__CUDA_ARCH__)
    return ::isnan(__half2float(val));
#elif defined(AF_ONEAPI)
    return std::isnan(val.data_);
#else
    return (val.data_ & 0x7FFF) > 0x7C00;
#endif
}

}  // namespace common
}  // namespace arrayfire
