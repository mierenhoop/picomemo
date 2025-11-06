#ifdef __x86_64__
#define HACL_CAN_COMPILE_INTRINSICS 1
#endif
#define HACL_CAN_COMPILE_UINT128 1

#define KRML_HOST_PRINTF(...) (void)0
#define KRML_HOST_EPRINTF(...) (void)0

// Start of amalgamation

/* Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
   Licensed under the Apache 2.0 and MIT Licenses. */

#ifndef KRML_COMPAT_H
#define KRML_COMPAT_H

#include <inttypes.h>

/* A series of macros that define C implementations of types that are not Low*,
 * to facilitate porting programs to Low*. */

typedef struct {
  uint32_t length;
  const char *data;
} FStar_Bytes_bytes;

typedef int32_t Prims_pos, Prims_nat, Prims_nonzero, Prims_int,
    krml_checked_int_t;

#define RETURN_OR(x)                                                           \
  do {                                                                         \
    int64_t __ret = x;                                                         \
    if (__ret < INT32_MIN || INT32_MAX < __ret) {                              \
      KRML_HOST_PRINTF(                                                        \
          "Prims.{int,nat,pos} integer overflow at %s:%d\n", __FILE__,         \
          __LINE__);                                                           \
      KRML_HOST_EXIT(252);                                                     \
    }                                                                          \
    return (int32_t)__ret;                                                     \
  } while (0)

#endif
/* Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
   Licensed under the Apache 2.0 and MIT Licenses. */

#ifndef __KRML_TARGET_H
#define __KRML_TARGET_H

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* Since KaRaMeL emits the inline keyword unconditionally, we follow the
 * guidelines at https://gcc.gnu.org/onlinedocs/gcc/Inline.html and make this
 * __inline__ to ensure the code compiles with -std=c90 and earlier. */
#ifdef __GNUC__
#  define inline __inline__
#endif

/* There is no support for aligned_alloc() in macOS before Catalina, so
 * let's make a macro to use _mm_malloc() and _mm_free() functions
 * from mm_malloc.h. */
#if defined(__APPLE__) && defined(__MACH__)
#  include <AvailabilityMacros.h>
#  if defined(MAC_OS_X_VERSION_MIN_REQUIRED) &&                                \
   (MAC_OS_X_VERSION_MIN_REQUIRED < 101500)
#    include <mm_malloc.h>
#    define LEGACY_MACOS
#  else
#    undef LEGACY_MACOS
#endif
#endif

/******************************************************************************/
/* Macros that KaRaMeL will generate.                                         */
/******************************************************************************/

/* For "bare" targets that do not have a C stdlib, the user might want to use
 * [-add-early-include '"mydefinitions.h"'] and override these. */
#ifndef KRML_HOST_PRINTF
#  define KRML_HOST_PRINTF printf
#endif

#if (                                                                          \
    (defined __STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) &&             \
    (!(defined KRML_HOST_EPRINTF)))
#  define KRML_HOST_EPRINTF(...) fprintf(stderr, __VA_ARGS__)
#elif !(defined KRML_HOST_EPRINTF) && defined(_MSC_VER)
#  define KRML_HOST_EPRINTF(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef KRML_HOST_EXIT
#  define KRML_HOST_EXIT exit
#endif

#ifndef KRML_HOST_MALLOC
#  define KRML_HOST_MALLOC malloc
#endif

#ifndef KRML_HOST_CALLOC
#  define KRML_HOST_CALLOC calloc
#endif

#ifndef KRML_HOST_FREE
#  define KRML_HOST_FREE free
#endif

#ifndef KRML_HOST_IGNORE
#  define KRML_HOST_IGNORE(x) (void)(x)
#endif

#ifndef KRML_MAYBE_UNUSED_VAR
#  define KRML_MAYBE_UNUSED_VAR(x) KRML_HOST_IGNORE(x)
#endif

#ifndef KRML_MAYBE_UNUSED
#  if defined(__GNUC__) || defined(__clang__)
#    define KRML_MAYBE_UNUSED __attribute__((unused))
#  else
#    define KRML_MAYBE_UNUSED
#  endif
#endif

#ifndef KRML_ATTRIBUTE_TARGET
#  if defined(__GNUC__) || defined(__clang__)
#    define KRML_ATTRIBUTE_TARGET(x) __attribute__((target(x)))
#  else
#    define KRML_ATTRIBUTE_TARGET(x)
#  endif
#endif

#ifndef KRML_NOINLINE
#  if defined (__GNUC__) || defined (__clang__)
#    define KRML_NOINLINE __attribute__((noinline,unused))
#  elif defined(_MSC_VER)
#    define KRML_NOINLINE __declspec(noinline)
#  elif defined (__SUNPRO_C)
#    define KRML_NOINLINE __attribute__((noinline))
#  else
#    define KRML_NOINLINE
#    warning "The KRML_NOINLINE macro is not defined for this toolchain!"
#    warning "The compiler may defeat side-channel resistance with optimizations."
#    warning "Please locate target.h and try to fill it out with a suitable definition for this compiler."
#  endif
#endif

#ifndef KRML_MUSTINLINE
#  if defined(_MSC_VER)
#    define KRML_MUSTINLINE inline __forceinline
#  elif defined (__GNUC__)
#    define KRML_MUSTINLINE inline __attribute__((always_inline))
#  elif defined (__SUNPRO_C)
#    define KRML_MUSTINLINE inline __attribute__((always_inline))
#  else
#    define KRML_MUSTINLINE inline
#    warning "The KRML_MUSTINLINE macro defaults to plain inline for this toolchain!"
#    warning "Please locate target.h and try to fill it out with a suitable definition for this compiler."
#  endif
#endif

#ifndef KRML_PRE_ALIGN
#  ifdef _MSC_VER
#    define KRML_PRE_ALIGN(X) __declspec(align(X))
#  else
#    define KRML_PRE_ALIGN(X)
#  endif
#endif

#ifndef KRML_POST_ALIGN
#  ifdef _MSC_VER
#    define KRML_POST_ALIGN(X)
#  else
#    define KRML_POST_ALIGN(X) __attribute__((aligned(X)))
#  endif
#endif

/* MinGW-W64 does not support C11 aligned_alloc, but it supports
 * MSVC's _aligned_malloc.
 */
#ifndef KRML_ALIGNED_MALLOC
#  ifdef __MINGW32__
#    include <_mingw.h>
#  endif
#  if (                                                                        \
      defined(_MSC_VER) ||                                                     \
      (defined(__MINGW32__) && defined(__MINGW64_VERSION_MAJOR)))
#    define KRML_ALIGNED_MALLOC(X, Y) _aligned_malloc(Y, X)
#  elif defined(LEGACY_MACOS)
#    define KRML_ALIGNED_MALLOC(X, Y) _mm_malloc(Y, X)
#  else
#    define KRML_ALIGNED_MALLOC(X, Y) aligned_alloc(X, Y)
#  endif
#endif

/* Since aligned allocations with MinGW-W64 are done with
 * _aligned_malloc (see above), such pointers must be freed with
 * _aligned_free.
 */
#ifndef KRML_ALIGNED_FREE
#  ifdef __MINGW32__
#    include <_mingw.h>
#  endif
#  if (                                                                        \
      defined(_MSC_VER) ||                                                     \
      (defined(__MINGW32__) && defined(__MINGW64_VERSION_MAJOR)))
#    define KRML_ALIGNED_FREE(X) _aligned_free(X)
#  elif defined(LEGACY_MACOS)
#    define KRML_ALIGNED_FREE(X) _mm_free(X)
#  else
#    define KRML_ALIGNED_FREE(X) free(X)
#  endif
#endif

#ifndef KRML_HOST_TIME

#  include <time.h>

/* Prims_nat not yet in scope */
inline static int32_t krml_time(void) {
  return (int32_t)time(NULL);
}

#  define KRML_HOST_TIME krml_time
#endif

/* In statement position, exiting is easy. */
#define KRML_EXIT                                                              \
  do {                                                                         \
    KRML_HOST_PRINTF("Unimplemented function at %s:%d\n", __FILE__, __LINE__); \
    KRML_HOST_EXIT(254);                                                       \
  } while (0)

/* In expression position, use the comma-operator and a malloc to return an
 * expression of the right size. KaRaMeL passes t as the parameter to the macro.
 */
#define KRML_EABORT(t, msg)                                                    \
  (KRML_HOST_PRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__, msg),  \
   KRML_HOST_EXIT(255), *((t *)KRML_HOST_MALLOC(sizeof(t))))

/* In FStar.Buffer.fst, the size of arrays is uint32_t, but it's a number of
 * *elements*. Do an ugly, run-time check (some of which KaRaMeL can eliminate).
 */
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 4))
#  define _KRML_CHECK_SIZE_PRAGMA                                              \
    _Pragma("GCC diagnostic ignored \"-Wtype-limits\"")
#else
#  define _KRML_CHECK_SIZE_PRAGMA
#endif

#define KRML_CHECK_SIZE(size_elt, sz)                                          \
  do {                                                                         \
    _KRML_CHECK_SIZE_PRAGMA                                                    \
    if (((size_t)(sz)) > ((size_t)(SIZE_MAX / (size_elt)))) {                  \
      KRML_HOST_PRINTF(                                                        \
          "Maximum allocatable size exceeded, aborting before overflow at "    \
          "%s:%d\n",                                                           \
          __FILE__, __LINE__);                                                 \
      KRML_HOST_EXIT(253);                                                     \
    }                                                                          \
  } while (0)

#if defined(_MSC_VER) && _MSC_VER < 1900
#  define KRML_HOST_SNPRINTF(buf, sz, fmt, arg)                                \
    _snprintf_s(buf, sz, _TRUNCATE, fmt, arg)
#else
#  define KRML_HOST_SNPRINTF(buf, sz, fmt, arg) snprintf(buf, sz, fmt, arg)
#endif

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 4))
#  define KRML_DEPRECATED(x) __attribute__((deprecated(x)))
#elif defined(__GNUC__)
/* deprecated attribute is not defined in GCC < 4.5. */
#  define KRML_DEPRECATED(x)
#elif defined(__SUNPRO_C)
#  define KRML_DEPRECATED(x) __attribute__((deprecated(x)))
#elif defined(_MSC_VER)
#  define KRML_DEPRECATED(x) __declspec(deprecated(x))
#endif

/* Macros for prettier unrolling of loops */
#define KRML_LOOP1(i, n, x) { \
  x \
  i += n; \
  (void) i; \
}

#define KRML_LOOP2(i, n, x)                                                    \
  KRML_LOOP1(i, n, x)                                                          \
  KRML_LOOP1(i, n, x)

#define KRML_LOOP3(i, n, x)                                                    \
  KRML_LOOP2(i, n, x)                                                          \
  KRML_LOOP1(i, n, x)

#define KRML_LOOP4(i, n, x)                                                    \
  KRML_LOOP2(i, n, x)                                                          \
  KRML_LOOP2(i, n, x)

#define KRML_LOOP5(i, n, x)                                                    \
  KRML_LOOP4(i, n, x)                                                          \
  KRML_LOOP1(i, n, x)

#define KRML_LOOP6(i, n, x)                                                    \
  KRML_LOOP4(i, n, x)                                                          \
  KRML_LOOP2(i, n, x)

#define KRML_LOOP7(i, n, x)                                                    \
  KRML_LOOP4(i, n, x)                                                          \
  KRML_LOOP3(i, n, x)

#define KRML_LOOP8(i, n, x)                                                    \
  KRML_LOOP4(i, n, x)                                                          \
  KRML_LOOP4(i, n, x)

#define KRML_LOOP9(i, n, x)                                                    \
  KRML_LOOP8(i, n, x)                                                          \
  KRML_LOOP1(i, n, x)

#define KRML_LOOP10(i, n, x)                                                   \
  KRML_LOOP8(i, n, x)                                                          \
  KRML_LOOP2(i, n, x)

#define KRML_LOOP11(i, n, x)                                                   \
  KRML_LOOP8(i, n, x)                                                          \
  KRML_LOOP3(i, n, x)

#define KRML_LOOP12(i, n, x)                                                   \
  KRML_LOOP8(i, n, x)                                                          \
  KRML_LOOP4(i, n, x)

#define KRML_LOOP13(i, n, x)                                                   \
  KRML_LOOP8(i, n, x)                                                          \
  KRML_LOOP5(i, n, x)

#define KRML_LOOP14(i, n, x)                                                   \
  KRML_LOOP8(i, n, x)                                                          \
  KRML_LOOP6(i, n, x)

#define KRML_LOOP15(i, n, x)                                                   \
  KRML_LOOP8(i, n, x)                                                          \
  KRML_LOOP7(i, n, x)

#define KRML_LOOP16(i, n, x)                                                   \
  KRML_LOOP8(i, n, x)                                                          \
  KRML_LOOP8(i, n, x)

#define KRML_UNROLL_FOR(i, z, n, k, x)                                         \
  do {                                                                         \
    uint32_t i = z;                                                            \
    KRML_LOOP##n(i, k, x)                                                      \
  } while (0)

#define KRML_ACTUAL_FOR(i, z, n, k, x)                                         \
  do {                                                                         \
    for (uint32_t i = z; i < n; i += k) {                                      \
      x                                                                        \
    }                                                                          \
  } while (0)

#ifndef KRML_UNROLL_MAX
#  define KRML_UNROLL_MAX 16
#endif

/* 1 is the number of loop iterations, i.e. (n - z)/k as evaluated by krml */
#if 0 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR0(i, z, n, k, x)
#else
#  define KRML_MAYBE_FOR0(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 1 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR1(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 1, k, x)
#else
#  define KRML_MAYBE_FOR1(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 2 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR2(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 2, k, x)
#else
#  define KRML_MAYBE_FOR2(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 3 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR3(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 3, k, x)
#else
#  define KRML_MAYBE_FOR3(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 4 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR4(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 4, k, x)
#else
#  define KRML_MAYBE_FOR4(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 5 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR5(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 5, k, x)
#else
#  define KRML_MAYBE_FOR5(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 6 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR6(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 6, k, x)
#else
#  define KRML_MAYBE_FOR6(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 7 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR7(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 7, k, x)
#else
#  define KRML_MAYBE_FOR7(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 8 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR8(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 8, k, x)
#else
#  define KRML_MAYBE_FOR8(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 9 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR9(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 9, k, x)
#else
#  define KRML_MAYBE_FOR9(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 10 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR10(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 10, k, x)
#else
#  define KRML_MAYBE_FOR10(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 11 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR11(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 11, k, x)
#else
#  define KRML_MAYBE_FOR11(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 12 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR12(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 12, k, x)
#else
#  define KRML_MAYBE_FOR12(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 13 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR13(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 13, k, x)
#else
#  define KRML_MAYBE_FOR13(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 14 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR14(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 14, k, x)
#else
#  define KRML_MAYBE_FOR14(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 15 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR15(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 15, k, x)
#else
#  define KRML_MAYBE_FOR15(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif

#if 16 <= KRML_UNROLL_MAX
#  define KRML_MAYBE_FOR16(i, z, n, k, x) KRML_UNROLL_FOR(i, z, 16, k, x)
#else
#  define KRML_MAYBE_FOR16(i, z, n, k, x) KRML_ACTUAL_FOR(i, z, n, k, x)
#endif
#endif
/* Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
   Licensed under the Apache 2.0 and MIT Licenses. */

#ifndef KRML_TYPES_H
#define KRML_TYPES_H

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Types which are either abstract, meaning that have to be implemented in C, or
 * which are models, meaning that they are swapped out at compile-time for
 * hand-written C types (in which case they're marked as noextract). */

typedef uint64_t FStar_UInt64_t, FStar_UInt64_t_;
typedef int64_t FStar_Int64_t, FStar_Int64_t_;
typedef uint32_t FStar_UInt32_t, FStar_UInt32_t_;
typedef int32_t FStar_Int32_t, FStar_Int32_t_;
typedef uint16_t FStar_UInt16_t, FStar_UInt16_t_;
typedef int16_t FStar_Int16_t, FStar_Int16_t_;
typedef uint8_t FStar_UInt8_t, FStar_UInt8_t_;
typedef int8_t FStar_Int8_t, FStar_Int8_t_;

/* Only useful when building krmllib, because it's in the dependency graph of
 * FStar.Int.Cast. */
typedef uint64_t FStar_UInt63_t, FStar_UInt63_t_;
typedef int64_t FStar_Int63_t, FStar_Int63_t_;

typedef double FStar_Float_float;
typedef uint32_t FStar_Char_char;
typedef FILE *FStar_IO_fd_read, *FStar_IO_fd_write;

typedef void *FStar_Dyn_dyn;

typedef const char *C_String_t, *C_String_t_, *C_Compat_String_t, *C_Compat_String_t_;

typedef int exit_code;
typedef FILE *channel;

typedef unsigned long long TestLib_cycles;

typedef uint64_t FStar_Date_dateTime, FStar_Date_timeSpan;

/* Now Prims.string is no longer illegal with the new model in LowStar.Printf;
 * it's operations that produce Prims_string which are illegal. Bring the
 * definition into scope by default. */
typedef const char *Prims_string;

#if (defined(_MSC_VER) && defined(_M_X64) && !defined(__clang__))
#define IS_MSVC64 1
#endif

/* This code makes a number of assumptions and should be refined. In particular,
 * it assumes that: any non-MSVC amd64 compiler supports int128. Maybe it would
 * be easier to just test for defined(__SIZEOF_INT128__) only? */
#if (defined(__x86_64__) || \
    defined(__x86_64) || \
    defined(__aarch64__) || \
    (defined(__powerpc64__) && defined(__LITTLE_ENDIAN__)) || \
    defined(__s390x__) || \
    (defined(_MSC_VER) && defined(_M_X64) && defined(__clang__)) || \
    (defined(__mips__) && defined(__LP64__)) || \
    (defined(__riscv) && __riscv_xlen == 64) || \
    defined(__SIZEOF_INT128__))
#define HAS_INT128 1
#endif

/* The uint128 type is a special case since we offer several implementations of
 * it, depending on the compiler and whether the user wants the verified
 * implementation or not. */
#if !defined(KRML_VERIFIED_UINT128) && defined(IS_MSVC64)
#  include <emmintrin.h>
typedef __m128i FStar_UInt128_uint128;
#elif !defined(KRML_VERIFIED_UINT128) && defined(HAS_INT128)
typedef unsigned __int128 FStar_UInt128_uint128;
#else
typedef struct FStar_UInt128_uint128_s {
  uint64_t low;
  uint64_t high;
} FStar_UInt128_uint128;
#endif

/* The former is defined once, here (otherwise, conflicts for test-c89. The
 * latter is for internal use. */
typedef FStar_UInt128_uint128 FStar_UInt128_t, uint128_t;


#endif

/* Avoid a circular loop: if this header is included via FStar_UInt8_16_32_64,
 * then don't bring the uint128 definitions into scope. */
#ifndef __FStar_UInt_8_16_32_64_H

#if !defined(KRML_VERIFIED_UINT128) && defined(IS_MSVC64)
#elif !defined(KRML_VERIFIED_UINT128) && defined(HAS_INT128)
#else
#endif

#endif
/* Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
   Licensed under the Apache 2.0 and MIT Licenses. */

#ifndef __LOWSTAR_ENDIANNESS_H
#define __LOWSTAR_ENDIANNESS_H

#include <string.h>
#include <inttypes.h>

/******************************************************************************/
/* Implementing C.fst (part 2: endian-ness macros)                            */
/******************************************************************************/

/* ... for Linux */
#if defined(__linux__) || defined(__CYGWIN__) || defined (__USE_SYSTEM_ENDIAN_H__) || defined(__GLIBC__)
#  include <endian.h>

/* ... for OSX */
#elif defined(__APPLE__)
#  include <libkern/OSByteOrder.h>
#  define htole64(x) OSSwapHostToLittleInt64(x)
#  define le64toh(x) OSSwapLittleToHostInt64(x)
#  define htobe64(x) OSSwapHostToBigInt64(x)
#  define be64toh(x) OSSwapBigToHostInt64(x)

#  define htole16(x) OSSwapHostToLittleInt16(x)
#  define le16toh(x) OSSwapLittleToHostInt16(x)
#  define htobe16(x) OSSwapHostToBigInt16(x)
#  define be16toh(x) OSSwapBigToHostInt16(x)

#  define htole32(x) OSSwapHostToLittleInt32(x)
#  define le32toh(x) OSSwapLittleToHostInt32(x)
#  define htobe32(x) OSSwapHostToBigInt32(x)
#  define be32toh(x) OSSwapBigToHostInt32(x)

/* ... for Solaris */
#elif defined(__sun__)
#  include <sys/byteorder.h>
#  define htole64(x) LE_64(x)
#  define le64toh(x) LE_64(x)
#  define htobe64(x) BE_64(x)
#  define be64toh(x) BE_64(x)

#  define htole16(x) LE_16(x)
#  define le16toh(x) LE_16(x)
#  define htobe16(x) BE_16(x)
#  define be16toh(x) BE_16(x)

#  define htole32(x) LE_32(x)
#  define le32toh(x) LE_32(x)
#  define htobe32(x) BE_32(x)
#  define be32toh(x) BE_32(x)

/* ... for the BSDs */
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#  include <sys/endian.h>
#elif defined(__OpenBSD__)
#  include <endian.h>

/* ... for Windows (MSVC)... not targeting XBOX 360! */
#elif defined(_MSC_VER)

#  include <stdlib.h>
#  define htobe16(x) _byteswap_ushort(x)
#  define htole16(x) (x)
#  define be16toh(x) _byteswap_ushort(x)
#  define le16toh(x) (x)

#  define htobe32(x) _byteswap_ulong(x)
#  define htole32(x) (x)
#  define be32toh(x) _byteswap_ulong(x)
#  define le32toh(x) (x)

#  define htobe64(x) _byteswap_uint64(x)
#  define htole64(x) (x)
#  define be64toh(x) _byteswap_uint64(x)
#  define le64toh(x) (x)

/* ... for Windows (GCC-like, e.g. mingw or clang) */
#elif (defined(_WIN32) || defined(_WIN64) || defined(__EMSCRIPTEN__)) &&       \
    (defined(__GNUC__) || defined(__clang__))

#  define htobe16(x) __builtin_bswap16(x)
#  define htole16(x) (x)
#  define be16toh(x) __builtin_bswap16(x)
#  define le16toh(x) (x)

#  define htobe32(x) __builtin_bswap32(x)
#  define htole32(x) (x)
#  define be32toh(x) __builtin_bswap32(x)
#  define le32toh(x) (x)

#  define htobe64(x) __builtin_bswap64(x)
#  define htole64(x) (x)
#  define be64toh(x) __builtin_bswap64(x)
#  define le64toh(x) (x)

/* ... generic big-endian fallback code */
/* ... AIX doesn't have __BYTE_ORDER__ (with XLC compiler) & is always big-endian */
#elif (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(_AIX)

/* byte swapping code inspired by:
 * https://github.com/rweather/arduinolibs/blob/master/libraries/Crypto/utility/EndianUtil.h
 * */

#  define htobe32(x) (x)
#  define be32toh(x) (x)
#  define htole32(x)                                                           \
    (__extension__({                                                           \
      uint32_t _temp = (x);                                                    \
      ((_temp >> 24) & 0x000000FF) | ((_temp >> 8) & 0x0000FF00) |             \
          ((_temp << 8) & 0x00FF0000) | ((_temp << 24) & 0xFF000000);          \
    }))
#  define le32toh(x) (htole32((x)))

#  define htobe64(x) (x)
#  define be64toh(x) (x)
#  define htole64(x)                                                           \
    (__extension__({                                                           \
      uint64_t __temp = (x);                                                   \
      uint32_t __low = htobe32((uint32_t)__temp);                              \
      uint32_t __high = htobe32((uint32_t)(__temp >> 32));                     \
      (((uint64_t)__low) << 32) | __high;                                      \
    }))
#  define le64toh(x) (htole64((x)))

/* ... generic little-endian fallback code */
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#  define htole32(x) (x)
#  define le32toh(x) (x)
#  define htobe32(x)                                                           \
    (__extension__({                                                           \
      uint32_t _temp = (x);                                                    \
      ((_temp >> 24) & 0x000000FF) | ((_temp >> 8) & 0x0000FF00) |             \
          ((_temp << 8) & 0x00FF0000) | ((_temp << 24) & 0xFF000000);          \
    }))
#  define be32toh(x) (htobe32((x)))

#  define htole64(x) (x)
#  define le64toh(x) (x)
#  define htobe64(x)                                                           \
    (__extension__({                                                           \
      uint64_t __temp = (x);                                                   \
      uint32_t __low = htobe32((uint32_t)__temp);                              \
      uint32_t __high = htobe32((uint32_t)(__temp >> 32));                     \
      (((uint64_t)__low) << 32) | __high;                                      \
    }))
#  define be64toh(x) (htobe64((x)))

/* ... couldn't determine endian-ness of the target platform */
#else
#  error "Please define __BYTE_ORDER__!"

#endif /* defined(__linux__) || ... */

/* Loads and stores. These avoid undefined behavior due to unaligned memory
 * accesses, via memcpy. */

inline static uint16_t load16(uint8_t *b) {
  uint16_t x;
  memcpy(&x, b, 2);
  return x;
}

inline static uint32_t load32(uint8_t *b) {
  uint32_t x;
  memcpy(&x, b, 4);
  return x;
}

inline static uint64_t load64(uint8_t *b) {
  uint64_t x;
  memcpy(&x, b, 8);
  return x;
}

inline static void store16(uint8_t *b, uint16_t i) {
  memcpy(b, &i, 2);
}

inline static void store32(uint8_t *b, uint32_t i) {
  memcpy(b, &i, 4);
}

inline static void store64(uint8_t *b, uint64_t i) {
  memcpy(b, &i, 8);
}

/* Legacy accessors so that this header can serve as an implementation of
 * C.Endianness */
#define load16_le(b) (le16toh(load16(b)))
#define store16_le(b, i) (store16(b, htole16(i)))
#define load16_be(b) (be16toh(load16(b)))
#define store16_be(b, i) (store16(b, htobe16(i)))

#define load32_le(b) (le32toh(load32(b)))
#define store32_le(b, i) (store32(b, htole32(i)))
#define load32_be(b) (be32toh(load32(b)))
#define store32_be(b, i) (store32(b, htobe32(i)))

#define load64_le(b) (le64toh(load64(b)))
#define store64_le(b, i) (store64(b, htole64(i)))
#define load64_be(b) (be64toh(load64(b)))
#define store64_be(b, i) (store64(b, htobe64(i)))

/* Co-existence of LowStar.Endianness and FStar.Endianness generates name
 * conflicts, because of course both insist on having no prefixes. Until a
 * prefix is added, or until we truly retire FStar.Endianness, solve this issue
 * in an elegant way. */
#define load16_le0 load16_le
#define store16_le0 store16_le
#define load16_be0 load16_be
#define store16_be0 store16_be

#define load32_le0 load32_le
#define store32_le0 store32_le
#define load32_be0 load32_be
#define store32_be0 store32_be

#define load64_le0 load64_le
#define store64_le0 store64_le
#define load64_be0 load64_be
#define store64_be0 store64_be

#define load128_le0 load128_le
#define store128_le0 store128_le
#define load128_be0 load128_be
#define store128_be0 store128_be

#endif
/*
  Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
  Licensed under the Apache 2.0 and MIT Licenses.
*/


#ifndef __FStar_UInt128_H
#define __FStar_UInt128_H

#include <inttypes.h>
#include <stdbool.h>

static inline FStar_UInt128_uint128
FStar_UInt128_add(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_add_underspec(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_add_mod(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_sub(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_sub_underspec(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_sub_mod(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_logand(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_logxor(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_logor(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128 FStar_UInt128_lognot(FStar_UInt128_uint128 a);

static inline FStar_UInt128_uint128
FStar_UInt128_shift_left(FStar_UInt128_uint128 a, uint32_t s);

static inline FStar_UInt128_uint128
FStar_UInt128_shift_right(FStar_UInt128_uint128 a, uint32_t s);

static inline bool FStar_UInt128_eq(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline bool FStar_UInt128_gt(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline bool FStar_UInt128_lt(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline bool FStar_UInt128_gte(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline bool FStar_UInt128_lte(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_eq_mask(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_gte_mask(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128 FStar_UInt128_uint64_to_uint128(uint64_t a);

static inline uint64_t FStar_UInt128_uint128_to_uint64(FStar_UInt128_uint128 a);

static inline FStar_UInt128_uint128 FStar_UInt128_mul32(uint64_t x, uint32_t y);

static inline FStar_UInt128_uint128 FStar_UInt128_mul_wide(uint64_t x, uint64_t y);


#define __FStar_UInt128_H_DEFINED
#endif
/*
  Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
  Licensed under the Apache 2.0 and MIT Licenses.
*/


#ifndef __LowStar_Endianness_H
#define __LowStar_Endianness_H

#include <inttypes.h>
#include <stdbool.h>

static inline void store128_le(uint8_t *x0, FStar_UInt128_uint128 x1);

static inline FStar_UInt128_uint128 load128_le(uint8_t *x0);

static inline void store128_be(uint8_t *x0, FStar_UInt128_uint128 x1);

static inline FStar_UInt128_uint128 load128_be(uint8_t *x0);


#define __LowStar_Endianness_H_DEFINED
#endif
/*
  Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
  Licensed under the Apache 2.0 and MIT Licenses.
*/


#ifndef __FStar_UInt_8_16_32_64_H
#define __FStar_UInt_8_16_32_64_H

#include <inttypes.h>
#include <stdbool.h>

extern krml_checked_int_t FStar_UInt64_n;

extern bool FStar_UInt64_uu___is_Mk(uint64_t projectee);

extern krml_checked_int_t FStar_UInt64___proj__Mk__item__v(uint64_t projectee);

extern krml_checked_int_t FStar_UInt64_v(uint64_t x);

typedef void *FStar_UInt64_fits;

extern uint64_t FStar_UInt64_uint_to_t(krml_checked_int_t x);

extern uint64_t FStar_UInt64_zero;

extern uint64_t FStar_UInt64_one;

extern uint64_t FStar_UInt64_minus(uint64_t a);

extern uint32_t FStar_UInt64_n_minus_one;

static KRML_NOINLINE uint64_t FStar_UInt64_eq_mask(uint64_t a, uint64_t b)
{
  uint64_t x = a ^ b;
  uint64_t minus_x = ~x + 1ULL;
  uint64_t x_or_minus_x = x | minus_x;
  uint64_t xnx = x_or_minus_x >> 63U;
  return xnx - 1ULL;
}

static KRML_NOINLINE uint64_t FStar_UInt64_gte_mask(uint64_t a, uint64_t b)
{
  uint64_t x = a;
  uint64_t y = b;
  uint64_t x_xor_y = x ^ y;
  uint64_t x_sub_y = x - y;
  uint64_t x_sub_y_xor_y = x_sub_y ^ y;
  uint64_t q = x_xor_y | x_sub_y_xor_y;
  uint64_t x_xor_q = x ^ q;
  uint64_t x_xor_q_ = x_xor_q >> 63U;
  return x_xor_q_ - 1ULL;
}

extern Prims_string FStar_UInt64_to_string(uint64_t uu___);

extern Prims_string FStar_UInt64_to_string_hex(uint64_t uu___);

extern Prims_string FStar_UInt64_to_string_hex_pad(uint64_t uu___);

extern uint64_t FStar_UInt64_of_string(Prims_string uu___);

extern krml_checked_int_t FStar_UInt32_n;

extern bool FStar_UInt32_uu___is_Mk(uint32_t projectee);

extern krml_checked_int_t FStar_UInt32___proj__Mk__item__v(uint32_t projectee);

extern krml_checked_int_t FStar_UInt32_v(uint32_t x);

typedef void *FStar_UInt32_fits;

extern uint32_t FStar_UInt32_uint_to_t(krml_checked_int_t x);

extern uint32_t FStar_UInt32_zero;

extern uint32_t FStar_UInt32_one;

extern uint32_t FStar_UInt32_minus(uint32_t a);

extern uint32_t FStar_UInt32_n_minus_one;

static KRML_NOINLINE uint32_t FStar_UInt32_eq_mask(uint32_t a, uint32_t b)
{
  uint32_t x = a ^ b;
  uint32_t minus_x = ~x + 1U;
  uint32_t x_or_minus_x = x | minus_x;
  uint32_t xnx = x_or_minus_x >> 31U;
  return xnx - 1U;
}

static KRML_NOINLINE uint32_t FStar_UInt32_gte_mask(uint32_t a, uint32_t b)
{
  uint32_t x = a;
  uint32_t y = b;
  uint32_t x_xor_y = x ^ y;
  uint32_t x_sub_y = x - y;
  uint32_t x_sub_y_xor_y = x_sub_y ^ y;
  uint32_t q = x_xor_y | x_sub_y_xor_y;
  uint32_t x_xor_q = x ^ q;
  uint32_t x_xor_q_ = x_xor_q >> 31U;
  return x_xor_q_ - 1U;
}

extern Prims_string FStar_UInt32_to_string(uint32_t uu___);

extern Prims_string FStar_UInt32_to_string_hex(uint32_t uu___);

extern Prims_string FStar_UInt32_to_string_hex_pad(uint32_t uu___);

extern uint32_t FStar_UInt32_of_string(Prims_string uu___);

extern krml_checked_int_t FStar_UInt16_n;

extern bool FStar_UInt16_uu___is_Mk(uint16_t projectee);

extern krml_checked_int_t FStar_UInt16___proj__Mk__item__v(uint16_t projectee);

extern krml_checked_int_t FStar_UInt16_v(uint16_t x);

typedef void *FStar_UInt16_fits;

extern uint16_t FStar_UInt16_uint_to_t(krml_checked_int_t x);

extern uint16_t FStar_UInt16_zero;

extern uint16_t FStar_UInt16_one;

extern uint16_t FStar_UInt16_minus(uint16_t a);

extern uint32_t FStar_UInt16_n_minus_one;

static KRML_NOINLINE uint16_t FStar_UInt16_eq_mask(uint16_t a, uint16_t b)
{
  uint16_t x = (uint32_t)a ^ (uint32_t)b;
  uint16_t minus_x = (uint32_t)~x + 1U;
  uint16_t x_or_minus_x = (uint32_t)x | (uint32_t)minus_x;
  uint16_t xnx = (uint32_t)x_or_minus_x >> 15U;
  return (uint32_t)xnx - 1U;
}

static KRML_NOINLINE uint16_t FStar_UInt16_gte_mask(uint16_t a, uint16_t b)
{
  uint16_t x = a;
  uint16_t y = b;
  uint16_t x_xor_y = (uint32_t)x ^ (uint32_t)y;
  uint16_t x_sub_y = (uint32_t)x - (uint32_t)y;
  uint16_t x_sub_y_xor_y = (uint32_t)x_sub_y ^ (uint32_t)y;
  uint16_t q = (uint32_t)x_xor_y | (uint32_t)x_sub_y_xor_y;
  uint16_t x_xor_q = (uint32_t)x ^ (uint32_t)q;
  uint16_t x_xor_q_ = (uint32_t)x_xor_q >> 15U;
  return (uint32_t)x_xor_q_ - 1U;
}

extern Prims_string FStar_UInt16_to_string(uint16_t uu___);

extern Prims_string FStar_UInt16_to_string_hex(uint16_t uu___);

extern Prims_string FStar_UInt16_to_string_hex_pad(uint16_t uu___);

extern uint16_t FStar_UInt16_of_string(Prims_string uu___);

extern krml_checked_int_t FStar_UInt8_n;

extern bool FStar_UInt8_uu___is_Mk(uint8_t projectee);

extern krml_checked_int_t FStar_UInt8___proj__Mk__item__v(uint8_t projectee);

extern krml_checked_int_t FStar_UInt8_v(uint8_t x);

typedef void *FStar_UInt8_fits;

extern uint8_t FStar_UInt8_uint_to_t(krml_checked_int_t x);

extern uint8_t FStar_UInt8_zero;

extern uint8_t FStar_UInt8_one;

extern uint8_t FStar_UInt8_minus(uint8_t a);

extern uint32_t FStar_UInt8_n_minus_one;

static KRML_NOINLINE uint8_t FStar_UInt8_eq_mask(uint8_t a, uint8_t b)
{
  uint8_t x = (uint32_t)a ^ (uint32_t)b;
  uint8_t minus_x = (uint32_t)~x + 1U;
  uint8_t x_or_minus_x = (uint32_t)x | (uint32_t)minus_x;
  uint8_t xnx = (uint32_t)x_or_minus_x >> 7U;
  return (uint32_t)xnx - 1U;
}

static KRML_NOINLINE uint8_t FStar_UInt8_gte_mask(uint8_t a, uint8_t b)
{
  uint8_t x = a;
  uint8_t y = b;
  uint8_t x_xor_y = (uint32_t)x ^ (uint32_t)y;
  uint8_t x_sub_y = (uint32_t)x - (uint32_t)y;
  uint8_t x_sub_y_xor_y = (uint32_t)x_sub_y ^ (uint32_t)y;
  uint8_t q = (uint32_t)x_xor_y | (uint32_t)x_sub_y_xor_y;
  uint8_t x_xor_q = (uint32_t)x ^ (uint32_t)q;
  uint8_t x_xor_q_ = (uint32_t)x_xor_q >> 7U;
  return (uint32_t)x_xor_q_ - 1U;
}

extern Prims_string FStar_UInt8_to_string(uint8_t uu___);

extern Prims_string FStar_UInt8_to_string_hex(uint8_t uu___);

extern Prims_string FStar_UInt8_to_string_hex_pad(uint8_t uu___);

extern uint8_t FStar_UInt8_of_string(Prims_string uu___);

typedef uint8_t FStar_UInt8_byte;


#define __FStar_UInt_8_16_32_64_H_DEFINED
#endif
/* Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
   Licensed under the Apache 2.0 and MIT Licenses. */

/******************************************************************************/
/* Machine integers (128-bit arithmetic)                                      */
/******************************************************************************/

/* This header contains two things.
 *
 * First, an implementation of 128-bit arithmetic suitable for 64-bit GCC and
 * Clang, i.e. all the operations from FStar.UInt128.
 *
 * Second, 128-bit operations from C.Endianness (or LowStar.Endianness),
 * suitable for any compiler and platform (via a series of ifdefs). This second
 * part is unfortunate, and should be fixed by moving {load,store}128_{be,le} to
 * FStar.UInt128 to avoid a maze of preprocessor guards and hand-written code.
 * */

/* This file is used for both the minimal and generic krmllib distributions. As
 * such, it assumes that the machine integers have been bundled the exact same
 * way in both cases. */

#ifndef FSTAR_UINT128_GCC64
#define FSTAR_UINT128_GCC64


/* GCC + using native unsigned __int128 support */

inline static uint128_t load128_le(uint8_t *b) {
  uint128_t l = (uint128_t)load64_le(b);
  uint128_t h = (uint128_t)load64_le(b + 8);
  return (h << 64 | l);
}

inline static void store128_le(uint8_t *b, uint128_t n) {
  store64_le(b, (uint64_t)n);
  store64_le(b + 8, (uint64_t)(n >> 64));
}

inline static uint128_t load128_be(uint8_t *b) {
  uint128_t h = (uint128_t)load64_be(b);
  uint128_t l = (uint128_t)load64_be(b + 8);
  return (h << 64 | l);
}

inline static void store128_be(uint8_t *b, uint128_t n) {
  store64_be(b, (uint64_t)(n >> 64));
  store64_be(b + 8, (uint64_t)n);
}

inline static uint128_t FStar_UInt128_add(uint128_t x, uint128_t y) {
  return x + y;
}

inline static uint128_t FStar_UInt128_mul(uint128_t x, uint128_t y) {
  return x * y;
}

inline static uint128_t FStar_UInt128_add_mod(uint128_t x, uint128_t y) {
  return x + y;
}

inline static uint128_t FStar_UInt128_sub(uint128_t x, uint128_t y) {
  return x - y;
}

inline static uint128_t FStar_UInt128_sub_mod(uint128_t x, uint128_t y) {
  return x - y;
}

inline static uint128_t FStar_UInt128_logand(uint128_t x, uint128_t y) {
  return x & y;
}

inline static uint128_t FStar_UInt128_logor(uint128_t x, uint128_t y) {
  return x | y;
}

inline static uint128_t FStar_UInt128_logxor(uint128_t x, uint128_t y) {
  return x ^ y;
}

inline static uint128_t FStar_UInt128_lognot(uint128_t x) {
  return ~x;
}

inline static uint128_t FStar_UInt128_shift_left(uint128_t x, uint32_t y) {
  return x << y;
}

inline static uint128_t FStar_UInt128_shift_right(uint128_t x, uint32_t y) {
  return x >> y;
}

inline static uint128_t FStar_UInt128_uint64_to_uint128(uint64_t x) {
  return (uint128_t)x;
}

inline static uint64_t FStar_UInt128_uint128_to_uint64(uint128_t x) {
  return (uint64_t)x;
}

inline static uint128_t FStar_UInt128_mul_wide(uint64_t x, uint64_t y) {
  return ((uint128_t) x) * y;
}

inline static uint128_t FStar_UInt128_eq_mask(uint128_t x, uint128_t y) {
  uint64_t mask =
      FStar_UInt64_eq_mask((uint64_t)(x >> 64), (uint64_t)(y >> 64)) &
      FStar_UInt64_eq_mask((uint64_t)x, (uint64_t)y);
  return ((uint128_t)mask) << 64 | mask;
}

inline static uint128_t FStar_UInt128_gte_mask(uint128_t x, uint128_t y) {
  uint64_t mask =
      (FStar_UInt64_gte_mask(x >> 64, y >> 64) &
       ~(FStar_UInt64_eq_mask(x >> 64, y >> 64))) |
      (FStar_UInt64_eq_mask(x >> 64, y >> 64) & FStar_UInt64_gte_mask((uint64_t)x, (uint64_t)y));
  return ((uint128_t)mask) << 64 | mask;
}

inline static uint64_t FStar_UInt128___proj__Mkuint128__item__low(uint128_t x) {
  return (uint64_t) x;
}

inline static uint64_t FStar_UInt128___proj__Mkuint128__item__high(uint128_t x) {
  return (uint64_t) (x >> 64);
}

inline static uint128_t FStar_UInt128_add_underspec(uint128_t x, uint128_t y) {
  return x + y;
}

inline static uint128_t FStar_UInt128_sub_underspec(uint128_t x, uint128_t y) {
  return x - y;
}

inline static bool FStar_UInt128_eq(uint128_t x, uint128_t y) {
  return x == y;
}

inline static bool FStar_UInt128_gt(uint128_t x, uint128_t y) {
  return x > y;
}

inline static bool FStar_UInt128_lt(uint128_t x, uint128_t y) {
  return x < y;
}

inline static bool FStar_UInt128_gte(uint128_t x, uint128_t y) {
  return x >= y;
}

inline static bool FStar_UInt128_lte(uint128_t x, uint128_t y) {
  return x <= y;
}

inline static uint128_t FStar_UInt128_mul32(uint64_t x, uint32_t y) {
  return (uint128_t) x * (uint128_t) y;
}

#endif
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __internal_Hacl_Krmllib_H
#define __internal_Hacl_Krmllib_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>


static KRML_NOINLINE uint32_t FStar_UInt32_eq_mask(uint32_t a, uint32_t b);

static KRML_NOINLINE uint32_t FStar_UInt32_gte_mask(uint32_t a, uint32_t b);

static KRML_NOINLINE uint8_t FStar_UInt8_eq_mask(uint8_t a, uint8_t b);

static KRML_NOINLINE uint16_t FStar_UInt16_eq_mask(uint16_t a, uint16_t b);

static inline FStar_UInt128_uint128
FStar_UInt128_add(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_logor(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_shift_left(FStar_UInt128_uint128 a, uint32_t s);

static inline FStar_UInt128_uint128 FStar_UInt128_mul_wide(uint64_t x, uint64_t y);

static inline void store128_be(uint8_t *x0, FStar_UInt128_uint128 x1);

static inline FStar_UInt128_uint128 load128_be(uint8_t *x0);

#if defined(__cplusplus)
}
#endif

#define __internal_Hacl_Krmllib_H_DEFINED
#endif
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __Hacl_Krmllib_H
#define __Hacl_Krmllib_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>

static KRML_NOINLINE uint64_t FStar_UInt64_eq_mask(uint64_t a, uint64_t b);

static KRML_NOINLINE uint64_t FStar_UInt64_gte_mask(uint64_t a, uint64_t b);

static inline FStar_UInt128_uint128
FStar_UInt128_add_mod(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_sub_mod(FStar_UInt128_uint128 a, FStar_UInt128_uint128 b);

static inline FStar_UInt128_uint128
FStar_UInt128_shift_right(FStar_UInt128_uint128 a, uint32_t s);

static inline FStar_UInt128_uint128 FStar_UInt128_uint64_to_uint128(uint64_t a);

static inline uint64_t FStar_UInt128_uint128_to_uint64(FStar_UInt128_uint128 a);

#if defined(__cplusplus)
}
#endif

#define __Hacl_Krmllib_H_DEFINED
#endif
#ifndef HACL_CAN_COMPILE_INTRINSICS
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __Hacl_IntTypes_Intrinsics_H
#define __Hacl_IntTypes_Intrinsics_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>


static inline uint32_t
Hacl_IntTypes_Intrinsics_add_carry_u32(uint32_t cin, uint32_t x, uint32_t y, uint32_t *r)
{
  uint64_t res = (uint64_t)x + (uint64_t)cin + (uint64_t)y;
  uint32_t c = (uint32_t)(res >> 32U);
  r[0U] = (uint32_t)res;
  return c;
}

static inline uint32_t
Hacl_IntTypes_Intrinsics_sub_borrow_u32(uint32_t cin, uint32_t x, uint32_t y, uint32_t *r)
{
  uint64_t res = (uint64_t)x - (uint64_t)y - (uint64_t)cin;
  uint32_t c = (uint32_t)(res >> 32U) & 1U;
  r[0U] = (uint32_t)res;
  return c;
}

static inline uint64_t
Hacl_IntTypes_Intrinsics_add_carry_u64(uint64_t cin, uint64_t x, uint64_t y, uint64_t *r)
{
  uint64_t res = x + cin + y;
  uint64_t c = (~FStar_UInt64_gte_mask(res, x) | (FStar_UInt64_eq_mask(res, x) & cin)) & 1ULL;
  r[0U] = res;
  return c;
}

static inline uint64_t
Hacl_IntTypes_Intrinsics_sub_borrow_u64(uint64_t cin, uint64_t x, uint64_t y, uint64_t *r)
{
  uint64_t res = x - y - cin;
  uint64_t
  c =
    ((FStar_UInt64_gte_mask(res, x) & ~FStar_UInt64_eq_mask(res, x)) |
      (FStar_UInt64_eq_mask(res, x) & cin))
    & 1ULL;
  r[0U] = res;
  return c;
}

#if defined(__cplusplus)
}
#endif

#define __Hacl_IntTypes_Intrinsics_H_DEFINED
#endif
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __Hacl_IntTypes_Intrinsics_128_H
#define __Hacl_IntTypes_Intrinsics_128_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>


static inline uint64_t
Hacl_IntTypes_Intrinsics_128_add_carry_u64(uint64_t cin, uint64_t x, uint64_t y, uint64_t *r)
{
  FStar_UInt128_uint128
  res =
    FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_uint64_to_uint128(x),
        FStar_UInt128_uint64_to_uint128(cin)),
      FStar_UInt128_uint64_to_uint128(y));
  uint64_t c = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(res, 64U));
  r[0U] = FStar_UInt128_uint128_to_uint64(res);
  return c;
}

static inline uint64_t
Hacl_IntTypes_Intrinsics_128_sub_borrow_u64(uint64_t cin, uint64_t x, uint64_t y, uint64_t *r)
{
  FStar_UInt128_uint128
  res =
    FStar_UInt128_sub_mod(FStar_UInt128_sub_mod(FStar_UInt128_uint64_to_uint128(x),
        FStar_UInt128_uint64_to_uint128(y)),
      FStar_UInt128_uint64_to_uint128(cin));
  uint64_t c = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(res, 64U)) & 1ULL;
  r[0U] = FStar_UInt128_uint128_to_uint64(res);
  return c;
}

#if defined(__cplusplus)
}
#endif

#define __Hacl_IntTypes_Intrinsics_128_H_DEFINED
#endif
#endif


#include <sys/types.h>

#if defined(__has_include)
#if __has_include("config.h")
#endif
#endif

/*
   GCC versions prior to 5.5 incorrectly optimize certain intrinsics.

   See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=81300

   CLANG versions prior to 5 crash on certain intrinsics.

   See https://bugs.llvm.org/show_bug.cgi?id=24943
*/

#if !defined(HACL_CAN_COMPILE_INTRINSICS) || \
    (defined(__clang__) && (__clang_major__ < 5)) || \
    (defined(__GNUC__) && !defined(__clang__) &&  \
     (__GNUC__ < 5 || (__GNUC__ == 5 && (__GNUC_MINOR__ < 5))))


#if defined(HACL_CAN_COMPILE_UINT128)


#define Lib_IntTypes_Intrinsics_add_carry_u64(x1, x2, x3, x4) \
    (Hacl_IntTypes_Intrinsics_128_add_carry_u64(x1, x2, x3, x4))

#define Lib_IntTypes_Intrinsics_sub_borrow_u64(x1, x2, x3, x4) \
    (Hacl_IntTypes_Intrinsics_128_sub_borrow_u64(x1, x2, x3, x4))

#else

#define Lib_IntTypes_Intrinsics_add_carry_u64(x1, x2, x3, x4) \
    (Hacl_IntTypes_Intrinsics_add_carry_u64(x1, x2, x3, x4))

#define Lib_IntTypes_Intrinsics_sub_borrow_u64(x1, x2, x3, x4) \
    (Hacl_IntTypes_Intrinsics_sub_borrow_u64(x1, x2, x3, x4))

#endif // defined(HACL_CAN_COMPILE_UINT128)

#define Lib_IntTypes_Intrinsics_add_carry_u32(x1, x2, x3, x4) \
    (Hacl_IntTypes_Intrinsics_add_carry_u32(x1, x2, x3, x4))

#define Lib_IntTypes_Intrinsics_sub_borrow_u32(x1, x2, x3, x4) \
    (Hacl_IntTypes_Intrinsics_sub_borrow_u32(x1, x2, x3, x4))

#else // !defined(HACL_CAN_COMPILE_INTRINSICS)

#if defined(_MSC_VER)
#include <immintrin.h>
#else
#include <x86intrin.h>
#endif

#define Lib_IntTypes_Intrinsics_add_carry_u32(x1, x2, x3, x4) \
    (_addcarry_u32(x1, x2, x3, (unsigned int *)x4))

#define Lib_IntTypes_Intrinsics_add_carry_u64(x1, x2, x3, x4) \
    (_addcarry_u64(x1, x2, x3, (long long unsigned int *)x4))

/*
   GCC versions prior to 7.2 pass arguments to _subborrow_u{32,64}
   in an incorrect order.

   See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=81294
*/
#if defined(__GNUC__) && !defined(__clang__) && \
    (__GNUC__ < 7 || (__GNUC__ == 7 && (__GNUC_MINOR__ < 2)))

#define Lib_IntTypes_Intrinsics_sub_borrow_u32(x1, x2, x3, x4) \
    (_subborrow_u32(x1, x3, x2, (unsigned int *)x4))

#define Lib_IntTypes_Intrinsics_sub_borrow_u64(x1, x2, x3, x4) \
    (_subborrow_u64(x1, x3, x2, (long long unsigned int *)x4))

#else

#define Lib_IntTypes_Intrinsics_sub_borrow_u32(x1, x2, x3, x4) \
    (_subborrow_u32(x1, x2, x3, (unsigned int *)x4))

#define Lib_IntTypes_Intrinsics_sub_borrow_u64(x1, x2, x3, x4) \
    (_subborrow_u64(x1, x2, x3, (long long unsigned int *)x4))

#endif // GCC < 7.2

#endif // !HACL_CAN_COMPILE_INTRINSICS
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __internal_Hacl_Streaming_Types_H
#define __internal_Hacl_Streaming_Types_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>


#define Hacl_Streaming_Types_None 0
#define Hacl_Streaming_Types_Some 1

typedef uint8_t Hacl_Streaming_Types_optional;

typedef struct Hacl_Streaming_Types_optional_32_s
{
  Hacl_Streaming_Types_optional tag;
  uint32_t *v;
}
Hacl_Streaming_Types_optional_32;

typedef struct Hacl_Streaming_Types_optional_64_s
{
  Hacl_Streaming_Types_optional tag;
  uint64_t *v;
}
Hacl_Streaming_Types_optional_64;

typedef struct Hacl_Streaming_Types_two_pointers_s
{
  uint64_t *fst;
  uint64_t *snd;
}
Hacl_Streaming_Types_two_pointers;

typedef struct Hacl_Streaming_MD_state_32_s
{
  uint32_t *block_state;
  uint8_t *buf;
  uint64_t total_len;
}
Hacl_Streaming_MD_state_32;

typedef struct Hacl_Streaming_MD_state_64_s
{
  uint64_t *block_state;
  uint8_t *buf;
  uint64_t total_len;
}
Hacl_Streaming_MD_state_64;

#if defined(__cplusplus)
}
#endif

#define __internal_Hacl_Streaming_Types_H_DEFINED
#endif
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __Hacl_Streaming_Types_H
#define __Hacl_Streaming_Types_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>

#define Spec_Hash_Definitions_SHA2_224 0
#define Spec_Hash_Definitions_SHA2_256 1
#define Spec_Hash_Definitions_SHA2_384 2
#define Spec_Hash_Definitions_SHA2_512 3
#define Spec_Hash_Definitions_SHA1 4
#define Spec_Hash_Definitions_MD5 5
#define Spec_Hash_Definitions_Blake2S 6
#define Spec_Hash_Definitions_Blake2B 7
#define Spec_Hash_Definitions_SHA3_256 8
#define Spec_Hash_Definitions_SHA3_224 9
#define Spec_Hash_Definitions_SHA3_384 10
#define Spec_Hash_Definitions_SHA3_512 11
#define Spec_Hash_Definitions_Shake128 12
#define Spec_Hash_Definitions_Shake256 13

typedef uint8_t Spec_Hash_Definitions_hash_alg;

#define Hacl_Streaming_Types_Success 0
#define Hacl_Streaming_Types_InvalidAlgorithm 1
#define Hacl_Streaming_Types_InvalidLength 2
#define Hacl_Streaming_Types_MaximumLengthExceeded 3
#define Hacl_Streaming_Types_OutOfMemory 4

typedef uint8_t Hacl_Streaming_Types_error_code;

typedef struct Hacl_Streaming_MD_state_32_s Hacl_Streaming_MD_state_32;

typedef struct Hacl_Streaming_MD_state_64_s Hacl_Streaming_MD_state_64;

#if defined(__cplusplus)
}
#endif

#define __Hacl_Streaming_Types_H_DEFINED
#endif
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __internal_Hacl_Bignum_Base_H
#define __internal_Hacl_Bignum_Base_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>


static inline uint32_t
Hacl_Bignum_Base_mul_wide_add2_u32(uint32_t a, uint32_t b, uint32_t c_in, uint32_t *out)
{
  uint32_t out0 = out[0U];
  uint64_t res = (uint64_t)a * (uint64_t)b + (uint64_t)c_in + (uint64_t)out0;
  out[0U] = (uint32_t)res;
  return (uint32_t)(res >> 32U);
}

static inline uint64_t
Hacl_Bignum_Base_mul_wide_add2_u64(uint64_t a, uint64_t b, uint64_t c_in, uint64_t *out)
{
  uint64_t out0 = out[0U];
  FStar_UInt128_uint128
  res =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(a, b),
        FStar_UInt128_uint64_to_uint128(c_in)),
      FStar_UInt128_uint64_to_uint128(out0));
  out[0U] = FStar_UInt128_uint128_to_uint64(res);
  return FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(res, 64U));
}

static inline void
Hacl_Bignum_Convert_bn_from_bytes_be_uint64(uint32_t len, uint8_t *b, uint64_t *res)
{
  uint32_t bnLen = (len - 1U) / 8U + 1U;
  uint32_t tmpLen = 8U * bnLen;
  KRML_CHECK_SIZE(sizeof (uint8_t), tmpLen);
  uint8_t tmp[tmpLen];
  memset(tmp, 0U, tmpLen * sizeof (uint8_t));
  memcpy(tmp + tmpLen - len, b, len * sizeof (uint8_t));
  for (uint32_t i = 0U; i < bnLen; i++)
  {
    uint64_t *os = res;
    uint64_t u = load64_be(tmp + (bnLen - i - 1U) * 8U);
    uint64_t x = u;
    os[i] = x;
  }
}

static inline void
Hacl_Bignum_Convert_bn_to_bytes_be_uint64(uint32_t len, uint64_t *b, uint8_t *res)
{
  uint32_t bnLen = (len - 1U) / 8U + 1U;
  uint32_t tmpLen = 8U * bnLen;
  KRML_CHECK_SIZE(sizeof (uint8_t), tmpLen);
  uint8_t tmp[tmpLen];
  memset(tmp, 0U, tmpLen * sizeof (uint8_t));
  for (uint32_t i = 0U; i < bnLen; i++)
  {
    store64_be(tmp + i * 8U, b[bnLen - i - 1U]);
  }
  memcpy(res, tmp + tmpLen - len, len * sizeof (uint8_t));
}

static inline uint32_t Hacl_Bignum_Lib_bn_get_top_index_u32(uint32_t len, uint32_t *b)
{
  uint32_t priv = 0U;
  for (uint32_t i = 0U; i < len; i++)
  {
    uint32_t mask = FStar_UInt32_eq_mask(b[i], 0U);
    priv = (mask & priv) | (~mask & i);
  }
  return priv;
}

static inline uint64_t Hacl_Bignum_Lib_bn_get_top_index_u64(uint32_t len, uint64_t *b)
{
  uint64_t priv = 0ULL;
  for (uint32_t i = 0U; i < len; i++)
  {
    uint64_t mask = FStar_UInt64_eq_mask(b[i], 0ULL);
    priv = (mask & priv) | (~mask & (uint64_t)i);
  }
  return priv;
}

static inline uint32_t
Hacl_Bignum_Lib_bn_get_bits_u32(uint32_t len, uint32_t *b, uint32_t i, uint32_t l)
{
  uint32_t i1 = i / 32U;
  uint32_t j = i % 32U;
  uint32_t p1 = b[i1] >> j;
  uint32_t ite;
  if (i1 + 1U < len && 0U < j)
  {
    ite = p1 | b[i1 + 1U] << (32U - j);
  }
  else
  {
    ite = p1;
  }
  return ite & ((1U << l) - 1U);
}

static inline uint64_t
Hacl_Bignum_Lib_bn_get_bits_u64(uint32_t len, uint64_t *b, uint32_t i, uint32_t l)
{
  uint32_t i1 = i / 64U;
  uint32_t j = i % 64U;
  uint64_t p1 = b[i1] >> j;
  uint64_t ite;
  if (i1 + 1U < len && 0U < j)
  {
    ite = p1 | b[i1 + 1U] << (64U - j);
  }
  else
  {
    ite = p1;
  }
  return ite & ((1ULL << l) - 1ULL);
}

static inline uint32_t
Hacl_Bignum_Addition_bn_sub_eq_len_u32(uint32_t aLen, uint32_t *a, uint32_t *b, uint32_t *res)
{
  uint32_t c = 0U;
  for (uint32_t i = 0U; i < aLen / 4U; i++)
  {
    uint32_t t1 = a[4U * i];
    uint32_t t20 = b[4U * i];
    uint32_t *res_i0 = res + 4U * i;
    c = Lib_IntTypes_Intrinsics_sub_borrow_u32(c, t1, t20, res_i0);
    uint32_t t10 = a[4U * i + 1U];
    uint32_t t21 = b[4U * i + 1U];
    uint32_t *res_i1 = res + 4U * i + 1U;
    c = Lib_IntTypes_Intrinsics_sub_borrow_u32(c, t10, t21, res_i1);
    uint32_t t11 = a[4U * i + 2U];
    uint32_t t22 = b[4U * i + 2U];
    uint32_t *res_i2 = res + 4U * i + 2U;
    c = Lib_IntTypes_Intrinsics_sub_borrow_u32(c, t11, t22, res_i2);
    uint32_t t12 = a[4U * i + 3U];
    uint32_t t2 = b[4U * i + 3U];
    uint32_t *res_i = res + 4U * i + 3U;
    c = Lib_IntTypes_Intrinsics_sub_borrow_u32(c, t12, t2, res_i);
  }
  for (uint32_t i = aLen / 4U * 4U; i < aLen; i++)
  {
    uint32_t t1 = a[i];
    uint32_t t2 = b[i];
    uint32_t *res_i = res + i;
    c = Lib_IntTypes_Intrinsics_sub_borrow_u32(c, t1, t2, res_i);
  }
  return c;
}

static inline uint64_t
Hacl_Bignum_Addition_bn_sub_eq_len_u64(uint32_t aLen, uint64_t *a, uint64_t *b, uint64_t *res)
{
  uint64_t c = 0ULL;
  for (uint32_t i = 0U; i < aLen / 4U; i++)
  {
    uint64_t t1 = a[4U * i];
    uint64_t t20 = b[4U * i];
    uint64_t *res_i0 = res + 4U * i;
    c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t1, t20, res_i0);
    uint64_t t10 = a[4U * i + 1U];
    uint64_t t21 = b[4U * i + 1U];
    uint64_t *res_i1 = res + 4U * i + 1U;
    c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t10, t21, res_i1);
    uint64_t t11 = a[4U * i + 2U];
    uint64_t t22 = b[4U * i + 2U];
    uint64_t *res_i2 = res + 4U * i + 2U;
    c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t11, t22, res_i2);
    uint64_t t12 = a[4U * i + 3U];
    uint64_t t2 = b[4U * i + 3U];
    uint64_t *res_i = res + 4U * i + 3U;
    c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t12, t2, res_i);
  }
  for (uint32_t i = aLen / 4U * 4U; i < aLen; i++)
  {
    uint64_t t1 = a[i];
    uint64_t t2 = b[i];
    uint64_t *res_i = res + i;
    c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t1, t2, res_i);
  }
  return c;
}

static inline uint32_t
Hacl_Bignum_Addition_bn_add_eq_len_u32(uint32_t aLen, uint32_t *a, uint32_t *b, uint32_t *res)
{
  uint32_t c = 0U;
  for (uint32_t i = 0U; i < aLen / 4U; i++)
  {
    uint32_t t1 = a[4U * i];
    uint32_t t20 = b[4U * i];
    uint32_t *res_i0 = res + 4U * i;
    c = Lib_IntTypes_Intrinsics_add_carry_u32(c, t1, t20, res_i0);
    uint32_t t10 = a[4U * i + 1U];
    uint32_t t21 = b[4U * i + 1U];
    uint32_t *res_i1 = res + 4U * i + 1U;
    c = Lib_IntTypes_Intrinsics_add_carry_u32(c, t10, t21, res_i1);
    uint32_t t11 = a[4U * i + 2U];
    uint32_t t22 = b[4U * i + 2U];
    uint32_t *res_i2 = res + 4U * i + 2U;
    c = Lib_IntTypes_Intrinsics_add_carry_u32(c, t11, t22, res_i2);
    uint32_t t12 = a[4U * i + 3U];
    uint32_t t2 = b[4U * i + 3U];
    uint32_t *res_i = res + 4U * i + 3U;
    c = Lib_IntTypes_Intrinsics_add_carry_u32(c, t12, t2, res_i);
  }
  for (uint32_t i = aLen / 4U * 4U; i < aLen; i++)
  {
    uint32_t t1 = a[i];
    uint32_t t2 = b[i];
    uint32_t *res_i = res + i;
    c = Lib_IntTypes_Intrinsics_add_carry_u32(c, t1, t2, res_i);
  }
  return c;
}

static inline uint64_t
Hacl_Bignum_Addition_bn_add_eq_len_u64(uint32_t aLen, uint64_t *a, uint64_t *b, uint64_t *res)
{
  uint64_t c = 0ULL;
  for (uint32_t i = 0U; i < aLen / 4U; i++)
  {
    uint64_t t1 = a[4U * i];
    uint64_t t20 = b[4U * i];
    uint64_t *res_i0 = res + 4U * i;
    c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t1, t20, res_i0);
    uint64_t t10 = a[4U * i + 1U];
    uint64_t t21 = b[4U * i + 1U];
    uint64_t *res_i1 = res + 4U * i + 1U;
    c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t10, t21, res_i1);
    uint64_t t11 = a[4U * i + 2U];
    uint64_t t22 = b[4U * i + 2U];
    uint64_t *res_i2 = res + 4U * i + 2U;
    c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t11, t22, res_i2);
    uint64_t t12 = a[4U * i + 3U];
    uint64_t t2 = b[4U * i + 3U];
    uint64_t *res_i = res + 4U * i + 3U;
    c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t12, t2, res_i);
  }
  for (uint32_t i = aLen / 4U * 4U; i < aLen; i++)
  {
    uint64_t t1 = a[i];
    uint64_t t2 = b[i];
    uint64_t *res_i = res + i;
    c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t1, t2, res_i);
  }
  return c;
}

static inline void
Hacl_Bignum_Multiplication_bn_mul_u32(
  uint32_t aLen,
  uint32_t *a,
  uint32_t bLen,
  uint32_t *b,
  uint32_t *res
)
{
  memset(res, 0U, (aLen + bLen) * sizeof (uint32_t));
  for (uint32_t i0 = 0U; i0 < bLen; i0++)
  {
    uint32_t bj = b[i0];
    uint32_t *res_j = res + i0;
    uint32_t c = 0U;
    for (uint32_t i = 0U; i < aLen / 4U; i++)
    {
      uint32_t a_i = a[4U * i];
      uint32_t *res_i0 = res_j + 4U * i;
      c = Hacl_Bignum_Base_mul_wide_add2_u32(a_i, bj, c, res_i0);
      uint32_t a_i0 = a[4U * i + 1U];
      uint32_t *res_i1 = res_j + 4U * i + 1U;
      c = Hacl_Bignum_Base_mul_wide_add2_u32(a_i0, bj, c, res_i1);
      uint32_t a_i1 = a[4U * i + 2U];
      uint32_t *res_i2 = res_j + 4U * i + 2U;
      c = Hacl_Bignum_Base_mul_wide_add2_u32(a_i1, bj, c, res_i2);
      uint32_t a_i2 = a[4U * i + 3U];
      uint32_t *res_i = res_j + 4U * i + 3U;
      c = Hacl_Bignum_Base_mul_wide_add2_u32(a_i2, bj, c, res_i);
    }
    for (uint32_t i = aLen / 4U * 4U; i < aLen; i++)
    {
      uint32_t a_i = a[i];
      uint32_t *res_i = res_j + i;
      c = Hacl_Bignum_Base_mul_wide_add2_u32(a_i, bj, c, res_i);
    }
    uint32_t r = c;
    res[aLen + i0] = r;
  }
}

static inline void
Hacl_Bignum_Multiplication_bn_mul_u64(
  uint32_t aLen,
  uint64_t *a,
  uint32_t bLen,
  uint64_t *b,
  uint64_t *res
)
{
  memset(res, 0U, (aLen + bLen) * sizeof (uint64_t));
  for (uint32_t i0 = 0U; i0 < bLen; i0++)
  {
    uint64_t bj = b[i0];
    uint64_t *res_j = res + i0;
    uint64_t c = 0ULL;
    for (uint32_t i = 0U; i < aLen / 4U; i++)
    {
      uint64_t a_i = a[4U * i];
      uint64_t *res_i0 = res_j + 4U * i;
      c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i, bj, c, res_i0);
      uint64_t a_i0 = a[4U * i + 1U];
      uint64_t *res_i1 = res_j + 4U * i + 1U;
      c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i0, bj, c, res_i1);
      uint64_t a_i1 = a[4U * i + 2U];
      uint64_t *res_i2 = res_j + 4U * i + 2U;
      c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i1, bj, c, res_i2);
      uint64_t a_i2 = a[4U * i + 3U];
      uint64_t *res_i = res_j + 4U * i + 3U;
      c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i2, bj, c, res_i);
    }
    for (uint32_t i = aLen / 4U * 4U; i < aLen; i++)
    {
      uint64_t a_i = a[i];
      uint64_t *res_i = res_j + i;
      c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i, bj, c, res_i);
    }
    uint64_t r = c;
    res[aLen + i0] = r;
  }
}

static inline void
Hacl_Bignum_Multiplication_bn_sqr_u32(uint32_t aLen, uint32_t *a, uint32_t *res)
{
  memset(res, 0U, (aLen + aLen) * sizeof (uint32_t));
  for (uint32_t i0 = 0U; i0 < aLen; i0++)
  {
    uint32_t *ab = a;
    uint32_t a_j = a[i0];
    uint32_t *res_j = res + i0;
    uint32_t c = 0U;
    for (uint32_t i = 0U; i < i0 / 4U; i++)
    {
      uint32_t a_i = ab[4U * i];
      uint32_t *res_i0 = res_j + 4U * i;
      c = Hacl_Bignum_Base_mul_wide_add2_u32(a_i, a_j, c, res_i0);
      uint32_t a_i0 = ab[4U * i + 1U];
      uint32_t *res_i1 = res_j + 4U * i + 1U;
      c = Hacl_Bignum_Base_mul_wide_add2_u32(a_i0, a_j, c, res_i1);
      uint32_t a_i1 = ab[4U * i + 2U];
      uint32_t *res_i2 = res_j + 4U * i + 2U;
      c = Hacl_Bignum_Base_mul_wide_add2_u32(a_i1, a_j, c, res_i2);
      uint32_t a_i2 = ab[4U * i + 3U];
      uint32_t *res_i = res_j + 4U * i + 3U;
      c = Hacl_Bignum_Base_mul_wide_add2_u32(a_i2, a_j, c, res_i);
    }
    for (uint32_t i = i0 / 4U * 4U; i < i0; i++)
    {
      uint32_t a_i = ab[i];
      uint32_t *res_i = res_j + i;
      c = Hacl_Bignum_Base_mul_wide_add2_u32(a_i, a_j, c, res_i);
    }
    uint32_t r = c;
    res[i0 + i0] = r;
  }
  uint32_t c0 = Hacl_Bignum_Addition_bn_add_eq_len_u32(aLen + aLen, res, res, res);
  KRML_MAYBE_UNUSED_VAR(c0);
  KRML_CHECK_SIZE(sizeof (uint32_t), aLen + aLen);
  uint32_t tmp[aLen + aLen];
  memset(tmp, 0U, (aLen + aLen) * sizeof (uint32_t));
  for (uint32_t i = 0U; i < aLen; i++)
  {
    uint64_t res1 = (uint64_t)a[i] * (uint64_t)a[i];
    uint32_t hi = (uint32_t)(res1 >> 32U);
    uint32_t lo = (uint32_t)res1;
    tmp[2U * i] = lo;
    tmp[2U * i + 1U] = hi;
  }
  uint32_t c1 = Hacl_Bignum_Addition_bn_add_eq_len_u32(aLen + aLen, res, tmp, res);
  KRML_MAYBE_UNUSED_VAR(c1);
}

static inline void
Hacl_Bignum_Multiplication_bn_sqr_u64(uint32_t aLen, uint64_t *a, uint64_t *res)
{
  memset(res, 0U, (aLen + aLen) * sizeof (uint64_t));
  for (uint32_t i0 = 0U; i0 < aLen; i0++)
  {
    uint64_t *ab = a;
    uint64_t a_j = a[i0];
    uint64_t *res_j = res + i0;
    uint64_t c = 0ULL;
    for (uint32_t i = 0U; i < i0 / 4U; i++)
    {
      uint64_t a_i = ab[4U * i];
      uint64_t *res_i0 = res_j + 4U * i;
      c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i, a_j, c, res_i0);
      uint64_t a_i0 = ab[4U * i + 1U];
      uint64_t *res_i1 = res_j + 4U * i + 1U;
      c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i0, a_j, c, res_i1);
      uint64_t a_i1 = ab[4U * i + 2U];
      uint64_t *res_i2 = res_j + 4U * i + 2U;
      c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i1, a_j, c, res_i2);
      uint64_t a_i2 = ab[4U * i + 3U];
      uint64_t *res_i = res_j + 4U * i + 3U;
      c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i2, a_j, c, res_i);
    }
    for (uint32_t i = i0 / 4U * 4U; i < i0; i++)
    {
      uint64_t a_i = ab[i];
      uint64_t *res_i = res_j + i;
      c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i, a_j, c, res_i);
    }
    uint64_t r = c;
    res[i0 + i0] = r;
  }
  uint64_t c0 = Hacl_Bignum_Addition_bn_add_eq_len_u64(aLen + aLen, res, res, res);
  KRML_MAYBE_UNUSED_VAR(c0);
  KRML_CHECK_SIZE(sizeof (uint64_t), aLen + aLen);
  uint64_t tmp[aLen + aLen];
  memset(tmp, 0U, (aLen + aLen) * sizeof (uint64_t));
  for (uint32_t i = 0U; i < aLen; i++)
  {
    FStar_UInt128_uint128 res1 = FStar_UInt128_mul_wide(a[i], a[i]);
    uint64_t hi = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(res1, 64U));
    uint64_t lo = FStar_UInt128_uint128_to_uint64(res1);
    tmp[2U * i] = lo;
    tmp[2U * i + 1U] = hi;
  }
  uint64_t c1 = Hacl_Bignum_Addition_bn_add_eq_len_u64(aLen + aLen, res, tmp, res);
  KRML_MAYBE_UNUSED_VAR(c1);
}

#if defined(__cplusplus)
}
#endif

#define __internal_Hacl_Bignum_Base_H_DEFINED
#endif
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __internal_Hacl_Bignum25519_51_H
#define __internal_Hacl_Bignum25519_51_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>


static inline void Hacl_Impl_Curve25519_Field51_fadd(uint64_t *out, uint64_t *f1, uint64_t *f2)
{
  uint64_t f10 = f1[0U];
  uint64_t f20 = f2[0U];
  uint64_t f11 = f1[1U];
  uint64_t f21 = f2[1U];
  uint64_t f12 = f1[2U];
  uint64_t f22 = f2[2U];
  uint64_t f13 = f1[3U];
  uint64_t f23 = f2[3U];
  uint64_t f14 = f1[4U];
  uint64_t f24 = f2[4U];
  out[0U] = f10 + f20;
  out[1U] = f11 + f21;
  out[2U] = f12 + f22;
  out[3U] = f13 + f23;
  out[4U] = f14 + f24;
}

static inline void Hacl_Impl_Curve25519_Field51_fsub(uint64_t *out, uint64_t *f1, uint64_t *f2)
{
  uint64_t f10 = f1[0U];
  uint64_t f20 = f2[0U];
  uint64_t f11 = f1[1U];
  uint64_t f21 = f2[1U];
  uint64_t f12 = f1[2U];
  uint64_t f22 = f2[2U];
  uint64_t f13 = f1[3U];
  uint64_t f23 = f2[3U];
  uint64_t f14 = f1[4U];
  uint64_t f24 = f2[4U];
  out[0U] = f10 + 0x3fffffffffff68ULL - f20;
  out[1U] = f11 + 0x3ffffffffffff8ULL - f21;
  out[2U] = f12 + 0x3ffffffffffff8ULL - f22;
  out[3U] = f13 + 0x3ffffffffffff8ULL - f23;
  out[4U] = f14 + 0x3ffffffffffff8ULL - f24;
}

static inline void
Hacl_Impl_Curve25519_Field51_fmul(
  uint64_t *out,
  uint64_t *f1,
  uint64_t *f2,
  FStar_UInt128_uint128 *uu___
)
{
  KRML_MAYBE_UNUSED_VAR(uu___);
  uint64_t f10 = f1[0U];
  uint64_t f11 = f1[1U];
  uint64_t f12 = f1[2U];
  uint64_t f13 = f1[3U];
  uint64_t f14 = f1[4U];
  uint64_t f20 = f2[0U];
  uint64_t f21 = f2[1U];
  uint64_t f22 = f2[2U];
  uint64_t f23 = f2[3U];
  uint64_t f24 = f2[4U];
  uint64_t tmp1 = f21 * 19ULL;
  uint64_t tmp2 = f22 * 19ULL;
  uint64_t tmp3 = f23 * 19ULL;
  uint64_t tmp4 = f24 * 19ULL;
  FStar_UInt128_uint128 o00 = FStar_UInt128_mul_wide(f10, f20);
  FStar_UInt128_uint128 o10 = FStar_UInt128_mul_wide(f10, f21);
  FStar_UInt128_uint128 o20 = FStar_UInt128_mul_wide(f10, f22);
  FStar_UInt128_uint128 o30 = FStar_UInt128_mul_wide(f10, f23);
  FStar_UInt128_uint128 o40 = FStar_UInt128_mul_wide(f10, f24);
  FStar_UInt128_uint128 o01 = FStar_UInt128_add(o00, FStar_UInt128_mul_wide(f11, tmp4));
  FStar_UInt128_uint128 o11 = FStar_UInt128_add(o10, FStar_UInt128_mul_wide(f11, f20));
  FStar_UInt128_uint128 o21 = FStar_UInt128_add(o20, FStar_UInt128_mul_wide(f11, f21));
  FStar_UInt128_uint128 o31 = FStar_UInt128_add(o30, FStar_UInt128_mul_wide(f11, f22));
  FStar_UInt128_uint128 o41 = FStar_UInt128_add(o40, FStar_UInt128_mul_wide(f11, f23));
  FStar_UInt128_uint128 o02 = FStar_UInt128_add(o01, FStar_UInt128_mul_wide(f12, tmp3));
  FStar_UInt128_uint128 o12 = FStar_UInt128_add(o11, FStar_UInt128_mul_wide(f12, tmp4));
  FStar_UInt128_uint128 o22 = FStar_UInt128_add(o21, FStar_UInt128_mul_wide(f12, f20));
  FStar_UInt128_uint128 o32 = FStar_UInt128_add(o31, FStar_UInt128_mul_wide(f12, f21));
  FStar_UInt128_uint128 o42 = FStar_UInt128_add(o41, FStar_UInt128_mul_wide(f12, f22));
  FStar_UInt128_uint128 o03 = FStar_UInt128_add(o02, FStar_UInt128_mul_wide(f13, tmp2));
  FStar_UInt128_uint128 o13 = FStar_UInt128_add(o12, FStar_UInt128_mul_wide(f13, tmp3));
  FStar_UInt128_uint128 o23 = FStar_UInt128_add(o22, FStar_UInt128_mul_wide(f13, tmp4));
  FStar_UInt128_uint128 o33 = FStar_UInt128_add(o32, FStar_UInt128_mul_wide(f13, f20));
  FStar_UInt128_uint128 o43 = FStar_UInt128_add(o42, FStar_UInt128_mul_wide(f13, f21));
  FStar_UInt128_uint128 o04 = FStar_UInt128_add(o03, FStar_UInt128_mul_wide(f14, tmp1));
  FStar_UInt128_uint128 o14 = FStar_UInt128_add(o13, FStar_UInt128_mul_wide(f14, tmp2));
  FStar_UInt128_uint128 o24 = FStar_UInt128_add(o23, FStar_UInt128_mul_wide(f14, tmp3));
  FStar_UInt128_uint128 o34 = FStar_UInt128_add(o33, FStar_UInt128_mul_wide(f14, tmp4));
  FStar_UInt128_uint128 o44 = FStar_UInt128_add(o43, FStar_UInt128_mul_wide(f14, f20));
  FStar_UInt128_uint128 tmp_w0 = o04;
  FStar_UInt128_uint128 tmp_w1 = o14;
  FStar_UInt128_uint128 tmp_w2 = o24;
  FStar_UInt128_uint128 tmp_w3 = o34;
  FStar_UInt128_uint128 tmp_w4 = o44;
  FStar_UInt128_uint128 l_ = FStar_UInt128_add(tmp_w0, FStar_UInt128_uint64_to_uint128(0ULL));
  uint64_t tmp01 = FStar_UInt128_uint128_to_uint64(l_) & 0x7ffffffffffffULL;
  uint64_t c0 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_, 51U));
  FStar_UInt128_uint128 l_0 = FStar_UInt128_add(tmp_w1, FStar_UInt128_uint64_to_uint128(c0));
  uint64_t tmp11 = FStar_UInt128_uint128_to_uint64(l_0) & 0x7ffffffffffffULL;
  uint64_t c1 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_0, 51U));
  FStar_UInt128_uint128 l_1 = FStar_UInt128_add(tmp_w2, FStar_UInt128_uint64_to_uint128(c1));
  uint64_t tmp21 = FStar_UInt128_uint128_to_uint64(l_1) & 0x7ffffffffffffULL;
  uint64_t c2 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_1, 51U));
  FStar_UInt128_uint128 l_2 = FStar_UInt128_add(tmp_w3, FStar_UInt128_uint64_to_uint128(c2));
  uint64_t tmp31 = FStar_UInt128_uint128_to_uint64(l_2) & 0x7ffffffffffffULL;
  uint64_t c3 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_2, 51U));
  FStar_UInt128_uint128 l_3 = FStar_UInt128_add(tmp_w4, FStar_UInt128_uint64_to_uint128(c3));
  uint64_t tmp41 = FStar_UInt128_uint128_to_uint64(l_3) & 0x7ffffffffffffULL;
  uint64_t c4 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_3, 51U));
  uint64_t l_4 = tmp01 + c4 * 19ULL;
  uint64_t tmp0_ = l_4 & 0x7ffffffffffffULL;
  uint64_t c5 = l_4 >> 51U;
  uint64_t o0 = tmp0_;
  uint64_t o1 = tmp11 + c5;
  uint64_t o2 = tmp21;
  uint64_t o3 = tmp31;
  uint64_t o4 = tmp41;
  out[0U] = o0;
  out[1U] = o1;
  out[2U] = o2;
  out[3U] = o3;
  out[4U] = o4;
}

static inline void
Hacl_Impl_Curve25519_Field51_fmul2(
  uint64_t *out,
  uint64_t *f1,
  uint64_t *f2,
  FStar_UInt128_uint128 *uu___
)
{
  KRML_MAYBE_UNUSED_VAR(uu___);
  uint64_t f10 = f1[0U];
  uint64_t f11 = f1[1U];
  uint64_t f12 = f1[2U];
  uint64_t f13 = f1[3U];
  uint64_t f14 = f1[4U];
  uint64_t f20 = f2[0U];
  uint64_t f21 = f2[1U];
  uint64_t f22 = f2[2U];
  uint64_t f23 = f2[3U];
  uint64_t f24 = f2[4U];
  uint64_t f30 = f1[5U];
  uint64_t f31 = f1[6U];
  uint64_t f32 = f1[7U];
  uint64_t f33 = f1[8U];
  uint64_t f34 = f1[9U];
  uint64_t f40 = f2[5U];
  uint64_t f41 = f2[6U];
  uint64_t f42 = f2[7U];
  uint64_t f43 = f2[8U];
  uint64_t f44 = f2[9U];
  uint64_t tmp11 = f21 * 19ULL;
  uint64_t tmp12 = f22 * 19ULL;
  uint64_t tmp13 = f23 * 19ULL;
  uint64_t tmp14 = f24 * 19ULL;
  uint64_t tmp21 = f41 * 19ULL;
  uint64_t tmp22 = f42 * 19ULL;
  uint64_t tmp23 = f43 * 19ULL;
  uint64_t tmp24 = f44 * 19ULL;
  FStar_UInt128_uint128 o00 = FStar_UInt128_mul_wide(f10, f20);
  FStar_UInt128_uint128 o15 = FStar_UInt128_mul_wide(f10, f21);
  FStar_UInt128_uint128 o25 = FStar_UInt128_mul_wide(f10, f22);
  FStar_UInt128_uint128 o30 = FStar_UInt128_mul_wide(f10, f23);
  FStar_UInt128_uint128 o40 = FStar_UInt128_mul_wide(f10, f24);
  FStar_UInt128_uint128 o010 = FStar_UInt128_add(o00, FStar_UInt128_mul_wide(f11, tmp14));
  FStar_UInt128_uint128 o110 = FStar_UInt128_add(o15, FStar_UInt128_mul_wide(f11, f20));
  FStar_UInt128_uint128 o210 = FStar_UInt128_add(o25, FStar_UInt128_mul_wide(f11, f21));
  FStar_UInt128_uint128 o310 = FStar_UInt128_add(o30, FStar_UInt128_mul_wide(f11, f22));
  FStar_UInt128_uint128 o410 = FStar_UInt128_add(o40, FStar_UInt128_mul_wide(f11, f23));
  FStar_UInt128_uint128 o020 = FStar_UInt128_add(o010, FStar_UInt128_mul_wide(f12, tmp13));
  FStar_UInt128_uint128 o120 = FStar_UInt128_add(o110, FStar_UInt128_mul_wide(f12, tmp14));
  FStar_UInt128_uint128 o220 = FStar_UInt128_add(o210, FStar_UInt128_mul_wide(f12, f20));
  FStar_UInt128_uint128 o320 = FStar_UInt128_add(o310, FStar_UInt128_mul_wide(f12, f21));
  FStar_UInt128_uint128 o420 = FStar_UInt128_add(o410, FStar_UInt128_mul_wide(f12, f22));
  FStar_UInt128_uint128 o030 = FStar_UInt128_add(o020, FStar_UInt128_mul_wide(f13, tmp12));
  FStar_UInt128_uint128 o130 = FStar_UInt128_add(o120, FStar_UInt128_mul_wide(f13, tmp13));
  FStar_UInt128_uint128 o230 = FStar_UInt128_add(o220, FStar_UInt128_mul_wide(f13, tmp14));
  FStar_UInt128_uint128 o330 = FStar_UInt128_add(o320, FStar_UInt128_mul_wide(f13, f20));
  FStar_UInt128_uint128 o430 = FStar_UInt128_add(o420, FStar_UInt128_mul_wide(f13, f21));
  FStar_UInt128_uint128 o040 = FStar_UInt128_add(o030, FStar_UInt128_mul_wide(f14, tmp11));
  FStar_UInt128_uint128 o140 = FStar_UInt128_add(o130, FStar_UInt128_mul_wide(f14, tmp12));
  FStar_UInt128_uint128 o240 = FStar_UInt128_add(o230, FStar_UInt128_mul_wide(f14, tmp13));
  FStar_UInt128_uint128 o340 = FStar_UInt128_add(o330, FStar_UInt128_mul_wide(f14, tmp14));
  FStar_UInt128_uint128 o440 = FStar_UInt128_add(o430, FStar_UInt128_mul_wide(f14, f20));
  FStar_UInt128_uint128 tmp_w10 = o040;
  FStar_UInt128_uint128 tmp_w11 = o140;
  FStar_UInt128_uint128 tmp_w12 = o240;
  FStar_UInt128_uint128 tmp_w13 = o340;
  FStar_UInt128_uint128 tmp_w14 = o440;
  FStar_UInt128_uint128 o0 = FStar_UInt128_mul_wide(f30, f40);
  FStar_UInt128_uint128 o1 = FStar_UInt128_mul_wide(f30, f41);
  FStar_UInt128_uint128 o2 = FStar_UInt128_mul_wide(f30, f42);
  FStar_UInt128_uint128 o3 = FStar_UInt128_mul_wide(f30, f43);
  FStar_UInt128_uint128 o4 = FStar_UInt128_mul_wide(f30, f44);
  FStar_UInt128_uint128 o01 = FStar_UInt128_add(o0, FStar_UInt128_mul_wide(f31, tmp24));
  FStar_UInt128_uint128 o111 = FStar_UInt128_add(o1, FStar_UInt128_mul_wide(f31, f40));
  FStar_UInt128_uint128 o211 = FStar_UInt128_add(o2, FStar_UInt128_mul_wide(f31, f41));
  FStar_UInt128_uint128 o31 = FStar_UInt128_add(o3, FStar_UInt128_mul_wide(f31, f42));
  FStar_UInt128_uint128 o41 = FStar_UInt128_add(o4, FStar_UInt128_mul_wide(f31, f43));
  FStar_UInt128_uint128 o02 = FStar_UInt128_add(o01, FStar_UInt128_mul_wide(f32, tmp23));
  FStar_UInt128_uint128 o121 = FStar_UInt128_add(o111, FStar_UInt128_mul_wide(f32, tmp24));
  FStar_UInt128_uint128 o221 = FStar_UInt128_add(o211, FStar_UInt128_mul_wide(f32, f40));
  FStar_UInt128_uint128 o32 = FStar_UInt128_add(o31, FStar_UInt128_mul_wide(f32, f41));
  FStar_UInt128_uint128 o42 = FStar_UInt128_add(o41, FStar_UInt128_mul_wide(f32, f42));
  FStar_UInt128_uint128 o03 = FStar_UInt128_add(o02, FStar_UInt128_mul_wide(f33, tmp22));
  FStar_UInt128_uint128 o131 = FStar_UInt128_add(o121, FStar_UInt128_mul_wide(f33, tmp23));
  FStar_UInt128_uint128 o231 = FStar_UInt128_add(o221, FStar_UInt128_mul_wide(f33, tmp24));
  FStar_UInt128_uint128 o33 = FStar_UInt128_add(o32, FStar_UInt128_mul_wide(f33, f40));
  FStar_UInt128_uint128 o43 = FStar_UInt128_add(o42, FStar_UInt128_mul_wide(f33, f41));
  FStar_UInt128_uint128 o04 = FStar_UInt128_add(o03, FStar_UInt128_mul_wide(f34, tmp21));
  FStar_UInt128_uint128 o141 = FStar_UInt128_add(o131, FStar_UInt128_mul_wide(f34, tmp22));
  FStar_UInt128_uint128 o241 = FStar_UInt128_add(o231, FStar_UInt128_mul_wide(f34, tmp23));
  FStar_UInt128_uint128 o34 = FStar_UInt128_add(o33, FStar_UInt128_mul_wide(f34, tmp24));
  FStar_UInt128_uint128 o44 = FStar_UInt128_add(o43, FStar_UInt128_mul_wide(f34, f40));
  FStar_UInt128_uint128 tmp_w20 = o04;
  FStar_UInt128_uint128 tmp_w21 = o141;
  FStar_UInt128_uint128 tmp_w22 = o241;
  FStar_UInt128_uint128 tmp_w23 = o34;
  FStar_UInt128_uint128 tmp_w24 = o44;
  FStar_UInt128_uint128 l_ = FStar_UInt128_add(tmp_w10, FStar_UInt128_uint64_to_uint128(0ULL));
  uint64_t tmp00 = FStar_UInt128_uint128_to_uint64(l_) & 0x7ffffffffffffULL;
  uint64_t c00 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_, 51U));
  FStar_UInt128_uint128 l_0 = FStar_UInt128_add(tmp_w11, FStar_UInt128_uint64_to_uint128(c00));
  uint64_t tmp10 = FStar_UInt128_uint128_to_uint64(l_0) & 0x7ffffffffffffULL;
  uint64_t c10 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_0, 51U));
  FStar_UInt128_uint128 l_1 = FStar_UInt128_add(tmp_w12, FStar_UInt128_uint64_to_uint128(c10));
  uint64_t tmp20 = FStar_UInt128_uint128_to_uint64(l_1) & 0x7ffffffffffffULL;
  uint64_t c20 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_1, 51U));
  FStar_UInt128_uint128 l_2 = FStar_UInt128_add(tmp_w13, FStar_UInt128_uint64_to_uint128(c20));
  uint64_t tmp30 = FStar_UInt128_uint128_to_uint64(l_2) & 0x7ffffffffffffULL;
  uint64_t c30 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_2, 51U));
  FStar_UInt128_uint128 l_3 = FStar_UInt128_add(tmp_w14, FStar_UInt128_uint64_to_uint128(c30));
  uint64_t tmp40 = FStar_UInt128_uint128_to_uint64(l_3) & 0x7ffffffffffffULL;
  uint64_t c40 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_3, 51U));
  uint64_t l_4 = tmp00 + c40 * 19ULL;
  uint64_t tmp0_ = l_4 & 0x7ffffffffffffULL;
  uint64_t c50 = l_4 >> 51U;
  uint64_t o100 = tmp0_;
  uint64_t o112 = tmp10 + c50;
  uint64_t o122 = tmp20;
  uint64_t o132 = tmp30;
  uint64_t o142 = tmp40;
  FStar_UInt128_uint128 l_5 = FStar_UInt128_add(tmp_w20, FStar_UInt128_uint64_to_uint128(0ULL));
  uint64_t tmp0 = FStar_UInt128_uint128_to_uint64(l_5) & 0x7ffffffffffffULL;
  uint64_t c0 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_5, 51U));
  FStar_UInt128_uint128 l_6 = FStar_UInt128_add(tmp_w21, FStar_UInt128_uint64_to_uint128(c0));
  uint64_t tmp1 = FStar_UInt128_uint128_to_uint64(l_6) & 0x7ffffffffffffULL;
  uint64_t c1 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_6, 51U));
  FStar_UInt128_uint128 l_7 = FStar_UInt128_add(tmp_w22, FStar_UInt128_uint64_to_uint128(c1));
  uint64_t tmp2 = FStar_UInt128_uint128_to_uint64(l_7) & 0x7ffffffffffffULL;
  uint64_t c2 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_7, 51U));
  FStar_UInt128_uint128 l_8 = FStar_UInt128_add(tmp_w23, FStar_UInt128_uint64_to_uint128(c2));
  uint64_t tmp3 = FStar_UInt128_uint128_to_uint64(l_8) & 0x7ffffffffffffULL;
  uint64_t c3 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_8, 51U));
  FStar_UInt128_uint128 l_9 = FStar_UInt128_add(tmp_w24, FStar_UInt128_uint64_to_uint128(c3));
  uint64_t tmp4 = FStar_UInt128_uint128_to_uint64(l_9) & 0x7ffffffffffffULL;
  uint64_t c4 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_9, 51U));
  uint64_t l_10 = tmp0 + c4 * 19ULL;
  uint64_t tmp0_0 = l_10 & 0x7ffffffffffffULL;
  uint64_t c5 = l_10 >> 51U;
  uint64_t o200 = tmp0_0;
  uint64_t o212 = tmp1 + c5;
  uint64_t o222 = tmp2;
  uint64_t o232 = tmp3;
  uint64_t o242 = tmp4;
  uint64_t o10 = o100;
  uint64_t o11 = o112;
  uint64_t o12 = o122;
  uint64_t o13 = o132;
  uint64_t o14 = o142;
  uint64_t o20 = o200;
  uint64_t o21 = o212;
  uint64_t o22 = o222;
  uint64_t o23 = o232;
  uint64_t o24 = o242;
  out[0U] = o10;
  out[1U] = o11;
  out[2U] = o12;
  out[3U] = o13;
  out[4U] = o14;
  out[5U] = o20;
  out[6U] = o21;
  out[7U] = o22;
  out[8U] = o23;
  out[9U] = o24;
}

static inline void Hacl_Impl_Curve25519_Field51_fmul1(uint64_t *out, uint64_t *f1, uint64_t f2)
{
  uint64_t f10 = f1[0U];
  uint64_t f11 = f1[1U];
  uint64_t f12 = f1[2U];
  uint64_t f13 = f1[3U];
  uint64_t f14 = f1[4U];
  FStar_UInt128_uint128 tmp_w0 = FStar_UInt128_mul_wide(f2, f10);
  FStar_UInt128_uint128 tmp_w1 = FStar_UInt128_mul_wide(f2, f11);
  FStar_UInt128_uint128 tmp_w2 = FStar_UInt128_mul_wide(f2, f12);
  FStar_UInt128_uint128 tmp_w3 = FStar_UInt128_mul_wide(f2, f13);
  FStar_UInt128_uint128 tmp_w4 = FStar_UInt128_mul_wide(f2, f14);
  FStar_UInt128_uint128 l_ = FStar_UInt128_add(tmp_w0, FStar_UInt128_uint64_to_uint128(0ULL));
  uint64_t tmp0 = FStar_UInt128_uint128_to_uint64(l_) & 0x7ffffffffffffULL;
  uint64_t c0 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_, 51U));
  FStar_UInt128_uint128 l_0 = FStar_UInt128_add(tmp_w1, FStar_UInt128_uint64_to_uint128(c0));
  uint64_t tmp1 = FStar_UInt128_uint128_to_uint64(l_0) & 0x7ffffffffffffULL;
  uint64_t c1 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_0, 51U));
  FStar_UInt128_uint128 l_1 = FStar_UInt128_add(tmp_w2, FStar_UInt128_uint64_to_uint128(c1));
  uint64_t tmp2 = FStar_UInt128_uint128_to_uint64(l_1) & 0x7ffffffffffffULL;
  uint64_t c2 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_1, 51U));
  FStar_UInt128_uint128 l_2 = FStar_UInt128_add(tmp_w3, FStar_UInt128_uint64_to_uint128(c2));
  uint64_t tmp3 = FStar_UInt128_uint128_to_uint64(l_2) & 0x7ffffffffffffULL;
  uint64_t c3 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_2, 51U));
  FStar_UInt128_uint128 l_3 = FStar_UInt128_add(tmp_w4, FStar_UInt128_uint64_to_uint128(c3));
  uint64_t tmp4 = FStar_UInt128_uint128_to_uint64(l_3) & 0x7ffffffffffffULL;
  uint64_t c4 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_3, 51U));
  uint64_t l_4 = tmp0 + c4 * 19ULL;
  uint64_t tmp0_ = l_4 & 0x7ffffffffffffULL;
  uint64_t c5 = l_4 >> 51U;
  uint64_t o0 = tmp0_;
  uint64_t o1 = tmp1 + c5;
  uint64_t o2 = tmp2;
  uint64_t o3 = tmp3;
  uint64_t o4 = tmp4;
  out[0U] = o0;
  out[1U] = o1;
  out[2U] = o2;
  out[3U] = o3;
  out[4U] = o4;
}

static inline void
Hacl_Impl_Curve25519_Field51_fsqr(uint64_t *out, uint64_t *f, FStar_UInt128_uint128 *uu___)
{
  KRML_MAYBE_UNUSED_VAR(uu___);
  uint64_t f0 = f[0U];
  uint64_t f1 = f[1U];
  uint64_t f2 = f[2U];
  uint64_t f3 = f[3U];
  uint64_t f4 = f[4U];
  uint64_t d0 = 2ULL * f0;
  uint64_t d1 = 2ULL * f1;
  uint64_t d2 = 38ULL * f2;
  uint64_t d3 = 19ULL * f3;
  uint64_t d419 = 19ULL * f4;
  uint64_t d4 = 2ULL * d419;
  FStar_UInt128_uint128
  s0 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(f0, f0),
        FStar_UInt128_mul_wide(d4, f1)),
      FStar_UInt128_mul_wide(d2, f3));
  FStar_UInt128_uint128
  s1 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d0, f1),
        FStar_UInt128_mul_wide(d4, f2)),
      FStar_UInt128_mul_wide(d3, f3));
  FStar_UInt128_uint128
  s2 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d0, f2),
        FStar_UInt128_mul_wide(f1, f1)),
      FStar_UInt128_mul_wide(d4, f3));
  FStar_UInt128_uint128
  s3 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d0, f3),
        FStar_UInt128_mul_wide(d1, f2)),
      FStar_UInt128_mul_wide(f4, d419));
  FStar_UInt128_uint128
  s4 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d0, f4),
        FStar_UInt128_mul_wide(d1, f3)),
      FStar_UInt128_mul_wide(f2, f2));
  FStar_UInt128_uint128 o00 = s0;
  FStar_UInt128_uint128 o10 = s1;
  FStar_UInt128_uint128 o20 = s2;
  FStar_UInt128_uint128 o30 = s3;
  FStar_UInt128_uint128 o40 = s4;
  FStar_UInt128_uint128 l_ = FStar_UInt128_add(o00, FStar_UInt128_uint64_to_uint128(0ULL));
  uint64_t tmp0 = FStar_UInt128_uint128_to_uint64(l_) & 0x7ffffffffffffULL;
  uint64_t c0 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_, 51U));
  FStar_UInt128_uint128 l_0 = FStar_UInt128_add(o10, FStar_UInt128_uint64_to_uint128(c0));
  uint64_t tmp1 = FStar_UInt128_uint128_to_uint64(l_0) & 0x7ffffffffffffULL;
  uint64_t c1 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_0, 51U));
  FStar_UInt128_uint128 l_1 = FStar_UInt128_add(o20, FStar_UInt128_uint64_to_uint128(c1));
  uint64_t tmp2 = FStar_UInt128_uint128_to_uint64(l_1) & 0x7ffffffffffffULL;
  uint64_t c2 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_1, 51U));
  FStar_UInt128_uint128 l_2 = FStar_UInt128_add(o30, FStar_UInt128_uint64_to_uint128(c2));
  uint64_t tmp3 = FStar_UInt128_uint128_to_uint64(l_2) & 0x7ffffffffffffULL;
  uint64_t c3 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_2, 51U));
  FStar_UInt128_uint128 l_3 = FStar_UInt128_add(o40, FStar_UInt128_uint64_to_uint128(c3));
  uint64_t tmp4 = FStar_UInt128_uint128_to_uint64(l_3) & 0x7ffffffffffffULL;
  uint64_t c4 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_3, 51U));
  uint64_t l_4 = tmp0 + c4 * 19ULL;
  uint64_t tmp0_ = l_4 & 0x7ffffffffffffULL;
  uint64_t c5 = l_4 >> 51U;
  uint64_t o0 = tmp0_;
  uint64_t o1 = tmp1 + c5;
  uint64_t o2 = tmp2;
  uint64_t o3 = tmp3;
  uint64_t o4 = tmp4;
  out[0U] = o0;
  out[1U] = o1;
  out[2U] = o2;
  out[3U] = o3;
  out[4U] = o4;
}

static inline void
Hacl_Impl_Curve25519_Field51_fsqr2(uint64_t *out, uint64_t *f, FStar_UInt128_uint128 *uu___)
{
  KRML_MAYBE_UNUSED_VAR(uu___);
  uint64_t f10 = f[0U];
  uint64_t f11 = f[1U];
  uint64_t f12 = f[2U];
  uint64_t f13 = f[3U];
  uint64_t f14 = f[4U];
  uint64_t f20 = f[5U];
  uint64_t f21 = f[6U];
  uint64_t f22 = f[7U];
  uint64_t f23 = f[8U];
  uint64_t f24 = f[9U];
  uint64_t d00 = 2ULL * f10;
  uint64_t d10 = 2ULL * f11;
  uint64_t d20 = 38ULL * f12;
  uint64_t d30 = 19ULL * f13;
  uint64_t d4190 = 19ULL * f14;
  uint64_t d40 = 2ULL * d4190;
  FStar_UInt128_uint128
  s00 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(f10, f10),
        FStar_UInt128_mul_wide(d40, f11)),
      FStar_UInt128_mul_wide(d20, f13));
  FStar_UInt128_uint128
  s10 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d00, f11),
        FStar_UInt128_mul_wide(d40, f12)),
      FStar_UInt128_mul_wide(d30, f13));
  FStar_UInt128_uint128
  s20 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d00, f12),
        FStar_UInt128_mul_wide(f11, f11)),
      FStar_UInt128_mul_wide(d40, f13));
  FStar_UInt128_uint128
  s30 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d00, f13),
        FStar_UInt128_mul_wide(d10, f12)),
      FStar_UInt128_mul_wide(f14, d4190));
  FStar_UInt128_uint128
  s40 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d00, f14),
        FStar_UInt128_mul_wide(d10, f13)),
      FStar_UInt128_mul_wide(f12, f12));
  FStar_UInt128_uint128 o100 = s00;
  FStar_UInt128_uint128 o110 = s10;
  FStar_UInt128_uint128 o120 = s20;
  FStar_UInt128_uint128 o130 = s30;
  FStar_UInt128_uint128 o140 = s40;
  uint64_t d0 = 2ULL * f20;
  uint64_t d1 = 2ULL * f21;
  uint64_t d2 = 38ULL * f22;
  uint64_t d3 = 19ULL * f23;
  uint64_t d419 = 19ULL * f24;
  uint64_t d4 = 2ULL * d419;
  FStar_UInt128_uint128
  s0 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(f20, f20),
        FStar_UInt128_mul_wide(d4, f21)),
      FStar_UInt128_mul_wide(d2, f23));
  FStar_UInt128_uint128
  s1 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d0, f21),
        FStar_UInt128_mul_wide(d4, f22)),
      FStar_UInt128_mul_wide(d3, f23));
  FStar_UInt128_uint128
  s2 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d0, f22),
        FStar_UInt128_mul_wide(f21, f21)),
      FStar_UInt128_mul_wide(d4, f23));
  FStar_UInt128_uint128
  s3 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d0, f23),
        FStar_UInt128_mul_wide(d1, f22)),
      FStar_UInt128_mul_wide(f24, d419));
  FStar_UInt128_uint128
  s4 =
    FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d0, f24),
        FStar_UInt128_mul_wide(d1, f23)),
      FStar_UInt128_mul_wide(f22, f22));
  FStar_UInt128_uint128 o200 = s0;
  FStar_UInt128_uint128 o210 = s1;
  FStar_UInt128_uint128 o220 = s2;
  FStar_UInt128_uint128 o230 = s3;
  FStar_UInt128_uint128 o240 = s4;
  FStar_UInt128_uint128 l_ = FStar_UInt128_add(o100, FStar_UInt128_uint64_to_uint128(0ULL));
  uint64_t tmp00 = FStar_UInt128_uint128_to_uint64(l_) & 0x7ffffffffffffULL;
  uint64_t c00 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_, 51U));
  FStar_UInt128_uint128 l_0 = FStar_UInt128_add(o110, FStar_UInt128_uint64_to_uint128(c00));
  uint64_t tmp10 = FStar_UInt128_uint128_to_uint64(l_0) & 0x7ffffffffffffULL;
  uint64_t c10 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_0, 51U));
  FStar_UInt128_uint128 l_1 = FStar_UInt128_add(o120, FStar_UInt128_uint64_to_uint128(c10));
  uint64_t tmp20 = FStar_UInt128_uint128_to_uint64(l_1) & 0x7ffffffffffffULL;
  uint64_t c20 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_1, 51U));
  FStar_UInt128_uint128 l_2 = FStar_UInt128_add(o130, FStar_UInt128_uint64_to_uint128(c20));
  uint64_t tmp30 = FStar_UInt128_uint128_to_uint64(l_2) & 0x7ffffffffffffULL;
  uint64_t c30 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_2, 51U));
  FStar_UInt128_uint128 l_3 = FStar_UInt128_add(o140, FStar_UInt128_uint64_to_uint128(c30));
  uint64_t tmp40 = FStar_UInt128_uint128_to_uint64(l_3) & 0x7ffffffffffffULL;
  uint64_t c40 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_3, 51U));
  uint64_t l_4 = tmp00 + c40 * 19ULL;
  uint64_t tmp0_ = l_4 & 0x7ffffffffffffULL;
  uint64_t c50 = l_4 >> 51U;
  uint64_t o101 = tmp0_;
  uint64_t o111 = tmp10 + c50;
  uint64_t o121 = tmp20;
  uint64_t o131 = tmp30;
  uint64_t o141 = tmp40;
  FStar_UInt128_uint128 l_5 = FStar_UInt128_add(o200, FStar_UInt128_uint64_to_uint128(0ULL));
  uint64_t tmp0 = FStar_UInt128_uint128_to_uint64(l_5) & 0x7ffffffffffffULL;
  uint64_t c0 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_5, 51U));
  FStar_UInt128_uint128 l_6 = FStar_UInt128_add(o210, FStar_UInt128_uint64_to_uint128(c0));
  uint64_t tmp1 = FStar_UInt128_uint128_to_uint64(l_6) & 0x7ffffffffffffULL;
  uint64_t c1 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_6, 51U));
  FStar_UInt128_uint128 l_7 = FStar_UInt128_add(o220, FStar_UInt128_uint64_to_uint128(c1));
  uint64_t tmp2 = FStar_UInt128_uint128_to_uint64(l_7) & 0x7ffffffffffffULL;
  uint64_t c2 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_7, 51U));
  FStar_UInt128_uint128 l_8 = FStar_UInt128_add(o230, FStar_UInt128_uint64_to_uint128(c2));
  uint64_t tmp3 = FStar_UInt128_uint128_to_uint64(l_8) & 0x7ffffffffffffULL;
  uint64_t c3 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_8, 51U));
  FStar_UInt128_uint128 l_9 = FStar_UInt128_add(o240, FStar_UInt128_uint64_to_uint128(c3));
  uint64_t tmp4 = FStar_UInt128_uint128_to_uint64(l_9) & 0x7ffffffffffffULL;
  uint64_t c4 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(l_9, 51U));
  uint64_t l_10 = tmp0 + c4 * 19ULL;
  uint64_t tmp0_0 = l_10 & 0x7ffffffffffffULL;
  uint64_t c5 = l_10 >> 51U;
  uint64_t o201 = tmp0_0;
  uint64_t o211 = tmp1 + c5;
  uint64_t o221 = tmp2;
  uint64_t o231 = tmp3;
  uint64_t o241 = tmp4;
  uint64_t o10 = o101;
  uint64_t o11 = o111;
  uint64_t o12 = o121;
  uint64_t o13 = o131;
  uint64_t o14 = o141;
  uint64_t o20 = o201;
  uint64_t o21 = o211;
  uint64_t o22 = o221;
  uint64_t o23 = o231;
  uint64_t o24 = o241;
  out[0U] = o10;
  out[1U] = o11;
  out[2U] = o12;
  out[3U] = o13;
  out[4U] = o14;
  out[5U] = o20;
  out[6U] = o21;
  out[7U] = o22;
  out[8U] = o23;
  out[9U] = o24;
}

static inline void Hacl_Impl_Curve25519_Field51_store_felem(uint64_t *u64s, uint64_t *f)
{
  uint64_t f0 = f[0U];
  uint64_t f1 = f[1U];
  uint64_t f2 = f[2U];
  uint64_t f3 = f[3U];
  uint64_t f4 = f[4U];
  uint64_t l_ = f0 + 0ULL;
  uint64_t tmp0 = l_ & 0x7ffffffffffffULL;
  uint64_t c0 = l_ >> 51U;
  uint64_t l_0 = f1 + c0;
  uint64_t tmp1 = l_0 & 0x7ffffffffffffULL;
  uint64_t c1 = l_0 >> 51U;
  uint64_t l_1 = f2 + c1;
  uint64_t tmp2 = l_1 & 0x7ffffffffffffULL;
  uint64_t c2 = l_1 >> 51U;
  uint64_t l_2 = f3 + c2;
  uint64_t tmp3 = l_2 & 0x7ffffffffffffULL;
  uint64_t c3 = l_2 >> 51U;
  uint64_t l_3 = f4 + c3;
  uint64_t tmp4 = l_3 & 0x7ffffffffffffULL;
  uint64_t c4 = l_3 >> 51U;
  uint64_t l_4 = tmp0 + c4 * 19ULL;
  uint64_t tmp0_ = l_4 & 0x7ffffffffffffULL;
  uint64_t c5 = l_4 >> 51U;
  uint64_t f01 = tmp0_;
  uint64_t f11 = tmp1 + c5;
  uint64_t f21 = tmp2;
  uint64_t f31 = tmp3;
  uint64_t f41 = tmp4;
  uint64_t m0 = FStar_UInt64_gte_mask(f01, 0x7ffffffffffedULL);
  uint64_t m1 = FStar_UInt64_eq_mask(f11, 0x7ffffffffffffULL);
  uint64_t m2 = FStar_UInt64_eq_mask(f21, 0x7ffffffffffffULL);
  uint64_t m3 = FStar_UInt64_eq_mask(f31, 0x7ffffffffffffULL);
  uint64_t m4 = FStar_UInt64_eq_mask(f41, 0x7ffffffffffffULL);
  uint64_t mask = (((m0 & m1) & m2) & m3) & m4;
  uint64_t f0_ = f01 - (mask & 0x7ffffffffffedULL);
  uint64_t f1_ = f11 - (mask & 0x7ffffffffffffULL);
  uint64_t f2_ = f21 - (mask & 0x7ffffffffffffULL);
  uint64_t f3_ = f31 - (mask & 0x7ffffffffffffULL);
  uint64_t f4_ = f41 - (mask & 0x7ffffffffffffULL);
  uint64_t f02 = f0_;
  uint64_t f12 = f1_;
  uint64_t f22 = f2_;
  uint64_t f32 = f3_;
  uint64_t f42 = f4_;
  uint64_t o00 = f02 | f12 << 51U;
  uint64_t o10 = f12 >> 13U | f22 << 38U;
  uint64_t o20 = f22 >> 26U | f32 << 25U;
  uint64_t o30 = f32 >> 39U | f42 << 12U;
  uint64_t o0 = o00;
  uint64_t o1 = o10;
  uint64_t o2 = o20;
  uint64_t o3 = o30;
  u64s[0U] = o0;
  u64s[1U] = o1;
  u64s[2U] = o2;
  u64s[3U] = o3;
}

static inline void
Hacl_Impl_Curve25519_Field51_cswap2(uint64_t bit, uint64_t *p1, uint64_t *p2)
{
  uint64_t mask = 0ULL - bit;
  KRML_MAYBE_FOR10(i,
    0U,
    10U,
    1U,
    uint64_t dummy = mask & (p1[i] ^ p2[i]);
    p1[i] = p1[i] ^ dummy;
    p2[i] = p2[i] ^ dummy;);
}

#if defined(__cplusplus)
}
#endif

#define __internal_Hacl_Bignum25519_51_H_DEFINED
#endif
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __internal_Hacl_Curve25519_51_H
#define __internal_Hacl_Curve25519_51_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>


void
Hacl_Curve25519_51_fsquare_times(
  uint64_t *o,
  uint64_t *inp,
  FStar_UInt128_uint128 *tmp,
  uint32_t n
);

void Hacl_Curve25519_51_finv(uint64_t *o, uint64_t *i, FStar_UInt128_uint128 *tmp);

#if defined(__cplusplus)
}
#endif

#define __internal_Hacl_Curve25519_51_H_DEFINED
#endif
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __Hacl_Curve25519_51_H
#define __Hacl_Curve25519_51_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>

/**
Compute the scalar multiple of a point.

@param out Pointer to 32 bytes of memory, allocated by the caller, where the resulting point is written to.
@param priv Pointer to 32 bytes of memory where the secret/private key is read from.
@param pub Pointer to 32 bytes of memory where the public point is read from.
*/
void Hacl_Curve25519_51_scalarmult(uint8_t *out, uint8_t *priv, uint8_t *pub);

/**
Calculate a public point from a secret/private key.

This computes a scalar multiplication of the secret/private key with the curve's basepoint.

@param pub Pointer to 32 bytes of memory, allocated by the caller, where the resulting point is written to.
@param priv Pointer to 32 bytes of memory where the secret/private key is read from.
*/
void Hacl_Curve25519_51_secret_to_public(uint8_t *pub, uint8_t *priv);

/**
Execute the diffie-hellmann key exchange.

@param out Pointer to 32 bytes of memory, allocated by the caller, where the resulting point is written to.
@param priv Pointer to 32 bytes of memory where **our** secret/private key is read from.
@param pub Pointer to 32 bytes of memory where **their** public point is read from.
*/
bool Hacl_Curve25519_51_ecdh(uint8_t *out, uint8_t *priv, uint8_t *pub);

#if defined(__cplusplus)
}
#endif

#define __Hacl_Curve25519_51_H_DEFINED
#endif
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __internal_Hacl_Hash_SHA2_H
#define __internal_Hacl_Hash_SHA2_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>


static const
uint32_t
Hacl_Hash_SHA2_h224[8U] =
  {
    0xc1059ed8U, 0x367cd507U, 0x3070dd17U, 0xf70e5939U, 0xffc00b31U, 0x68581511U, 0x64f98fa7U,
    0xbefa4fa4U
  };

static const
uint32_t
Hacl_Hash_SHA2_h256[8U] =
  {
    0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU,
    0x5be0cd19U
  };

static const
uint64_t
Hacl_Hash_SHA2_h384[8U] =
  {
    0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL, 0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
    0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL, 0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL
  };

static const
uint64_t
Hacl_Hash_SHA2_h512[8U] =
  {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
  };

static const
uint32_t
Hacl_Hash_SHA2_k224_256[64U] =
  {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U
  };

static const
uint64_t
Hacl_Hash_SHA2_k384_512[80U] =
  {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
  };

void Hacl_Hash_SHA2_sha256_init(uint32_t *hash);

void Hacl_Hash_SHA2_sha256_update_nblocks(uint32_t len, uint8_t *b, uint32_t *st);

void
Hacl_Hash_SHA2_sha256_update_last(uint64_t totlen, uint32_t len, uint8_t *b, uint32_t *hash);

void Hacl_Hash_SHA2_sha256_finish(uint32_t *st, uint8_t *h);

void Hacl_Hash_SHA2_sha224_init(uint32_t *hash);

void Hacl_Hash_SHA2_sha224_update_nblocks(uint32_t len, uint8_t *b, uint32_t *st);

void
Hacl_Hash_SHA2_sha224_update_last(uint64_t totlen, uint32_t len, uint8_t *b, uint32_t *st);

void Hacl_Hash_SHA2_sha224_finish(uint32_t *st, uint8_t *h);

void Hacl_Hash_SHA2_sha512_init(uint64_t *hash);

void Hacl_Hash_SHA2_sha512_update_nblocks(uint32_t len, uint8_t *b, uint64_t *st);

void
Hacl_Hash_SHA2_sha512_update_last(
  FStar_UInt128_uint128 totlen,
  uint32_t len,
  uint8_t *b,
  uint64_t *hash
);

void Hacl_Hash_SHA2_sha512_finish(uint64_t *st, uint8_t *h);

void Hacl_Hash_SHA2_sha384_init(uint64_t *hash);

void Hacl_Hash_SHA2_sha384_update_nblocks(uint32_t len, uint8_t *b, uint64_t *st);

void
Hacl_Hash_SHA2_sha384_update_last(
  FStar_UInt128_uint128 totlen,
  uint32_t len,
  uint8_t *b,
  uint64_t *st
);

void Hacl_Hash_SHA2_sha384_finish(uint64_t *st, uint8_t *h);

#if defined(__cplusplus)
}
#endif

#define __internal_Hacl_Hash_SHA2_H_DEFINED
#endif
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __Hacl_Hash_SHA2_H
#define __Hacl_Hash_SHA2_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>


typedef Hacl_Streaming_MD_state_32 Hacl_Hash_SHA2_state_t_224;

typedef Hacl_Streaming_MD_state_32 Hacl_Hash_SHA2_state_t_256;

typedef Hacl_Streaming_MD_state_64 Hacl_Hash_SHA2_state_t_384;

typedef Hacl_Streaming_MD_state_64 Hacl_Hash_SHA2_state_t_512;

/**
Allocate initial state for the SHA2_256 hash. The state is to be freed by
calling `free_256`.
*/
Hacl_Streaming_MD_state_32 *Hacl_Hash_SHA2_malloc_256(void);

/**
Copies the state passed as argument into a newly allocated state (deep copy).
The state is to be freed by calling `free_256`. Cloning the state this way is
useful, for instance, if your control-flow diverges and you need to feed
more (different) data into the hash in each branch.
*/
Hacl_Streaming_MD_state_32 *Hacl_Hash_SHA2_copy_256(Hacl_Streaming_MD_state_32 *state);

/**
Reset an existing state to the initial hash state with empty data.
*/
void Hacl_Hash_SHA2_reset_256(Hacl_Streaming_MD_state_32 *state);

/**
Feed an arbitrary amount of data into the hash. This function returns 0 for
success, or 1 if the combined length of all of the data passed to `update_256`
(since the last call to `reset_256`) exceeds 2^61-1 bytes.

This function is identical to the update function for SHA2_224.
*/
Hacl_Streaming_Types_error_code
Hacl_Hash_SHA2_update_256(
  Hacl_Streaming_MD_state_32 *state,
  uint8_t *input,
  uint32_t input_len
);

/**
Write the resulting hash into `output`, an array of 32 bytes. The state remains
valid after a call to `digest_256`, meaning the user may feed more data into
the hash via `update_256`. (The digest_256 function operates on an internal copy of
the state and therefore does not invalidate the client-held state `p`.)
*/
void Hacl_Hash_SHA2_digest_256(Hacl_Streaming_MD_state_32 *state, uint8_t *output);

/**
Free a state allocated with `malloc_256`.

This function is identical to the free function for SHA2_224.
*/
void Hacl_Hash_SHA2_free_256(Hacl_Streaming_MD_state_32 *state);

/**
Hash `input`, of len `input_len`, into `output`, an array of 32 bytes.
*/
void Hacl_Hash_SHA2_hash_256(uint8_t *output, uint8_t *input, uint32_t input_len);

Hacl_Streaming_MD_state_32 *Hacl_Hash_SHA2_malloc_224(void);

void Hacl_Hash_SHA2_reset_224(Hacl_Streaming_MD_state_32 *state);

Hacl_Streaming_Types_error_code
Hacl_Hash_SHA2_update_224(
  Hacl_Streaming_MD_state_32 *state,
  uint8_t *input,
  uint32_t input_len
);

/**
Write the resulting hash into `output`, an array of 28 bytes. The state remains
valid after a call to `digest_224`, meaning the user may feed more data into
the hash via `update_224`.
*/
void Hacl_Hash_SHA2_digest_224(Hacl_Streaming_MD_state_32 *state, uint8_t *output);

void Hacl_Hash_SHA2_free_224(Hacl_Streaming_MD_state_32 *state);

/**
Hash `input`, of len `input_len`, into `output`, an array of 28 bytes.
*/
void Hacl_Hash_SHA2_hash_224(uint8_t *output, uint8_t *input, uint32_t input_len);

Hacl_Streaming_MD_state_64 *Hacl_Hash_SHA2_malloc_512(void);

/**
Copies the state passed as argument into a newly allocated state (deep copy).
The state is to be freed by calling `free_512`. Cloning the state this way is
useful, for instance, if your control-flow diverges and you need to feed
more (different) data into the hash in each branch.
*/
Hacl_Streaming_MD_state_64 *Hacl_Hash_SHA2_copy_512(Hacl_Streaming_MD_state_64 *state);

void Hacl_Hash_SHA2_reset_512(Hacl_Streaming_MD_state_64 *state);

/**
Feed an arbitrary amount of data into the hash. This function returns 0 for
success, or 1 if the combined length of all of the data passed to `update_512`
(since the last call to `reset_512`) exceeds 2^125-1 bytes.

This function is identical to the update function for SHA2_384.
*/
Hacl_Streaming_Types_error_code
Hacl_Hash_SHA2_update_512(
  Hacl_Streaming_MD_state_64 *state,
  uint8_t *input,
  uint32_t input_len
);

/**
Write the resulting hash into `output`, an array of 64 bytes. The state remains
valid after a call to `digest_512`, meaning the user may feed more data into
the hash via `update_512`. (The digest_512 function operates on an internal copy of
the state and therefore does not invalidate the client-held state `p`.)
*/
void Hacl_Hash_SHA2_digest_512(Hacl_Streaming_MD_state_64 *state, uint8_t *output);

/**
Free a state allocated with `malloc_512`.

This function is identical to the free function for SHA2_384.
*/
void Hacl_Hash_SHA2_free_512(Hacl_Streaming_MD_state_64 *state);

/**
Hash `input`, of len `input_len`, into `output`, an array of 64 bytes.
*/
void Hacl_Hash_SHA2_hash_512(uint8_t *output, uint8_t *input, uint32_t input_len);

Hacl_Streaming_MD_state_64 *Hacl_Hash_SHA2_malloc_384(void);

void Hacl_Hash_SHA2_reset_384(Hacl_Streaming_MD_state_64 *state);

Hacl_Streaming_Types_error_code
Hacl_Hash_SHA2_update_384(
  Hacl_Streaming_MD_state_64 *state,
  uint8_t *input,
  uint32_t input_len
);

/**
Write the resulting hash into `output`, an array of 48 bytes. The state remains
valid after a call to `digest_384`, meaning the user may feed more data into
the hash via `update_384`.
*/
void Hacl_Hash_SHA2_digest_384(Hacl_Streaming_MD_state_64 *state, uint8_t *output);

void Hacl_Hash_SHA2_free_384(Hacl_Streaming_MD_state_64 *state);

/**
Hash `input`, of len `input_len`, into `output`, an array of 48 bytes.
*/
void Hacl_Hash_SHA2_hash_384(uint8_t *output, uint8_t *input, uint32_t input_len);

#if defined(__cplusplus)
}
#endif

#define __Hacl_Hash_SHA2_H_DEFINED
#endif
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __internal_Hacl_Ed25519_PrecompTable_H
#define __internal_Hacl_Ed25519_PrecompTable_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>

static const
uint64_t
Hacl_Ed25519_PrecompTable_precomp_basepoint_table_w4[320U] =
  {
    0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 1ULL, 0ULL, 0ULL, 0ULL, 0ULL, 1ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL,
    0ULL, 0ULL, 0ULL, 0ULL, 1738742601995546ULL, 1146398526822698ULL, 2070867633025821ULL,
    562264141797630ULL, 587772402128613ULL, 1801439850948184ULL, 1351079888211148ULL,
    450359962737049ULL, 900719925474099ULL, 1801439850948198ULL, 1ULL, 0ULL, 0ULL, 0ULL, 0ULL,
    1841354044333475ULL, 16398895984059ULL, 755974180946558ULL, 900171276175154ULL,
    1821297809914039ULL, 1661154287933054ULL, 284530020860578ULL, 1390261174866914ULL,
    1524110943907984ULL, 1045603498418422ULL, 928651508580478ULL, 1383326941296346ULL,
    961937908925785ULL, 80455759693706ULL, 904734540352947ULL, 1507481815385608ULL,
    2223447444246085ULL, 1083941587175919ULL, 2059929906842505ULL, 1581435440146976ULL,
    782730187692425ULL, 9928394897574ULL, 1539449519985236ULL, 1923587931078510ULL,
    552919286076056ULL, 376925408065760ULL, 447320488831784ULL, 1362918338468019ULL,
    1470031896696846ULL, 2189796996539902ULL, 1337552949959847ULL, 1762287177775726ULL,
    237994495816815ULL, 1277840395970544ULL, 543972849007241ULL, 1224692671618814ULL,
    162359533289271ULL, 282240927125249ULL, 586909166382289ULL, 17726488197838ULL,
    377014554985659ULL, 1433835303052512ULL, 702061469493692ULL, 1142253108318154ULL,
    318297794307551ULL, 954362646308543ULL, 517363881452320ULL, 1868013482130416ULL,
    262562472373260ULL, 902232853249919ULL, 2107343057055746ULL, 462368348619024ULL,
    1893758677092974ULL, 2177729767846389ULL, 2168532543559143ULL, 443867094639821ULL,
    730169342581022ULL, 1564589016879755ULL, 51218195700649ULL, 76684578423745ULL,
    560266272480743ULL, 922517457707697ULL, 2066645939860874ULL, 1318277348414638ULL,
    1576726809084003ULL, 1817337608563665ULL, 1874240939237666ULL, 754733726333910ULL,
    97085310406474ULL, 751148364309235ULL, 1622159695715187ULL, 1444098819684916ULL,
    130920805558089ULL, 1260449179085308ULL, 1860021740768461ULL, 110052860348509ULL,
    193830891643810ULL, 164148413933881ULL, 180017794795332ULL, 1523506525254651ULL,
    465981629225956ULL, 559733514964572ULL, 1279624874416974ULL, 2026642326892306ULL,
    1425156829982409ULL, 2160936383793147ULL, 1061870624975247ULL, 2023497043036941ULL,
    117942212883190ULL, 490339622800774ULL, 1729931303146295ULL, 422305932971074ULL,
    529103152793096ULL, 1211973233775992ULL, 721364955929681ULL, 1497674430438813ULL,
    342545521275073ULL, 2102107575279372ULL, 2108462244669966ULL, 1382582406064082ULL,
    2206396818383323ULL, 2109093268641147ULL, 10809845110983ULL, 1605176920880099ULL,
    744640650753946ULL, 1712758897518129ULL, 373410811281809ULL, 648838265800209ULL,
    813058095530999ULL, 513987632620169ULL, 465516160703329ULL, 2136322186126330ULL,
    1979645899422932ULL, 1197131006470786ULL, 1467836664863979ULL, 1340751381374628ULL,
    1810066212667962ULL, 1009933588225499ULL, 1106129188080873ULL, 1388980405213901ULL,
    533719246598044ULL, 1169435803073277ULL, 198920999285821ULL, 487492330629854ULL,
    1807093008537778ULL, 1540899012923865ULL, 2075080271659867ULL, 1527990806921523ULL,
    1323728742908002ULL, 1568595959608205ULL, 1388032187497212ULL, 2026968840050568ULL,
    1396591153295755ULL, 820416950170901ULL, 520060313205582ULL, 2016404325094901ULL,
    1584709677868520ULL, 272161374469956ULL, 1567188603996816ULL, 1986160530078221ULL,
    553930264324589ULL, 1058426729027503ULL, 8762762886675ULL, 2216098143382988ULL,
    1835145266889223ULL, 1712936431558441ULL, 1017009937844974ULL, 585361667812740ULL,
    2114711541628181ULL, 2238729632971439ULL, 121257546253072ULL, 847154149018345ULL,
    211972965476684ULL, 287499084460129ULL, 2098247259180197ULL, 839070411583329ULL,
    339551619574372ULL, 1432951287640743ULL, 526481249498942ULL, 931991661905195ULL,
    1884279965674487ULL, 200486405604411ULL, 364173020594788ULL, 518034455936955ULL,
    1085564703965501ULL, 16030410467927ULL, 604865933167613ULL, 1695298441093964ULL,
    498856548116159ULL, 2193030062787034ULL, 1706339802964179ULL, 1721199073493888ULL,
    820740951039755ULL, 1216053436896834ULL, 23954895815139ULL, 1662515208920491ULL,
    1705443427511899ULL, 1957928899570365ULL, 1189636258255725ULL, 1795695471103809ULL,
    1691191297654118ULL, 282402585374360ULL, 460405330264832ULL, 63765529445733ULL,
    469763447404473ULL, 733607089694996ULL, 685410420186959ULL, 1096682630419738ULL,
    1162548510542362ULL, 1020949526456676ULL, 1211660396870573ULL, 613126398222696ULL,
    1117829165843251ULL, 742432540886650ULL, 1483755088010658ULL, 942392007134474ULL,
    1447834130944107ULL, 489368274863410ULL, 23192985544898ULL, 648442406146160ULL,
    785438843373876ULL, 249464684645238ULL, 170494608205618ULL, 335112827260550ULL,
    1462050123162735ULL, 1084803668439016ULL, 853459233600325ULL, 215777728187495ULL,
    1965759433526974ULL, 1349482894446537ULL, 694163317612871ULL, 860536766165036ULL,
    1178788094084321ULL, 1652739626626996ULL, 2115723946388185ULL, 1577204379094664ULL,
    1083882859023240ULL, 1768759143381635ULL, 1737180992507258ULL, 246054513922239ULL,
    577253134087234ULL, 356340280578042ULL, 1638917769925142ULL, 223550348130103ULL,
    470592666638765ULL, 22663573966996ULL, 596552461152400ULL, 364143537069499ULL, 3942119457699ULL,
    107951982889287ULL, 1843471406713209ULL, 1625773041610986ULL, 1466141092501702ULL,
    1043024095021271ULL, 310429964047508ULL, 98559121500372ULL, 152746933782868ULL,
    259407205078261ULL, 828123093322585ULL, 1576847274280091ULL, 1170871375757302ULL,
    1588856194642775ULL, 984767822341977ULL, 1141497997993760ULL, 809325345150796ULL,
    1879837728202511ULL, 201340910657893ULL, 1079157558888483ULL, 1052373448588065ULL,
    1732036202501778ULL, 2105292670328445ULL, 679751387312402ULL, 1679682144926229ULL,
    1695823455818780ULL, 498852317075849ULL, 1786555067788433ULL, 1670727545779425ULL,
    117945875433544ULL, 407939139781844ULL, 854632120023778ULL, 1413383148360437ULL,
    286030901733673ULL, 1207361858071196ULL, 461340408181417ULL, 1096919590360164ULL,
    1837594897475685ULL, 533755561544165ULL, 1638688042247712ULL, 1431653684793005ULL,
    1036458538873559ULL, 390822120341779ULL, 1920929837111618ULL, 543426740024168ULL,
    645751357799929ULL, 2245025632994463ULL, 1550778638076452ULL, 223738153459949ULL,
    1337209385492033ULL, 1276967236456531ULL, 1463815821063071ULL, 2070620870191473ULL,
    1199170709413753ULL, 273230877394166ULL, 1873264887608046ULL, 890877152910775ULL
  };

static const
uint64_t
Hacl_Ed25519_PrecompTable_precomp_g_pow2_64_table_w4[320U] =
  {
    0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 1ULL, 0ULL, 0ULL, 0ULL, 0ULL, 1ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL,
    0ULL, 0ULL, 0ULL, 0ULL, 13559344787725ULL, 2051621493703448ULL, 1947659315640708ULL,
    626856790370168ULL, 1592804284034836ULL, 1781728767459187ULL, 278818420518009ULL,
    2038030359908351ULL, 910625973862690ULL, 471887343142239ULL, 1298543306606048ULL,
    794147365642417ULL, 129968992326749ULL, 523140861678572ULL, 1166419653909231ULL,
    2009637196928390ULL, 1288020222395193ULL, 1007046974985829ULL, 208981102651386ULL,
    2074009315253380ULL, 1564056062071967ULL, 276822668750618ULL, 206621292512572ULL,
    470304361809269ULL, 895215438398493ULL, 1527859053868686ULL, 1624967223409369ULL,
    811821865979736ULL, 350450534838340ULL, 219143807921807ULL, 507994540371254ULL,
    986513794574720ULL, 1142661369967121ULL, 621278293399257ULL, 556189161519781ULL,
    351964007865066ULL, 2011573453777822ULL, 1367125527151537ULL, 1691316722438196ULL,
    731328817345164ULL, 1284781192709232ULL, 478439299539269ULL, 204842178076429ULL,
    2085125369913651ULL, 1980773492792985ULL, 1480264409524940ULL, 688389585376233ULL,
    612962643526972ULL, 165595382536676ULL, 1850300069212263ULL, 1176357203491551ULL,
    1880164984292321ULL, 10786153104736ULL, 1242293560510203ULL, 1358399951884084ULL,
    1901358796610357ULL, 1385092558795806ULL, 1734893785311348ULL, 2046201851951191ULL,
    1233811309557352ULL, 1531160168656129ULL, 1543287181303358ULL, 516121446374119ULL,
    723422668089935ULL, 1228176774959679ULL, 1598014722726267ULL, 1630810326658412ULL,
    1343833067463760ULL, 1024397964362099ULL, 1157142161346781ULL, 56422174971792ULL,
    544901687297092ULL, 1291559028869009ULL, 1336918672345120ULL, 1390874603281353ULL,
    1127199512010904ULL, 992644979940964ULL, 1035213479783573ULL, 36043651196100ULL,
    1220961519321221ULL, 1348190007756977ULL, 579420200329088ULL, 1703819961008985ULL,
    1993919213460047ULL, 2225080008232251ULL, 392785893702372ULL, 464312521482632ULL,
    1224525362116057ULL, 810394248933036ULL, 932513521649107ULL, 592314953488703ULL,
    586334603791548ULL, 1310888126096549ULL, 650842674074281ULL, 1596447001791059ULL,
    2086767406328284ULL, 1866377645879940ULL, 1721604362642743ULL, 738502322566890ULL,
    1851901097729689ULL, 1158347571686914ULL, 2023626733470827ULL, 329625404653699ULL,
    563555875598551ULL, 516554588079177ULL, 1134688306104598ULL, 186301198420809ULL,
    1339952213563300ULL, 643605614625891ULL, 1947505332718043ULL, 1722071694852824ULL,
    601679570440694ULL, 1821275721236351ULL, 1808307842870389ULL, 1654165204015635ULL,
    1457334100715245ULL, 217784948678349ULL, 1820622417674817ULL, 1946121178444661ULL,
    597980757799332ULL, 1745271227710764ULL, 2010952890941980ULL, 339811849696648ULL,
    1066120666993872ULL, 261276166508990ULL, 323098645774553ULL, 207454744271283ULL,
    941448672977675ULL, 71890920544375ULL, 840849789313357ULL, 1223996070717926ULL,
    196832550853408ULL, 115986818309231ULL, 1586171527267675ULL, 1666169080973450ULL,
    1456454731176365ULL, 44467854369003ULL, 2149656190691480ULL, 283446383597589ULL,
    2040542647729974ULL, 305705593840224ULL, 475315822269791ULL, 648133452550632ULL,
    169218658835720ULL, 24960052338251ULL, 938907951346766ULL, 425970950490510ULL,
    1037622011013183ULL, 1026882082708180ULL, 1635699409504916ULL, 1644776942870488ULL,
    2151820331175914ULL, 824120674069819ULL, 835744976610113ULL, 1991271032313190ULL,
    96507354724855ULL, 400645405133260ULL, 343728076650825ULL, 1151585441385566ULL,
    1403339955333520ULL, 230186314139774ULL, 1736248861506714ULL, 1010804378904572ULL,
    1394932289845636ULL, 1901351256960852ULL, 2187471430089807ULL, 1003853262342670ULL,
    1327743396767461ULL, 1465160415991740ULL, 366625359144534ULL, 1534791405247604ULL,
    1790905930250187ULL, 1255484115292738ULL, 2223291365520443ULL, 210967717407408ULL,
    26722916813442ULL, 1919574361907910ULL, 468825088280256ULL, 2230011775946070ULL,
    1628365642214479ULL, 568871869234932ULL, 1066987968780488ULL, 1692242903745558ULL,
    1678903997328589ULL, 214262165888021ULL, 1929686748607204ULL, 1790138967989670ULL,
    1790261616022076ULL, 1559824537553112ULL, 1230364591311358ULL, 147531939886346ULL,
    1528207085815487ULL, 477957922927292ULL, 285670243881618ULL, 264430080123332ULL,
    1163108160028611ULL, 373201522147371ULL, 34903775270979ULL, 1750870048600662ULL,
    1319328308741084ULL, 1547548634278984ULL, 1691259592202927ULL, 2247758037259814ULL,
    329611399953677ULL, 1385555496268877ULL, 2242438354031066ULL, 1329523854843632ULL,
    399895373846055ULL, 678005703193452ULL, 1496357700997771ULL, 71909969781942ULL,
    1515391418612349ULL, 470110837888178ULL, 1981307309417466ULL, 1259888737412276ULL,
    669991710228712ULL, 1048546834514303ULL, 1678323291295512ULL, 2172033978088071ULL,
    1529278455500556ULL, 901984601941894ULL, 780867622403807ULL, 550105677282793ULL,
    975860231176136ULL, 525188281689178ULL, 49966114807992ULL, 1776449263836645ULL,
    267851776380338ULL, 2225969494054620ULL, 2016794225789822ULL, 1186108678266608ULL,
    1023083271408882ULL, 1119289418565906ULL, 1248185897348801ULL, 1846081539082697ULL,
    23756429626075ULL, 1441999021105403ULL, 724497586552825ULL, 1287761623605379ULL,
    685303359654224ULL, 2217156930690570ULL, 163769288918347ULL, 1098423278284094ULL,
    1391470723006008ULL, 570700152353516ULL, 744804507262556ULL, 2200464788609495ULL,
    624141899161992ULL, 2249570166275684ULL, 378706441983561ULL, 122486379999375ULL,
    430741162798924ULL, 113847463452574ULL, 266250457840685ULL, 2120743625072743ULL,
    222186221043927ULL, 1964290018305582ULL, 1435278008132477ULL, 1670867456663734ULL,
    2009989552599079ULL, 1348024113448744ULL, 1158423886300455ULL, 1356467152691569ULL,
    306943042363674ULL, 926879628664255ULL, 1349295689598324ULL, 725558330071205ULL,
    536569987519948ULL, 116436990335366ULL, 1551888573800376ULL, 2044698345945451ULL,
    104279940291311ULL, 251526570943220ULL, 754735828122925ULL, 33448073576361ULL,
    994605876754543ULL, 546007584022006ULL, 2217332798409487ULL, 706477052561591ULL,
    131174619428653ULL, 2148698284087243ULL, 239290486205186ULL, 2161325796952184ULL,
    1713452845607994ULL, 1297861562938913ULL, 1779539876828514ULL, 1926559018603871ULL,
    296485747893968ULL, 1859208206640686ULL, 538513979002718ULL, 103998826506137ULL,
    2025375396538469ULL, 1370680785701206ULL, 1698557311253840ULL, 1411096399076595ULL,
    2132580530813677ULL, 2071564345845035ULL, 498581428556735ULL, 1136010486691371ULL,
    1927619356993146ULL
  };

static const
uint64_t
Hacl_Ed25519_PrecompTable_precomp_g_pow2_128_table_w4[320U] =
  {
    0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 1ULL, 0ULL, 0ULL, 0ULL, 0ULL, 1ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL,
    0ULL, 0ULL, 0ULL, 0ULL, 557549315715710ULL, 196756086293855ULL, 846062225082495ULL,
    1865068224838092ULL, 991112090754908ULL, 522916421512828ULL, 2098523346722375ULL,
    1135633221747012ULL, 858420432114866ULL, 186358544306082ULL, 1044420411868480ULL,
    2080052304349321ULL, 557301814716724ULL, 1305130257814057ULL, 2126012765451197ULL,
    1441004402875101ULL, 353948968859203ULL, 470765987164835ULL, 1507675957683570ULL,
    1086650358745097ULL, 1911913434398388ULL, 66086091117182ULL, 1137511952425971ULL,
    36958263512141ULL, 2193310025325256ULL, 1085191426269045ULL, 1232148267909446ULL,
    1449894406170117ULL, 1241416717139557ULL, 1940876999212868ULL, 829758415918121ULL,
    309608450373449ULL, 2228398547683851ULL, 1580623271960188ULL, 1675601502456740ULL,
    1360363115493548ULL, 1098397313096815ULL, 1809255384359797ULL, 1458261916834384ULL,
    210682545649705ULL, 1606836641068115ULL, 1230478270405318ULL, 1843192771547802ULL,
    1794596343564051ULL, 229060710252162ULL, 2169742775467181ULL, 701467067318072ULL,
    696018499035555ULL, 521051885339807ULL, 158329567901874ULL, 740426481832143ULL,
    1369811177301441ULL, 503351589084015ULL, 1781114827942261ULL, 1650493549693035ULL,
    2174562418345156ULL, 456517194809244ULL, 2052761522121179ULL, 2233342271123682ULL,
    1445872925177435ULL, 1131882576902813ULL, 220765848055241ULL, 1280259961403769ULL,
    1581497080160712ULL, 1477441080108824ULL, 218428165202767ULL, 1970598141278907ULL,
    643366736173069ULL, 2167909426804014ULL, 834993711408259ULL, 1922437166463212ULL,
    1900036281472252ULL, 513794844386304ULL, 1297904164900114ULL, 1147626295373268ULL,
    1910101606251299ULL, 182933838633381ULL, 806229530787362ULL, 155511666433200ULL,
    290522463375462ULL, 534373523491751ULL, 1302938814480515ULL, 1664979184120445ULL,
    304235649499423ULL, 339284524318609ULL, 1881717946973483ULL, 1670802286833842ULL,
    2223637120675737ULL, 135818919485814ULL, 1144856572842792ULL, 2234981613434386ULL,
    963917024969826ULL, 402275378284993ULL, 141532417412170ULL, 921537468739387ULL,
    963905069722607ULL, 1405442890733358ULL, 1567763927164655ULL, 1664776329195930ULL,
    2095924165508507ULL, 994243110271379ULL, 1243925610609353ULL, 1029845815569727ULL,
    1001968867985629ULL, 170368934002484ULL, 1100906131583801ULL, 1825190326449569ULL,
    1462285121182096ULL, 1545240767016377ULL, 797859025652273ULL, 1062758326657530ULL,
    1125600735118266ULL, 739325756774527ULL, 1420144485966996ULL, 1915492743426702ULL,
    752968196344993ULL, 882156396938351ULL, 1909097048763227ULL, 849058590685611ULL,
    840754951388500ULL, 1832926948808323ULL, 2023317100075297ULL, 322382745442827ULL,
    1569741341737601ULL, 1678986113194987ULL, 757598994581938ULL, 29678659580705ULL,
    1239680935977986ULL, 1509239427168474ULL, 1055981929287006ULL, 1894085471158693ULL,
    916486225488490ULL, 642168890366120ULL, 300453362620010ULL, 1858797242721481ULL,
    2077989823177130ULL, 510228455273334ULL, 1473284798689270ULL, 5173934574301ULL,
    765285232030050ULL, 1007154707631065ULL, 1862128712885972ULL, 168873464821340ULL,
    1967853269759318ULL, 1489896018263031ULL, 592451806166369ULL, 1242298565603883ULL,
    1838918921339058ULL, 697532763910695ULL, 294335466239059ULL, 135687058387449ULL,
    2133734403874176ULL, 2121911143127699ULL, 20222476737364ULL, 1200824626476747ULL,
    1397731736540791ULL, 702378430231418ULL, 59059527640068ULL, 460992547183981ULL,
    1016125857842765ULL, 1273530839608957ULL, 96724128829301ULL, 1313433042425233ULL,
    3543822857227ULL, 761975685357118ULL, 110417360745248ULL, 1079634164577663ULL,
    2044574510020457ULL, 338709058603120ULL, 94541336042799ULL, 127963233585039ULL,
    94427896272258ULL, 1143501979342182ULL, 1217958006212230ULL, 2153887831492134ULL,
    1519219513255575ULL, 251793195454181ULL, 392517349345200ULL, 1507033011868881ULL,
    2208494254670752ULL, 1364389582694359ULL, 2214069430728063ULL, 1272814257105752ULL,
    741450148906352ULL, 1105776675555685ULL, 824447222014984ULL, 528745219306376ULL,
    589427609121575ULL, 1501786838809155ULL, 379067373073147ULL, 184909476589356ULL,
    1346887560616185ULL, 1932023742314082ULL, 1633302311869264ULL, 1685314821133069ULL,
    1836610282047884ULL, 1595571594397150ULL, 615441688872198ULL, 1926435616702564ULL,
    235632180396480ULL, 1051918343571810ULL, 2150570051687050ULL, 879198845408738ULL,
    1443966275205464ULL, 481362545245088ULL, 512807443532642ULL, 641147578283480ULL,
    1594276116945596ULL, 1844812743300602ULL, 2044559316019485ULL, 202620777969020ULL,
    852992984136302ULL, 1500869642692910ULL, 1085216217052457ULL, 1736294372259758ULL,
    2009666354486552ULL, 1262389020715248ULL, 1166527705256867ULL, 1409917450806036ULL,
    1705819160057637ULL, 1116901782584378ULL, 1278460472285473ULL, 257879811360157ULL,
    40314007176886ULL, 701309846749639ULL, 1380457676672777ULL, 631519782380272ULL,
    1196339573466793ULL, 955537708940017ULL, 532725633381530ULL, 641190593731833ULL,
    7214357153807ULL, 481922072107983ULL, 1634886189207352ULL, 1247659758261633ULL,
    1655809614786430ULL, 43105797900223ULL, 76205809912607ULL, 1936575107455823ULL,
    1107927314642236ULL, 2199986333469333ULL, 802974829322510ULL, 718173128143482ULL,
    539385184235615ULL, 2075693785611221ULL, 953281147333690ULL, 1623571637172587ULL,
    655274535022250ULL, 1568078078819021ULL, 101142125049712ULL, 1488441673350881ULL,
    1457969561944515ULL, 1492622544287712ULL, 2041460689280803ULL, 1961848091392887ULL,
    461003520846938ULL, 934728060399807ULL, 117723291519705ULL, 1027773762863526ULL,
    56765304991567ULL, 2184028379550479ULL, 1768767711894030ULL, 1304432068983172ULL,
    498080974452325ULL, 2134905654858163ULL, 1446137427202647ULL, 551613831549590ULL,
    680288767054205ULL, 1278113339140386ULL, 378149431842614ULL, 80520494426960ULL,
    2080985256348782ULL, 673432591799820ULL, 739189463724560ULL, 1847191452197509ULL,
    527737312871602ULL, 477609358840073ULL, 1891633072677946ULL, 1841456828278466ULL,
    2242502936489002ULL, 524791829362709ULL, 276648168514036ULL, 991706903257619ULL,
    512580228297906ULL, 1216855104975946ULL, 67030930303149ULL, 769593945208213ULL,
    2048873385103577ULL, 455635274123107ULL, 2077404927176696ULL, 1803539634652306ULL,
    1837579953843417ULL, 1564240068662828ULL, 1964310918970435ULL, 832822906252492ULL,
    1516044634195010ULL, 770571447506889ULL, 602215152486818ULL, 1760828333136947ULL,
    730156776030376ULL
  };

static const
uint64_t
Hacl_Ed25519_PrecompTable_precomp_g_pow2_192_table_w4[320U] =
  {
    0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 1ULL, 0ULL, 0ULL, 0ULL, 0ULL, 1ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL,
    0ULL, 0ULL, 0ULL, 0ULL, 1129953239743101ULL, 1240339163956160ULL, 61002583352401ULL,
    2017604552196030ULL, 1576867829229863ULL, 1508654942849389ULL, 270111619664077ULL,
    1253097517254054ULL, 721798270973250ULL, 161923365415298ULL, 828530877526011ULL,
    1494851059386763ULL, 662034171193976ULL, 1315349646974670ULL, 2199229517308806ULL,
    497078277852673ULL, 1310507715989956ULL, 1881315714002105ULL, 2214039404983803ULL,
    1331036420272667ULL, 296286697520787ULL, 1179367922639127ULL, 25348441419697ULL,
    2200984961703188ULL, 150893128908291ULL, 1978614888570852ULL, 1539657347172046ULL,
    553810196523619ULL, 246017573977646ULL, 1440448985385485ULL, 346049108099981ULL,
    601166606218546ULL, 855822004151713ULL, 1957521326383188ULL, 1114240380430887ULL,
    1349639675122048ULL, 957375954499040ULL, 111551795360136ULL, 618586733648988ULL,
    490708840688866ULL, 1267002049697314ULL, 1130723224930028ULL, 215603029480828ULL,
    1277138555414710ULL, 1556750324971322ULL, 1407903521793741ULL, 1836836546590749ULL,
    576500297444199ULL, 2074707599091135ULL, 1826239864380012ULL, 1935365705983312ULL,
    239501825683682ULL, 1594236669034980ULL, 1283078975055301ULL, 856745636255925ULL,
    1342128647959981ULL, 945216428379689ULL, 938746202496410ULL, 105775123333919ULL,
    1379852610117266ULL, 1770216827500275ULL, 1016017267535704ULL, 1902885522469532ULL,
    994184703730489ULL, 2227487538793763ULL, 53155967096055ULL, 1264120808114350ULL,
    1334928769376729ULL, 393911808079997ULL, 826229239481845ULL, 1827903006733192ULL,
    1449283706008465ULL, 1258040415217849ULL, 1641484112868370ULL, 1140150841968176ULL,
    391113338021313ULL, 162138667815833ULL, 742204396566060ULL, 110709233440557ULL,
    90179377432917ULL, 530511949644489ULL, 911568635552279ULL, 135869304780166ULL,
    617719999563692ULL, 1802525001631319ULL, 1836394639510490ULL, 1862739456475085ULL,
    1378284444664288ULL, 1617882529391756ULL, 876124429891172ULL, 1147654641445091ULL,
    1476943370400542ULL, 688601222759067ULL, 2120281968990205ULL, 1387113236912611ULL,
    2125245820685788ULL, 1030674016350092ULL, 1594684598654247ULL, 1165939511879820ULL,
    271499323244173ULL, 546587254515484ULL, 945603425742936ULL, 1242252568170226ULL,
    561598728058142ULL, 604827091794712ULL, 19869753585186ULL, 565367744708915ULL,
    536755754533603ULL, 1767258313589487ULL, 907952975936127ULL, 292851652613937ULL,
    163573546237963ULL, 837601408384564ULL, 591996990118301ULL, 2126051747693057ULL,
    182247548824566ULL, 908369044122868ULL, 1335442699947273ULL, 2234292296528612ULL,
    689537529333034ULL, 2174778663790714ULL, 1011407643592667ULL, 1856130618715473ULL,
    1557437221651741ULL, 2250285407006102ULL, 1412384213410827ULL, 1428042038612456ULL,
    962709733973660ULL, 313995703125919ULL, 1844969155869325ULL, 787716782673657ULL,
    622504542173478ULL, 930119043384654ULL, 2128870043952488ULL, 537781531479523ULL,
    1556666269904940ULL, 417333635741346ULL, 1986743846438415ULL, 877620478041197ULL,
    2205624582983829ULL, 595260668884488ULL, 2025159350373157ULL, 2091659716088235ULL,
    1423634716596391ULL, 653686638634080ULL, 1972388399989956ULL, 795575741798014ULL,
    889240107997846ULL, 1446156876910732ULL, 1028507012221776ULL, 1071697574586478ULL,
    1689630411899691ULL, 604092816502174ULL, 1909917373896122ULL, 1602544877643837ULL,
    1227177032923867ULL, 62684197535630ULL, 186146290753883ULL, 414449055316766ULL,
    1560555880866750ULL, 157579947096755ULL, 230526795502384ULL, 1197673369665894ULL,
    593779215869037ULL, 214638834474097ULL, 1796344443484478ULL, 493550548257317ULL,
    1628442824033694ULL, 1410811655893495ULL, 1009361960995171ULL, 604736219740352ULL,
    392445928555351ULL, 1254295770295706ULL, 1958074535046128ULL, 508699942241019ULL,
    739405911261325ULL, 1678760393882409ULL, 517763708545996ULL, 640040257898722ULL,
    384966810872913ULL, 407454748380128ULL, 152604679407451ULL, 185102854927662ULL,
    1448175503649595ULL, 100328519208674ULL, 1153263667012830ULL, 1643926437586490ULL,
    609632142834154ULL, 980984004749261ULL, 855290732258779ULL, 2186022163021506ULL,
    1254052618626070ULL, 1850030517182611ULL, 162348933090207ULL, 1948712273679932ULL,
    1331832516262191ULL, 1219400369175863ULL, 89689036937483ULL, 1554886057235815ULL,
    1520047528432789ULL, 81263957652811ULL, 146612464257008ULL, 2207945627164163ULL,
    919846660682546ULL, 1925694087906686ULL, 2102027292388012ULL, 887992003198635ULL,
    1817924871537027ULL, 746660005584342ULL, 753757153275525ULL, 91394270908699ULL,
    511837226544151ULL, 736341543649373ULL, 1256371121466367ULL, 1977778299551813ULL,
    817915174462263ULL, 1602323381418035ULL, 190035164572930ULL, 603796401391181ULL,
    2152666873671669ULL, 1813900316324112ULL, 1292622433358041ULL, 888439870199892ULL,
    978918155071994ULL, 534184417909805ULL, 466460084317313ULL, 1275223140288685ULL,
    786407043883517ULL, 1620520623925754ULL, 1753625021290269ULL, 751937175104525ULL,
    905301961820613ULL, 697059847245437ULL, 584919033981144ULL, 1272165506533156ULL,
    1532180021450866ULL, 1901407354005301ULL, 1421319720492586ULL, 2179081609765456ULL,
    2193253156667632ULL, 1080248329608584ULL, 2158422436462066ULL, 759167597017850ULL,
    545759071151285ULL, 641600428493698ULL, 943791424499848ULL, 469571542427864ULL,
    951117845222467ULL, 1780538594373407ULL, 614611122040309ULL, 1354826131886963ULL,
    221898131992340ULL, 1145699723916219ULL, 798735379961769ULL, 1843560518208287ULL,
    1424523160161545ULL, 205549016574779ULL, 2239491587362749ULL, 1918363582399888ULL,
    1292183072788455ULL, 1783513123192567ULL, 1584027954317205ULL, 1890421443925740ULL,
    1718459319874929ULL, 1522091040748809ULL, 399467600667219ULL, 1870973059066576ULL,
    287514433150348ULL, 1397845311152885ULL, 1880440629872863ULL, 709302939340341ULL,
    1813571361109209ULL, 86598795876860ULL, 1146964554310612ULL, 1590956584862432ULL,
    2097004628155559ULL, 656227622102390ULL, 1808500445541891ULL, 958336726523135ULL,
    2007604569465975ULL, 313504950390997ULL, 1399686004953620ULL, 1759732788465234ULL,
    1562539721055836ULL, 1575722765016293ULL, 793318366641259ULL, 443876859384887ULL,
    547308921989704ULL, 636698687503328ULL, 2179175835287340ULL, 498333551718258ULL,
    932248760026176ULL, 1612395686304653ULL, 2179774103745626ULL, 1359658123541018ULL,
    171488501802442ULL, 1625034951791350ULL, 520196922773633ULL, 1873787546341877ULL,
    303457823885368ULL
  };

static const
uint64_t
Hacl_Ed25519_PrecompTable_precomp_basepoint_table_w5[640U] =
  {
    0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 1ULL, 0ULL, 0ULL, 0ULL, 0ULL, 1ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL,
    0ULL, 0ULL, 0ULL, 0ULL, 1738742601995546ULL, 1146398526822698ULL, 2070867633025821ULL,
    562264141797630ULL, 587772402128613ULL, 1801439850948184ULL, 1351079888211148ULL,
    450359962737049ULL, 900719925474099ULL, 1801439850948198ULL, 1ULL, 0ULL, 0ULL, 0ULL, 0ULL,
    1841354044333475ULL, 16398895984059ULL, 755974180946558ULL, 900171276175154ULL,
    1821297809914039ULL, 1661154287933054ULL, 284530020860578ULL, 1390261174866914ULL,
    1524110943907984ULL, 1045603498418422ULL, 928651508580478ULL, 1383326941296346ULL,
    961937908925785ULL, 80455759693706ULL, 904734540352947ULL, 1507481815385608ULL,
    2223447444246085ULL, 1083941587175919ULL, 2059929906842505ULL, 1581435440146976ULL,
    782730187692425ULL, 9928394897574ULL, 1539449519985236ULL, 1923587931078510ULL,
    552919286076056ULL, 376925408065760ULL, 447320488831784ULL, 1362918338468019ULL,
    1470031896696846ULL, 2189796996539902ULL, 1337552949959847ULL, 1762287177775726ULL,
    237994495816815ULL, 1277840395970544ULL, 543972849007241ULL, 1224692671618814ULL,
    162359533289271ULL, 282240927125249ULL, 586909166382289ULL, 17726488197838ULL,
    377014554985659ULL, 1433835303052512ULL, 702061469493692ULL, 1142253108318154ULL,
    318297794307551ULL, 954362646308543ULL, 517363881452320ULL, 1868013482130416ULL,
    262562472373260ULL, 902232853249919ULL, 2107343057055746ULL, 462368348619024ULL,
    1893758677092974ULL, 2177729767846389ULL, 2168532543559143ULL, 443867094639821ULL,
    730169342581022ULL, 1564589016879755ULL, 51218195700649ULL, 76684578423745ULL,
    560266272480743ULL, 922517457707697ULL, 2066645939860874ULL, 1318277348414638ULL,
    1576726809084003ULL, 1817337608563665ULL, 1874240939237666ULL, 754733726333910ULL,
    97085310406474ULL, 751148364309235ULL, 1622159695715187ULL, 1444098819684916ULL,
    130920805558089ULL, 1260449179085308ULL, 1860021740768461ULL, 110052860348509ULL,
    193830891643810ULL, 164148413933881ULL, 180017794795332ULL, 1523506525254651ULL,
    465981629225956ULL, 559733514964572ULL, 1279624874416974ULL, 2026642326892306ULL,
    1425156829982409ULL, 2160936383793147ULL, 1061870624975247ULL, 2023497043036941ULL,
    117942212883190ULL, 490339622800774ULL, 1729931303146295ULL, 422305932971074ULL,
    529103152793096ULL, 1211973233775992ULL, 721364955929681ULL, 1497674430438813ULL,
    342545521275073ULL, 2102107575279372ULL, 2108462244669966ULL, 1382582406064082ULL,
    2206396818383323ULL, 2109093268641147ULL, 10809845110983ULL, 1605176920880099ULL,
    744640650753946ULL, 1712758897518129ULL, 373410811281809ULL, 648838265800209ULL,
    813058095530999ULL, 513987632620169ULL, 465516160703329ULL, 2136322186126330ULL,
    1979645899422932ULL, 1197131006470786ULL, 1467836664863979ULL, 1340751381374628ULL,
    1810066212667962ULL, 1009933588225499ULL, 1106129188080873ULL, 1388980405213901ULL,
    533719246598044ULL, 1169435803073277ULL, 198920999285821ULL, 487492330629854ULL,
    1807093008537778ULL, 1540899012923865ULL, 2075080271659867ULL, 1527990806921523ULL,
    1323728742908002ULL, 1568595959608205ULL, 1388032187497212ULL, 2026968840050568ULL,
    1396591153295755ULL, 820416950170901ULL, 520060313205582ULL, 2016404325094901ULL,
    1584709677868520ULL, 272161374469956ULL, 1567188603996816ULL, 1986160530078221ULL,
    553930264324589ULL, 1058426729027503ULL, 8762762886675ULL, 2216098143382988ULL,
    1835145266889223ULL, 1712936431558441ULL, 1017009937844974ULL, 585361667812740ULL,
    2114711541628181ULL, 2238729632971439ULL, 121257546253072ULL, 847154149018345ULL,
    211972965476684ULL, 287499084460129ULL, 2098247259180197ULL, 839070411583329ULL,
    339551619574372ULL, 1432951287640743ULL, 526481249498942ULL, 931991661905195ULL,
    1884279965674487ULL, 200486405604411ULL, 364173020594788ULL, 518034455936955ULL,
    1085564703965501ULL, 16030410467927ULL, 604865933167613ULL, 1695298441093964ULL,
    498856548116159ULL, 2193030062787034ULL, 1706339802964179ULL, 1721199073493888ULL,
    820740951039755ULL, 1216053436896834ULL, 23954895815139ULL, 1662515208920491ULL,
    1705443427511899ULL, 1957928899570365ULL, 1189636258255725ULL, 1795695471103809ULL,
    1691191297654118ULL, 282402585374360ULL, 460405330264832ULL, 63765529445733ULL,
    469763447404473ULL, 733607089694996ULL, 685410420186959ULL, 1096682630419738ULL,
    1162548510542362ULL, 1020949526456676ULL, 1211660396870573ULL, 613126398222696ULL,
    1117829165843251ULL, 742432540886650ULL, 1483755088010658ULL, 942392007134474ULL,
    1447834130944107ULL, 489368274863410ULL, 23192985544898ULL, 648442406146160ULL,
    785438843373876ULL, 249464684645238ULL, 170494608205618ULL, 335112827260550ULL,
    1462050123162735ULL, 1084803668439016ULL, 853459233600325ULL, 215777728187495ULL,
    1965759433526974ULL, 1349482894446537ULL, 694163317612871ULL, 860536766165036ULL,
    1178788094084321ULL, 1652739626626996ULL, 2115723946388185ULL, 1577204379094664ULL,
    1083882859023240ULL, 1768759143381635ULL, 1737180992507258ULL, 246054513922239ULL,
    577253134087234ULL, 356340280578042ULL, 1638917769925142ULL, 223550348130103ULL,
    470592666638765ULL, 22663573966996ULL, 596552461152400ULL, 364143537069499ULL, 3942119457699ULL,
    107951982889287ULL, 1843471406713209ULL, 1625773041610986ULL, 1466141092501702ULL,
    1043024095021271ULL, 310429964047508ULL, 98559121500372ULL, 152746933782868ULL,
    259407205078261ULL, 828123093322585ULL, 1576847274280091ULL, 1170871375757302ULL,
    1588856194642775ULL, 984767822341977ULL, 1141497997993760ULL, 809325345150796ULL,
    1879837728202511ULL, 201340910657893ULL, 1079157558888483ULL, 1052373448588065ULL,
    1732036202501778ULL, 2105292670328445ULL, 679751387312402ULL, 1679682144926229ULL,
    1695823455818780ULL, 498852317075849ULL, 1786555067788433ULL, 1670727545779425ULL,
    117945875433544ULL, 407939139781844ULL, 854632120023778ULL, 1413383148360437ULL,
    286030901733673ULL, 1207361858071196ULL, 461340408181417ULL, 1096919590360164ULL,
    1837594897475685ULL, 533755561544165ULL, 1638688042247712ULL, 1431653684793005ULL,
    1036458538873559ULL, 390822120341779ULL, 1920929837111618ULL, 543426740024168ULL,
    645751357799929ULL, 2245025632994463ULL, 1550778638076452ULL, 223738153459949ULL,
    1337209385492033ULL, 1276967236456531ULL, 1463815821063071ULL, 2070620870191473ULL,
    1199170709413753ULL, 273230877394166ULL, 1873264887608046ULL, 890877152910775ULL,
    983226445635730ULL, 44873798519521ULL, 697147127512130ULL, 961631038239304ULL,
    709966160696826ULL, 1706677689540366ULL, 502782733796035ULL, 812545535346033ULL,
    1693622521296452ULL, 1955813093002510ULL, 1259937612881362ULL, 1873032503803559ULL,
    1140330566016428ULL, 1675726082440190ULL, 60029928909786ULL, 170335608866763ULL,
    766444312315022ULL, 2025049511434113ULL, 2200845622430647ULL, 1201269851450408ULL,
    590071752404907ULL, 1400995030286946ULL, 2152637413853822ULL, 2108495473841983ULL,
    3855406710349ULL, 1726137673168580ULL, 51004317200100ULL, 1749082328586939ULL,
    1704088976144558ULL, 1977318954775118ULL, 2062602253162400ULL, 948062503217479ULL,
    361953965048030ULL, 1528264887238440ULL, 62582552172290ULL, 2241602163389280ULL,
    156385388121765ULL, 2124100319761492ULL, 388928050571382ULL, 1556123596922727ULL,
    979310669812384ULL, 113043855206104ULL, 2023223924825469ULL, 643651703263034ULL,
    2234446903655540ULL, 1577241261424997ULL, 860253174523845ULL, 1691026473082448ULL,
    1091672764933872ULL, 1957463109756365ULL, 530699502660193ULL, 349587141723569ULL,
    674661681919563ULL, 1633727303856240ULL, 708909037922144ULL, 2160722508518119ULL,
    1302188051602540ULL, 976114603845777ULL, 120004758721939ULL, 1681630708873780ULL,
    622274095069244ULL, 1822346309016698ULL, 1100921177951904ULL, 2216952659181677ULL,
    1844020550362490ULL, 1976451368365774ULL, 1321101422068822ULL, 1189859436282668ULL,
    2008801879735257ULL, 2219413454333565ULL, 424288774231098ULL, 359793146977912ULL,
    270293357948703ULL, 587226003677000ULL, 1482071926139945ULL, 1419630774650359ULL,
    1104739070570175ULL, 1662129023224130ULL, 1609203612533411ULL, 1250932720691980ULL,
    95215711818495ULL, 498746909028150ULL, 158151296991874ULL, 1201379988527734ULL,
    561599945143989ULL, 2211577425617888ULL, 2166577612206324ULL, 1057590354233512ULL,
    1968123280416769ULL, 1316586165401313ULL, 762728164447634ULL, 2045395244316047ULL,
    1531796898725716ULL, 315385971670425ULL, 1109421039396756ULL, 2183635256408562ULL,
    1896751252659461ULL, 840236037179080ULL, 796245792277211ULL, 508345890111193ULL,
    1275386465287222ULL, 513560822858784ULL, 1784735733120313ULL, 1346467478899695ULL,
    601125231208417ULL, 701076661112726ULL, 1841998436455089ULL, 1156768600940434ULL,
    1967853462343221ULL, 2178318463061452ULL, 481885520752741ULL, 675262828640945ULL,
    1033539418596582ULL, 1743329872635846ULL, 159322641251283ULL, 1573076470127113ULL,
    954827619308195ULL, 778834750662635ULL, 619912782122617ULL, 515681498488209ULL,
    1675866144246843ULL, 811716020969981ULL, 1125515272217398ULL, 1398917918287342ULL,
    1301680949183175ULL, 726474739583734ULL, 587246193475200ULL, 1096581582611864ULL,
    1469911826213486ULL, 1990099711206364ULL, 1256496099816508ULL, 2019924615195672ULL,
    1251232456707555ULL, 2042971196009755ULL, 214061878479265ULL, 115385726395472ULL,
    1677875239524132ULL, 756888883383540ULL, 1153862117756233ULL, 503391530851096ULL,
    946070017477513ULL, 1878319040542579ULL, 1101349418586920ULL, 793245696431613ULL,
    397920495357645ULL, 2174023872951112ULL, 1517867915189593ULL, 1829855041462995ULL,
    1046709983503619ULL, 424081940711857ULL, 2112438073094647ULL, 1504338467349861ULL,
    2244574127374532ULL, 2136937537441911ULL, 1741150838990304ULL, 25894628400571ULL,
    512213526781178ULL, 1168384260796379ULL, 1424607682379833ULL, 938677789731564ULL,
    872882241891896ULL, 1713199397007700ULL, 1410496326218359ULL, 854379752407031ULL,
    465141611727634ULL, 315176937037857ULL, 1020115054571233ULL, 1856290111077229ULL,
    2028366269898204ULL, 1432980880307543ULL, 469932710425448ULL, 581165267592247ULL,
    496399148156603ULL, 2063435226705903ULL, 2116841086237705ULL, 498272567217048ULL,
    1829438076967906ULL, 1573925801278491ULL, 460763576329867ULL, 1705264723728225ULL,
    999514866082412ULL, 29635061779362ULL, 1884233592281020ULL, 1449755591461338ULL,
    42579292783222ULL, 1869504355369200ULL, 495506004805251ULL, 264073104888427ULL,
    2088880861028612ULL, 104646456386576ULL, 1258445191399967ULL, 1348736801545799ULL,
    2068276361286613ULL, 884897216646374ULL, 922387476801376ULL, 1043886580402805ULL,
    1240883498470831ULL, 1601554651937110ULL, 804382935289482ULL, 512379564477239ULL,
    1466384519077032ULL, 1280698500238386ULL, 211303836685749ULL, 2081725624793803ULL,
    545247644516879ULL, 215313359330384ULL, 286479751145614ULL, 2213650281751636ULL,
    2164927945999874ULL, 2072162991540882ULL, 1443769115444779ULL, 1581473274363095ULL,
    434633875922699ULL, 340456055781599ULL, 373043091080189ULL, 839476566531776ULL,
    1856706858509978ULL, 931616224909153ULL, 1888181317414065ULL, 213654322650262ULL,
    1161078103416244ULL, 1822042328851513ULL, 915817709028812ULL, 1828297056698188ULL,
    1212017130909403ULL, 60258343247333ULL, 342085800008230ULL, 930240559508270ULL,
    1549884999174952ULL, 809895264249462ULL, 184726257947682ULL, 1157065433504828ULL,
    1209999630381477ULL, 999920399374391ULL, 1714770150788163ULL, 2026130985413228ULL,
    506776632883140ULL, 1349042668246528ULL, 1937232292976967ULL, 942302637530730ULL,
    160211904766226ULL, 1042724500438571ULL, 212454865139142ULL, 244104425172642ULL,
    1376990622387496ULL, 76126752421227ULL, 1027540886376422ULL, 1912210655133026ULL,
    13410411589575ULL, 1475856708587773ULL, 615563352691682ULL, 1446629324872644ULL,
    1683670301784014ULL, 1049873327197127ULL, 1826401704084838ULL, 2032577048760775ULL,
    1922203607878853ULL, 836708788764806ULL, 2193084654695012ULL, 1342923183256659ULL,
    849356986294271ULL, 1228863973965618ULL, 94886161081867ULL, 1423288430204892ULL,
    2016167528707016ULL, 1633187660972877ULL, 1550621242301752ULL, 340630244512994ULL,
    2103577710806901ULL, 221625016538931ULL, 421544147350960ULL, 580428704555156ULL,
    1479831381265617ULL, 518057926544698ULL, 955027348790630ULL, 1326749172561598ULL,
    1118304625755967ULL, 1994005916095176ULL, 1799757332780663ULL, 751343129396941ULL,
    1468672898746144ULL, 1451689964451386ULL, 755070293921171ULL, 904857405877052ULL,
    1276087530766984ULL, 403986562858511ULL, 1530661255035337ULL, 1644972908910502ULL,
    1370170080438957ULL, 139839536695744ULL, 909930462436512ULL, 1899999215356933ULL,
    635992381064566ULL, 788740975837654ULL, 224241231493695ULL, 1267090030199302ULL,
    998908061660139ULL, 1784537499699278ULL, 859195370018706ULL, 1953966091439379ULL,
    2189271820076010ULL, 2039067059943978ULL, 1526694380855202ULL, 2040321513194941ULL,
    329922071218689ULL, 1953032256401326ULL, 989631424403521ULL, 328825014934242ULL,
    9407151397696ULL, 63551373671268ULL, 1624728632895792ULL, 1608324920739262ULL,
    1178239350351945ULL, 1198077399579702ULL, 277620088676229ULL, 1775359437312528ULL,
    1653558177737477ULL, 1652066043408850ULL, 1063359889686622ULL, 1975063804860653ULL
  };

#if defined(__cplusplus)
}
#endif

#define __internal_Hacl_Ed25519_PrecompTable_H_DEFINED
#endif
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef __internal_Hacl_Ed25519_H
#define __internal_Hacl_Ed25519_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>


void Hacl_Bignum25519_reduce_513(uint64_t *a);

void Hacl_Bignum25519_inverse(uint64_t *out, uint64_t *a);

void Hacl_Bignum25519_load_51(uint64_t *output, uint8_t *input);

void Hacl_Bignum25519_store_51(uint8_t *output, uint64_t *input);

void Hacl_Impl_Ed25519_PointDouble_point_double(uint64_t *out, uint64_t *p);

void Hacl_Impl_Ed25519_PointAdd_point_add(uint64_t *out, uint64_t *p, uint64_t *q);

void Hacl_Impl_Ed25519_PointConstants_make_point_inf(uint64_t *b);

bool Hacl_Impl_Ed25519_PointDecompress_point_decompress(uint64_t *out, uint8_t *s);

void Hacl_Impl_Ed25519_PointCompress_point_compress(uint8_t *z, uint64_t *p);

bool Hacl_Impl_Ed25519_PointEqual_point_equal(uint64_t *p, uint64_t *q);

void Hacl_Impl_Ed25519_PointNegate_point_negate(uint64_t *p, uint64_t *out);

void Hacl_Impl_Ed25519_Ladder_point_mul(uint64_t *out, uint8_t *scalar, uint64_t *q);

#if defined(__cplusplus)
}
#endif

#define __internal_Hacl_Ed25519_H_DEFINED
#endif
/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */




static const uint8_t g25519[32U] = { 9U };

static void point_add_and_double(uint64_t *q, uint64_t *p01_tmp1, FStar_UInt128_uint128 *tmp2)
{
  uint64_t *nq = p01_tmp1;
  uint64_t *nq_p1 = p01_tmp1 + 10U;
  uint64_t *tmp1 = p01_tmp1 + 20U;
  uint64_t *x1 = q;
  uint64_t *x2 = nq;
  uint64_t *z2 = nq + 5U;
  uint64_t *z3 = nq_p1 + 5U;
  uint64_t *a = tmp1;
  uint64_t *b = tmp1 + 5U;
  uint64_t *ab = tmp1;
  uint64_t *dc = tmp1 + 10U;
  Hacl_Impl_Curve25519_Field51_fadd(a, x2, z2);
  Hacl_Impl_Curve25519_Field51_fsub(b, x2, z2);
  uint64_t *x3 = nq_p1;
  uint64_t *z31 = nq_p1 + 5U;
  uint64_t *d0 = dc;
  uint64_t *c0 = dc + 5U;
  Hacl_Impl_Curve25519_Field51_fadd(c0, x3, z31);
  Hacl_Impl_Curve25519_Field51_fsub(d0, x3, z31);
  Hacl_Impl_Curve25519_Field51_fmul2(dc, dc, ab, tmp2);
  Hacl_Impl_Curve25519_Field51_fadd(x3, d0, c0);
  Hacl_Impl_Curve25519_Field51_fsub(z31, d0, c0);
  uint64_t *a1 = tmp1;
  uint64_t *b1 = tmp1 + 5U;
  uint64_t *d = tmp1 + 10U;
  uint64_t *c = tmp1 + 15U;
  uint64_t *ab1 = tmp1;
  uint64_t *dc1 = tmp1 + 10U;
  Hacl_Impl_Curve25519_Field51_fsqr2(dc1, ab1, tmp2);
  Hacl_Impl_Curve25519_Field51_fsqr2(nq_p1, nq_p1, tmp2);
  a1[0U] = c[0U];
  a1[1U] = c[1U];
  a1[2U] = c[2U];
  a1[3U] = c[3U];
  a1[4U] = c[4U];
  Hacl_Impl_Curve25519_Field51_fsub(c, d, c);
  Hacl_Impl_Curve25519_Field51_fmul1(b1, c, 121665ULL);
  Hacl_Impl_Curve25519_Field51_fadd(b1, b1, d);
  Hacl_Impl_Curve25519_Field51_fmul2(nq, dc1, ab1, tmp2);
  Hacl_Impl_Curve25519_Field51_fmul(z3, z3, x1, tmp2);
}

static void point_double(uint64_t *nq, uint64_t *tmp1, FStar_UInt128_uint128 *tmp2)
{
  uint64_t *x2 = nq;
  uint64_t *z2 = nq + 5U;
  uint64_t *a = tmp1;
  uint64_t *b = tmp1 + 5U;
  uint64_t *d = tmp1 + 10U;
  uint64_t *c = tmp1 + 15U;
  uint64_t *ab = tmp1;
  uint64_t *dc = tmp1 + 10U;
  Hacl_Impl_Curve25519_Field51_fadd(a, x2, z2);
  Hacl_Impl_Curve25519_Field51_fsub(b, x2, z2);
  Hacl_Impl_Curve25519_Field51_fsqr2(dc, ab, tmp2);
  a[0U] = c[0U];
  a[1U] = c[1U];
  a[2U] = c[2U];
  a[3U] = c[3U];
  a[4U] = c[4U];
  Hacl_Impl_Curve25519_Field51_fsub(c, d, c);
  Hacl_Impl_Curve25519_Field51_fmul1(b, c, 121665ULL);
  Hacl_Impl_Curve25519_Field51_fadd(b, b, d);
  Hacl_Impl_Curve25519_Field51_fmul2(nq, dc, ab, tmp2);
}

static void montgomery_ladder(uint64_t *out, uint8_t *key, uint64_t *init)
{
  FStar_UInt128_uint128 tmp2[10U];
  for (uint32_t _i = 0U; _i < 10U; ++_i)
    tmp2[_i] = FStar_UInt128_uint64_to_uint128(0ULL);
  uint64_t p01_tmp1_swap[41U] = { 0U };
  uint64_t *p0 = p01_tmp1_swap;
  uint64_t *p01 = p01_tmp1_swap;
  uint64_t *p03 = p01;
  uint64_t *p11 = p01 + 10U;
  memcpy(p11, init, 10U * sizeof (uint64_t));
  uint64_t *x0 = p03;
  uint64_t *z0 = p03 + 5U;
  x0[0U] = 1ULL;
  x0[1U] = 0ULL;
  x0[2U] = 0ULL;
  x0[3U] = 0ULL;
  x0[4U] = 0ULL;
  z0[0U] = 0ULL;
  z0[1U] = 0ULL;
  z0[2U] = 0ULL;
  z0[3U] = 0ULL;
  z0[4U] = 0ULL;
  uint64_t *p01_tmp1 = p01_tmp1_swap;
  uint64_t *p01_tmp11 = p01_tmp1_swap;
  uint64_t *nq1 = p01_tmp1_swap;
  uint64_t *nq_p11 = p01_tmp1_swap + 10U;
  uint64_t *swap = p01_tmp1_swap + 40U;
  Hacl_Impl_Curve25519_Field51_cswap2(1ULL, nq1, nq_p11);
  point_add_and_double(init, p01_tmp11, tmp2);
  swap[0U] = 1ULL;
  for (uint32_t i = 0U; i < 251U; i++)
  {
    uint64_t *p01_tmp12 = p01_tmp1_swap;
    uint64_t *swap1 = p01_tmp1_swap + 40U;
    uint64_t *nq2 = p01_tmp12;
    uint64_t *nq_p12 = p01_tmp12 + 10U;
    uint64_t bit = (uint64_t)((uint32_t)key[(253U - i) / 8U] >> (253U - i) % 8U & 1U);
    uint64_t sw = swap1[0U] ^ bit;
    Hacl_Impl_Curve25519_Field51_cswap2(sw, nq2, nq_p12);
    point_add_and_double(init, p01_tmp12, tmp2);
    swap1[0U] = bit;
  }
  uint64_t sw = swap[0U];
  Hacl_Impl_Curve25519_Field51_cswap2(sw, nq1, nq_p11);
  uint64_t *nq10 = p01_tmp1;
  uint64_t *tmp1 = p01_tmp1 + 20U;
  point_double(nq10, tmp1, tmp2);
  point_double(nq10, tmp1, tmp2);
  point_double(nq10, tmp1, tmp2);
  memcpy(out, p0, 10U * sizeof (uint64_t));
}

void
Hacl_Curve25519_51_fsquare_times(
  uint64_t *o,
  uint64_t *inp,
  FStar_UInt128_uint128 *tmp,
  uint32_t n
)
{
  Hacl_Impl_Curve25519_Field51_fsqr(o, inp, tmp);
  for (uint32_t i = 0U; i < n - 1U; i++)
  {
    Hacl_Impl_Curve25519_Field51_fsqr(o, o, tmp);
  }
}

void Hacl_Curve25519_51_finv(uint64_t *o, uint64_t *i, FStar_UInt128_uint128 *tmp)
{
  uint64_t t1[20U] = { 0U };
  uint64_t *a1 = t1;
  uint64_t *b1 = t1 + 5U;
  uint64_t *t010 = t1 + 15U;
  FStar_UInt128_uint128 *tmp10 = tmp;
  Hacl_Curve25519_51_fsquare_times(a1, i, tmp10, 1U);
  Hacl_Curve25519_51_fsquare_times(t010, a1, tmp10, 2U);
  Hacl_Impl_Curve25519_Field51_fmul(b1, t010, i, tmp);
  Hacl_Impl_Curve25519_Field51_fmul(a1, b1, a1, tmp);
  Hacl_Curve25519_51_fsquare_times(t010, a1, tmp10, 1U);
  Hacl_Impl_Curve25519_Field51_fmul(b1, t010, b1, tmp);
  Hacl_Curve25519_51_fsquare_times(t010, b1, tmp10, 5U);
  Hacl_Impl_Curve25519_Field51_fmul(b1, t010, b1, tmp);
  uint64_t *b10 = t1 + 5U;
  uint64_t *c10 = t1 + 10U;
  uint64_t *t011 = t1 + 15U;
  FStar_UInt128_uint128 *tmp11 = tmp;
  Hacl_Curve25519_51_fsquare_times(t011, b10, tmp11, 10U);
  Hacl_Impl_Curve25519_Field51_fmul(c10, t011, b10, tmp);
  Hacl_Curve25519_51_fsquare_times(t011, c10, tmp11, 20U);
  Hacl_Impl_Curve25519_Field51_fmul(t011, t011, c10, tmp);
  Hacl_Curve25519_51_fsquare_times(t011, t011, tmp11, 10U);
  Hacl_Impl_Curve25519_Field51_fmul(b10, t011, b10, tmp);
  Hacl_Curve25519_51_fsquare_times(t011, b10, tmp11, 50U);
  Hacl_Impl_Curve25519_Field51_fmul(c10, t011, b10, tmp);
  uint64_t *b11 = t1 + 5U;
  uint64_t *c1 = t1 + 10U;
  uint64_t *t01 = t1 + 15U;
  FStar_UInt128_uint128 *tmp1 = tmp;
  Hacl_Curve25519_51_fsquare_times(t01, c1, tmp1, 100U);
  Hacl_Impl_Curve25519_Field51_fmul(t01, t01, c1, tmp);
  Hacl_Curve25519_51_fsquare_times(t01, t01, tmp1, 50U);
  Hacl_Impl_Curve25519_Field51_fmul(t01, t01, b11, tmp);
  Hacl_Curve25519_51_fsquare_times(t01, t01, tmp1, 5U);
  uint64_t *a = t1;
  uint64_t *t0 = t1 + 15U;
  Hacl_Impl_Curve25519_Field51_fmul(o, t0, a, tmp);
}

static void encode_point(uint8_t *o, uint64_t *i)
{
  uint64_t *x = i;
  uint64_t *z = i + 5U;
  uint64_t tmp[5U] = { 0U };
  uint64_t u64s[4U] = { 0U };
  FStar_UInt128_uint128 tmp_w[10U];
  for (uint32_t _i = 0U; _i < 10U; ++_i)
    tmp_w[_i] = FStar_UInt128_uint64_to_uint128(0ULL);
  Hacl_Curve25519_51_finv(tmp, z, tmp_w);
  Hacl_Impl_Curve25519_Field51_fmul(tmp, tmp, x, tmp_w);
  Hacl_Impl_Curve25519_Field51_store_felem(u64s, tmp);
  KRML_MAYBE_FOR4(i0, 0U, 4U, 1U, store64_le(o + i0 * 8U, u64s[i0]););
}

/**
Compute the scalar multiple of a point.

@param out Pointer to 32 bytes of memory, allocated by the caller, where the resulting point is written to.
@param priv Pointer to 32 bytes of memory where the secret/private key is read from.
@param pub Pointer to 32 bytes of memory where the public point is read from.
*/
void Hacl_Curve25519_51_scalarmult(uint8_t *out, uint8_t *priv, uint8_t *pub)
{
  uint64_t init[10U] = { 0U };
  uint64_t tmp[4U] = { 0U };
  KRML_MAYBE_FOR4(i,
    0U,
    4U,
    1U,
    uint64_t *os = tmp;
    uint8_t *bj = pub + i * 8U;
    uint64_t u = load64_le(bj);
    uint64_t r = u;
    uint64_t x = r;
    os[i] = x;);
  uint64_t tmp3 = tmp[3U];
  tmp[3U] = tmp3 & 0x7fffffffffffffffULL;
  uint64_t *x = init;
  uint64_t *z = init + 5U;
  z[0U] = 1ULL;
  z[1U] = 0ULL;
  z[2U] = 0ULL;
  z[3U] = 0ULL;
  z[4U] = 0ULL;
  uint64_t f0l = tmp[0U] & 0x7ffffffffffffULL;
  uint64_t f0h = tmp[0U] >> 51U;
  uint64_t f1l = (tmp[1U] & 0x3fffffffffULL) << 13U;
  uint64_t f1h = tmp[1U] >> 38U;
  uint64_t f2l = (tmp[2U] & 0x1ffffffULL) << 26U;
  uint64_t f2h = tmp[2U] >> 25U;
  uint64_t f3l = (tmp[3U] & 0xfffULL) << 39U;
  uint64_t f3h = tmp[3U] >> 12U;
  x[0U] = f0l;
  x[1U] = f0h | f1l;
  x[2U] = f1h | f2l;
  x[3U] = f2h | f3l;
  x[4U] = f3h;
  montgomery_ladder(init, priv, init);
  encode_point(out, init);
}

/**
Calculate a public point from a secret/private key.

This computes a scalar multiplication of the secret/private key with the curve's basepoint.

@param pub Pointer to 32 bytes of memory, allocated by the caller, where the resulting point is written to.
@param priv Pointer to 32 bytes of memory where the secret/private key is read from.
*/
void Hacl_Curve25519_51_secret_to_public(uint8_t *pub, uint8_t *priv)
{
  uint8_t basepoint[32U] = { 0U };
  for (uint32_t i = 0U; i < 32U; i++)
  {
    uint8_t *os = basepoint;
    uint8_t x = g25519[i];
    os[i] = x;
  }
  Hacl_Curve25519_51_scalarmult(pub, priv, basepoint);
}

/**
Execute the diffie-hellmann key exchange.

@param out Pointer to 32 bytes of memory, allocated by the caller, where the resulting point is written to.
@param priv Pointer to 32 bytes of memory where **our** secret/private key is read from.
@param pub Pointer to 32 bytes of memory where **their** public point is read from.
*/
bool Hacl_Curve25519_51_ecdh(uint8_t *out, uint8_t *priv, uint8_t *pub)
{
  uint8_t zeros[32U] = { 0U };
  Hacl_Curve25519_51_scalarmult(out, priv, pub);
  uint8_t res = 255U;
  for (uint32_t i = 0U; i < 32U; i++)
  {
    uint8_t uu____0 = FStar_UInt8_eq_mask(out[i], zeros[i]);
    res = (uint32_t)uu____0 & (uint32_t)res;
  }
  uint8_t z = res;
  bool r = z == 255U;
  return !r;
}

/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */




void Hacl_Hash_SHA2_sha256_init(uint32_t *hash)
{
  KRML_MAYBE_FOR8(i,
    0U,
    8U,
    1U,
    uint32_t *os = hash;
    uint32_t x = Hacl_Hash_SHA2_h256[i];
    os[i] = x;);
}

static inline void sha256_update(uint8_t *b, uint32_t *hash)
{
  uint32_t hash_old[8U] = { 0U };
  uint32_t ws[16U] = { 0U };
  memcpy(hash_old, hash, 8U * sizeof (uint32_t));
  uint8_t *b10 = b;
  uint32_t u = load32_be(b10);
  ws[0U] = u;
  uint32_t u0 = load32_be(b10 + 4U);
  ws[1U] = u0;
  uint32_t u1 = load32_be(b10 + 8U);
  ws[2U] = u1;
  uint32_t u2 = load32_be(b10 + 12U);
  ws[3U] = u2;
  uint32_t u3 = load32_be(b10 + 16U);
  ws[4U] = u3;
  uint32_t u4 = load32_be(b10 + 20U);
  ws[5U] = u4;
  uint32_t u5 = load32_be(b10 + 24U);
  ws[6U] = u5;
  uint32_t u6 = load32_be(b10 + 28U);
  ws[7U] = u6;
  uint32_t u7 = load32_be(b10 + 32U);
  ws[8U] = u7;
  uint32_t u8 = load32_be(b10 + 36U);
  ws[9U] = u8;
  uint32_t u9 = load32_be(b10 + 40U);
  ws[10U] = u9;
  uint32_t u10 = load32_be(b10 + 44U);
  ws[11U] = u10;
  uint32_t u11 = load32_be(b10 + 48U);
  ws[12U] = u11;
  uint32_t u12 = load32_be(b10 + 52U);
  ws[13U] = u12;
  uint32_t u13 = load32_be(b10 + 56U);
  ws[14U] = u13;
  uint32_t u14 = load32_be(b10 + 60U);
  ws[15U] = u14;
  KRML_MAYBE_FOR4(i0,
    0U,
    4U,
    1U,
    KRML_MAYBE_FOR16(i,
      0U,
      16U,
      1U,
      uint32_t k_t = Hacl_Hash_SHA2_k224_256[16U * i0 + i];
      uint32_t ws_t = ws[i];
      uint32_t a0 = hash[0U];
      uint32_t b0 = hash[1U];
      uint32_t c0 = hash[2U];
      uint32_t d0 = hash[3U];
      uint32_t e0 = hash[4U];
      uint32_t f0 = hash[5U];
      uint32_t g0 = hash[6U];
      uint32_t h02 = hash[7U];
      uint32_t k_e_t = k_t;
      uint32_t
      t1 =
        h02 + ((e0 << 26U | e0 >> 6U) ^ ((e0 << 21U | e0 >> 11U) ^ (e0 << 7U | e0 >> 25U))) +
          ((e0 & f0) ^ (~e0 & g0))
        + k_e_t
        + ws_t;
      uint32_t
      t2 =
        ((a0 << 30U | a0 >> 2U) ^ ((a0 << 19U | a0 >> 13U) ^ (a0 << 10U | a0 >> 22U))) +
          ((a0 & b0) ^ ((a0 & c0) ^ (b0 & c0)));
      uint32_t a1 = t1 + t2;
      uint32_t b1 = a0;
      uint32_t c1 = b0;
      uint32_t d1 = c0;
      uint32_t e1 = d0 + t1;
      uint32_t f1 = e0;
      uint32_t g1 = f0;
      uint32_t h12 = g0;
      hash[0U] = a1;
      hash[1U] = b1;
      hash[2U] = c1;
      hash[3U] = d1;
      hash[4U] = e1;
      hash[5U] = f1;
      hash[6U] = g1;
      hash[7U] = h12;);
    if (i0 < 3U)
    {
      KRML_MAYBE_FOR16(i,
        0U,
        16U,
        1U,
        uint32_t t16 = ws[i];
        uint32_t t15 = ws[(i + 1U) % 16U];
        uint32_t t7 = ws[(i + 9U) % 16U];
        uint32_t t2 = ws[(i + 14U) % 16U];
        uint32_t s1 = (t2 << 15U | t2 >> 17U) ^ ((t2 << 13U | t2 >> 19U) ^ t2 >> 10U);
        uint32_t s0 = (t15 << 25U | t15 >> 7U) ^ ((t15 << 14U | t15 >> 18U) ^ t15 >> 3U);
        ws[i] = s1 + t7 + s0 + t16;);
    });
  KRML_MAYBE_FOR8(i,
    0U,
    8U,
    1U,
    uint32_t *os = hash;
    uint32_t x = hash[i] + hash_old[i];
    os[i] = x;);
}

void Hacl_Hash_SHA2_sha256_update_nblocks(uint32_t len, uint8_t *b, uint32_t *st)
{
  uint32_t blocks = len / 64U;
  for (uint32_t i = 0U; i < blocks; i++)
  {
    uint8_t *b0 = b;
    uint8_t *mb = b0 + i * 64U;
    sha256_update(mb, st);
  }
}

void
Hacl_Hash_SHA2_sha256_update_last(uint64_t totlen, uint32_t len, uint8_t *b, uint32_t *hash)
{
  uint32_t blocks;
  if (len + 8U + 1U <= 64U)
  {
    blocks = 1U;
  }
  else
  {
    blocks = 2U;
  }
  uint32_t fin = blocks * 64U;
  uint8_t last[128U] = { 0U };
  uint8_t totlen_buf[8U] = { 0U };
  uint64_t total_len_bits = totlen << 3U;
  store64_be(totlen_buf, total_len_bits);
  uint8_t *b0 = b;
  memcpy(last, b0, len * sizeof (uint8_t));
  last[len] = 0x80U;
  memcpy(last + fin - 8U, totlen_buf, 8U * sizeof (uint8_t));
  uint8_t *last00 = last;
  uint8_t *last10 = last + 64U;
  uint8_t *l0 = last00;
  uint8_t *l1 = last10;
  uint8_t *lb0 = l0;
  uint8_t *lb1 = l1;
  uint8_t *last0 = lb0;
  uint8_t *last1 = lb1;
  sha256_update(last0, hash);
  if (blocks > 1U)
  {
    sha256_update(last1, hash);
    return;
  }
}

void Hacl_Hash_SHA2_sha256_finish(uint32_t *st, uint8_t *h)
{
  uint8_t hbuf[32U] = { 0U };
  KRML_MAYBE_FOR8(i, 0U, 8U, 1U, store32_be(hbuf + i * 4U, st[i]););
  memcpy(h, hbuf, 32U * sizeof (uint8_t));
}

void Hacl_Hash_SHA2_sha224_init(uint32_t *hash)
{
  KRML_MAYBE_FOR8(i,
    0U,
    8U,
    1U,
    uint32_t *os = hash;
    uint32_t x = Hacl_Hash_SHA2_h224[i];
    os[i] = x;);
}

void Hacl_Hash_SHA2_sha224_update_nblocks(uint32_t len, uint8_t *b, uint32_t *st)
{
  Hacl_Hash_SHA2_sha256_update_nblocks(len, b, st);
}

void Hacl_Hash_SHA2_sha224_update_last(uint64_t totlen, uint32_t len, uint8_t *b, uint32_t *st)
{
  Hacl_Hash_SHA2_sha256_update_last(totlen, len, b, st);
}

void Hacl_Hash_SHA2_sha224_finish(uint32_t *st, uint8_t *h)
{
  uint8_t hbuf[32U] = { 0U };
  KRML_MAYBE_FOR8(i, 0U, 8U, 1U, store32_be(hbuf + i * 4U, st[i]););
  memcpy(h, hbuf, 28U * sizeof (uint8_t));
}

void Hacl_Hash_SHA2_sha512_init(uint64_t *hash)
{
  KRML_MAYBE_FOR8(i,
    0U,
    8U,
    1U,
    uint64_t *os = hash;
    uint64_t x = Hacl_Hash_SHA2_h512[i];
    os[i] = x;);
}

static inline void sha512_update(uint8_t *b, uint64_t *hash)
{
  uint64_t hash_old[8U] = { 0U };
  uint64_t ws[16U] = { 0U };
  memcpy(hash_old, hash, 8U * sizeof (uint64_t));
  uint8_t *b10 = b;
  uint64_t u = load64_be(b10);
  ws[0U] = u;
  uint64_t u0 = load64_be(b10 + 8U);
  ws[1U] = u0;
  uint64_t u1 = load64_be(b10 + 16U);
  ws[2U] = u1;
  uint64_t u2 = load64_be(b10 + 24U);
  ws[3U] = u2;
  uint64_t u3 = load64_be(b10 + 32U);
  ws[4U] = u3;
  uint64_t u4 = load64_be(b10 + 40U);
  ws[5U] = u4;
  uint64_t u5 = load64_be(b10 + 48U);
  ws[6U] = u5;
  uint64_t u6 = load64_be(b10 + 56U);
  ws[7U] = u6;
  uint64_t u7 = load64_be(b10 + 64U);
  ws[8U] = u7;
  uint64_t u8 = load64_be(b10 + 72U);
  ws[9U] = u8;
  uint64_t u9 = load64_be(b10 + 80U);
  ws[10U] = u9;
  uint64_t u10 = load64_be(b10 + 88U);
  ws[11U] = u10;
  uint64_t u11 = load64_be(b10 + 96U);
  ws[12U] = u11;
  uint64_t u12 = load64_be(b10 + 104U);
  ws[13U] = u12;
  uint64_t u13 = load64_be(b10 + 112U);
  ws[14U] = u13;
  uint64_t u14 = load64_be(b10 + 120U);
  ws[15U] = u14;
  KRML_MAYBE_FOR5(i0,
    0U,
    5U,
    1U,
    KRML_MAYBE_FOR16(i,
      0U,
      16U,
      1U,
      uint64_t k_t = Hacl_Hash_SHA2_k384_512[16U * i0 + i];
      uint64_t ws_t = ws[i];
      uint64_t a0 = hash[0U];
      uint64_t b0 = hash[1U];
      uint64_t c0 = hash[2U];
      uint64_t d0 = hash[3U];
      uint64_t e0 = hash[4U];
      uint64_t f0 = hash[5U];
      uint64_t g0 = hash[6U];
      uint64_t h02 = hash[7U];
      uint64_t k_e_t = k_t;
      uint64_t
      t1 =
        h02 + ((e0 << 50U | e0 >> 14U) ^ ((e0 << 46U | e0 >> 18U) ^ (e0 << 23U | e0 >> 41U))) +
          ((e0 & f0) ^ (~e0 & g0))
        + k_e_t
        + ws_t;
      uint64_t
      t2 =
        ((a0 << 36U | a0 >> 28U) ^ ((a0 << 30U | a0 >> 34U) ^ (a0 << 25U | a0 >> 39U))) +
          ((a0 & b0) ^ ((a0 & c0) ^ (b0 & c0)));
      uint64_t a1 = t1 + t2;
      uint64_t b1 = a0;
      uint64_t c1 = b0;
      uint64_t d1 = c0;
      uint64_t e1 = d0 + t1;
      uint64_t f1 = e0;
      uint64_t g1 = f0;
      uint64_t h12 = g0;
      hash[0U] = a1;
      hash[1U] = b1;
      hash[2U] = c1;
      hash[3U] = d1;
      hash[4U] = e1;
      hash[5U] = f1;
      hash[6U] = g1;
      hash[7U] = h12;);
    if (i0 < 4U)
    {
      KRML_MAYBE_FOR16(i,
        0U,
        16U,
        1U,
        uint64_t t16 = ws[i];
        uint64_t t15 = ws[(i + 1U) % 16U];
        uint64_t t7 = ws[(i + 9U) % 16U];
        uint64_t t2 = ws[(i + 14U) % 16U];
        uint64_t s1 = (t2 << 45U | t2 >> 19U) ^ ((t2 << 3U | t2 >> 61U) ^ t2 >> 6U);
        uint64_t s0 = (t15 << 63U | t15 >> 1U) ^ ((t15 << 56U | t15 >> 8U) ^ t15 >> 7U);
        ws[i] = s1 + t7 + s0 + t16;);
    });
  KRML_MAYBE_FOR8(i,
    0U,
    8U,
    1U,
    uint64_t *os = hash;
    uint64_t x = hash[i] + hash_old[i];
    os[i] = x;);
}

void Hacl_Hash_SHA2_sha512_update_nblocks(uint32_t len, uint8_t *b, uint64_t *st)
{
  uint32_t blocks = len / 128U;
  for (uint32_t i = 0U; i < blocks; i++)
  {
    uint8_t *b0 = b;
    uint8_t *mb = b0 + i * 128U;
    sha512_update(mb, st);
  }
}

void
Hacl_Hash_SHA2_sha512_update_last(
  FStar_UInt128_uint128 totlen,
  uint32_t len,
  uint8_t *b,
  uint64_t *hash
)
{
  uint32_t blocks;
  if (len + 16U + 1U <= 128U)
  {
    blocks = 1U;
  }
  else
  {
    blocks = 2U;
  }
  uint32_t fin = blocks * 128U;
  uint8_t last[256U] = { 0U };
  uint8_t totlen_buf[16U] = { 0U };
  FStar_UInt128_uint128 total_len_bits = FStar_UInt128_shift_left(totlen, 3U);
  store128_be(totlen_buf, total_len_bits);
  uint8_t *b0 = b;
  memcpy(last, b0, len * sizeof (uint8_t));
  last[len] = 0x80U;
  memcpy(last + fin - 16U, totlen_buf, 16U * sizeof (uint8_t));
  uint8_t *last00 = last;
  uint8_t *last10 = last + 128U;
  uint8_t *l0 = last00;
  uint8_t *l1 = last10;
  uint8_t *lb0 = l0;
  uint8_t *lb1 = l1;
  uint8_t *last0 = lb0;
  uint8_t *last1 = lb1;
  sha512_update(last0, hash);
  if (blocks > 1U)
  {
    sha512_update(last1, hash);
    return;
  }
}

void Hacl_Hash_SHA2_sha512_finish(uint64_t *st, uint8_t *h)
{
  uint8_t hbuf[64U] = { 0U };
  KRML_MAYBE_FOR8(i, 0U, 8U, 1U, store64_be(hbuf + i * 8U, st[i]););
  memcpy(h, hbuf, 64U * sizeof (uint8_t));
}

void Hacl_Hash_SHA2_sha384_init(uint64_t *hash)
{
  KRML_MAYBE_FOR8(i,
    0U,
    8U,
    1U,
    uint64_t *os = hash;
    uint64_t x = Hacl_Hash_SHA2_h384[i];
    os[i] = x;);
}

void Hacl_Hash_SHA2_sha384_update_nblocks(uint32_t len, uint8_t *b, uint64_t *st)
{
  Hacl_Hash_SHA2_sha512_update_nblocks(len, b, st);
}

void
Hacl_Hash_SHA2_sha384_update_last(
  FStar_UInt128_uint128 totlen,
  uint32_t len,
  uint8_t *b,
  uint64_t *st
)
{
  Hacl_Hash_SHA2_sha512_update_last(totlen, len, b, st);
}

void Hacl_Hash_SHA2_sha384_finish(uint64_t *st, uint8_t *h)
{
  uint8_t hbuf[64U] = { 0U };
  KRML_MAYBE_FOR8(i, 0U, 8U, 1U, store64_be(hbuf + i * 8U, st[i]););
  memcpy(h, hbuf, 48U * sizeof (uint8_t));
}

/**
Allocate initial state for the SHA2_256 hash. The state is to be freed by
calling `free_256`.
*/
Hacl_Streaming_MD_state_32 *Hacl_Hash_SHA2_malloc_256(void)
{
  uint8_t *buf = (uint8_t *)KRML_HOST_CALLOC(64U, sizeof (uint8_t));
  if (buf == NULL)
  {
    return NULL;
  }
  uint8_t *buf1 = buf;
  uint32_t *b = (uint32_t *)KRML_HOST_CALLOC(8U, sizeof (uint32_t));
  Hacl_Streaming_Types_optional_32 block_state;
  if (b == NULL)
  {
    block_state = ((Hacl_Streaming_Types_optional_32){ .tag = Hacl_Streaming_Types_None });
  }
  else
  {
    block_state = ((Hacl_Streaming_Types_optional_32){ .tag = Hacl_Streaming_Types_Some, .v = b });
  }
  if (block_state.tag == Hacl_Streaming_Types_None)
  {
    KRML_HOST_FREE(buf1);
    return NULL;
  }
  if (block_state.tag == Hacl_Streaming_Types_Some)
  {
    uint32_t *block_state1 = block_state.v;
    Hacl_Streaming_Types_optional k_ = Hacl_Streaming_Types_Some;
    switch (k_)
    {
      case Hacl_Streaming_Types_None:
        {
          return NULL;
        }
      case Hacl_Streaming_Types_Some:
        {
          Hacl_Streaming_MD_state_32
          s = { .block_state = block_state1, .buf = buf1, .total_len = (uint64_t)0U };
          Hacl_Streaming_MD_state_32
          *p = (Hacl_Streaming_MD_state_32 *)KRML_HOST_MALLOC(sizeof (Hacl_Streaming_MD_state_32));
          if (p != NULL)
          {
            p[0U] = s;
          }
          if (p == NULL)
          {
            KRML_HOST_FREE(block_state1);
            KRML_HOST_FREE(buf1);
            return NULL;
          }
          Hacl_Hash_SHA2_sha256_init(block_state1);
          return p;
        }
      default:
        {
          KRML_HOST_EPRINTF("KaRaMeL incomplete match at %s:%d\n", __FILE__, __LINE__);
          KRML_HOST_EXIT(253U);
        }
    }
  }
  KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
    __FILE__,
    __LINE__,
    "unreachable (pattern matches are exhaustive in F*)");
  KRML_HOST_EXIT(255U);
}

/**
Copies the state passed as argument into a newly allocated state (deep copy).
The state is to be freed by calling `free_256`. Cloning the state this way is
useful, for instance, if your control-flow diverges and you need to feed
more (different) data into the hash in each branch.
*/
Hacl_Streaming_MD_state_32 *Hacl_Hash_SHA2_copy_256(Hacl_Streaming_MD_state_32 *state)
{
  Hacl_Streaming_MD_state_32 scrut = *state;
  uint32_t *block_state0 = scrut.block_state;
  uint8_t *buf0 = scrut.buf;
  uint64_t total_len0 = scrut.total_len;
  uint8_t *buf = (uint8_t *)KRML_HOST_CALLOC(64U, sizeof (uint8_t));
  if (buf == NULL)
  {
    return NULL;
  }
  memcpy(buf, buf0, 64U * sizeof (uint8_t));
  uint32_t *b = (uint32_t *)KRML_HOST_CALLOC(8U, sizeof (uint32_t));
  Hacl_Streaming_Types_optional_32 block_state;
  if (b == NULL)
  {
    block_state = ((Hacl_Streaming_Types_optional_32){ .tag = Hacl_Streaming_Types_None });
  }
  else
  {
    block_state = ((Hacl_Streaming_Types_optional_32){ .tag = Hacl_Streaming_Types_Some, .v = b });
  }
  if (block_state.tag == Hacl_Streaming_Types_None)
  {
    KRML_HOST_FREE(buf);
    return NULL;
  }
  if (block_state.tag == Hacl_Streaming_Types_Some)
  {
    uint32_t *block_state1 = block_state.v;
    memcpy(block_state1, block_state0, 8U * sizeof (uint32_t));
    Hacl_Streaming_Types_optional k_ = Hacl_Streaming_Types_Some;
    switch (k_)
    {
      case Hacl_Streaming_Types_None:
        {
          return NULL;
        }
      case Hacl_Streaming_Types_Some:
        {
          Hacl_Streaming_MD_state_32
          s = { .block_state = block_state1, .buf = buf, .total_len = total_len0 };
          Hacl_Streaming_MD_state_32
          *p = (Hacl_Streaming_MD_state_32 *)KRML_HOST_MALLOC(sizeof (Hacl_Streaming_MD_state_32));
          if (p != NULL)
          {
            p[0U] = s;
          }
          if (p == NULL)
          {
            KRML_HOST_FREE(block_state1);
            KRML_HOST_FREE(buf);
            return NULL;
          }
          return p;
        }
      default:
        {
          KRML_HOST_EPRINTF("KaRaMeL incomplete match at %s:%d\n", __FILE__, __LINE__);
          KRML_HOST_EXIT(253U);
        }
    }
  }
  KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
    __FILE__,
    __LINE__,
    "unreachable (pattern matches are exhaustive in F*)");
  KRML_HOST_EXIT(255U);
}

/**
Reset an existing state to the initial hash state with empty data.
*/
void Hacl_Hash_SHA2_reset_256(Hacl_Streaming_MD_state_32 *state)
{
  Hacl_Streaming_MD_state_32 scrut = *state;
  uint8_t *buf = scrut.buf;
  uint32_t *block_state = scrut.block_state;
  Hacl_Hash_SHA2_sha256_init(block_state);
  Hacl_Streaming_MD_state_32
  tmp = { .block_state = block_state, .buf = buf, .total_len = (uint64_t)0U };
  state[0U] = tmp;
}

static inline Hacl_Streaming_Types_error_code
update_224_256(Hacl_Streaming_MD_state_32 *state, uint8_t *chunk, uint32_t chunk_len)
{
  Hacl_Streaming_MD_state_32 s = *state;
  uint64_t total_len = s.total_len;
  if ((uint64_t)chunk_len > 2305843009213693951ULL - total_len)
  {
    return Hacl_Streaming_Types_MaximumLengthExceeded;
  }
  uint32_t sz;
  if (total_len % (uint64_t)64U == 0ULL && total_len > 0ULL)
  {
    sz = 64U;
  }
  else
  {
    sz = (uint32_t)(total_len % (uint64_t)64U);
  }
  if (chunk_len <= 64U - sz)
  {
    Hacl_Streaming_MD_state_32 s1 = *state;
    uint32_t *block_state1 = s1.block_state;
    uint8_t *buf = s1.buf;
    uint64_t total_len1 = s1.total_len;
    uint32_t sz1;
    if (total_len1 % (uint64_t)64U == 0ULL && total_len1 > 0ULL)
    {
      sz1 = 64U;
    }
    else
    {
      sz1 = (uint32_t)(total_len1 % (uint64_t)64U);
    }
    uint8_t *buf2 = buf + sz1;
    memcpy(buf2, chunk, chunk_len * sizeof (uint8_t));
    uint64_t total_len2 = total_len1 + (uint64_t)chunk_len;
    *state =
      (
        (Hacl_Streaming_MD_state_32){
          .block_state = block_state1,
          .buf = buf,
          .total_len = total_len2
        }
      );
  }
  else if (sz == 0U)
  {
    Hacl_Streaming_MD_state_32 s1 = *state;
    uint32_t *block_state1 = s1.block_state;
    uint8_t *buf = s1.buf;
    uint64_t total_len1 = s1.total_len;
    uint32_t sz1;
    if (total_len1 % (uint64_t)64U == 0ULL && total_len1 > 0ULL)
    {
      sz1 = 64U;
    }
    else
    {
      sz1 = (uint32_t)(total_len1 % (uint64_t)64U);
    }
    if (!(sz1 == 0U))
    {
      Hacl_Hash_SHA2_sha256_update_nblocks(64U, buf, block_state1);
    }
    uint32_t ite;
    if ((uint64_t)chunk_len % (uint64_t)64U == 0ULL && (uint64_t)chunk_len > 0ULL)
    {
      ite = 64U;
    }
    else
    {
      ite = (uint32_t)((uint64_t)chunk_len % (uint64_t)64U);
    }
    uint32_t n_blocks = (chunk_len - ite) / 64U;
    uint32_t data1_len = n_blocks * 64U;
    uint32_t data2_len = chunk_len - data1_len;
    uint8_t *data1 = chunk;
    uint8_t *data2 = chunk + data1_len;
    Hacl_Hash_SHA2_sha256_update_nblocks(data1_len / 64U * 64U, data1, block_state1);
    uint8_t *dst = buf;
    memcpy(dst, data2, data2_len * sizeof (uint8_t));
    *state =
      (
        (Hacl_Streaming_MD_state_32){
          .block_state = block_state1,
          .buf = buf,
          .total_len = total_len1 + (uint64_t)chunk_len
        }
      );
  }
  else
  {
    uint32_t diff = 64U - sz;
    uint8_t *chunk1 = chunk;
    uint8_t *chunk2 = chunk + diff;
    Hacl_Streaming_MD_state_32 s1 = *state;
    uint32_t *block_state10 = s1.block_state;
    uint8_t *buf0 = s1.buf;
    uint64_t total_len10 = s1.total_len;
    uint32_t sz10;
    if (total_len10 % (uint64_t)64U == 0ULL && total_len10 > 0ULL)
    {
      sz10 = 64U;
    }
    else
    {
      sz10 = (uint32_t)(total_len10 % (uint64_t)64U);
    }
    uint8_t *buf2 = buf0 + sz10;
    memcpy(buf2, chunk1, diff * sizeof (uint8_t));
    uint64_t total_len2 = total_len10 + (uint64_t)diff;
    *state =
      (
        (Hacl_Streaming_MD_state_32){
          .block_state = block_state10,
          .buf = buf0,
          .total_len = total_len2
        }
      );
    Hacl_Streaming_MD_state_32 s10 = *state;
    uint32_t *block_state1 = s10.block_state;
    uint8_t *buf = s10.buf;
    uint64_t total_len1 = s10.total_len;
    uint32_t sz1;
    if (total_len1 % (uint64_t)64U == 0ULL && total_len1 > 0ULL)
    {
      sz1 = 64U;
    }
    else
    {
      sz1 = (uint32_t)(total_len1 % (uint64_t)64U);
    }
    if (!(sz1 == 0U))
    {
      Hacl_Hash_SHA2_sha256_update_nblocks(64U, buf, block_state1);
    }
    uint32_t ite;
    if
    ((uint64_t)(chunk_len - diff) % (uint64_t)64U == 0ULL && (uint64_t)(chunk_len - diff) > 0ULL)
    {
      ite = 64U;
    }
    else
    {
      ite = (uint32_t)((uint64_t)(chunk_len - diff) % (uint64_t)64U);
    }
    uint32_t n_blocks = (chunk_len - diff - ite) / 64U;
    uint32_t data1_len = n_blocks * 64U;
    uint32_t data2_len = chunk_len - diff - data1_len;
    uint8_t *data1 = chunk2;
    uint8_t *data2 = chunk2 + data1_len;
    Hacl_Hash_SHA2_sha256_update_nblocks(data1_len / 64U * 64U, data1, block_state1);
    uint8_t *dst = buf;
    memcpy(dst, data2, data2_len * sizeof (uint8_t));
    *state =
      (
        (Hacl_Streaming_MD_state_32){
          .block_state = block_state1,
          .buf = buf,
          .total_len = total_len1 + (uint64_t)(chunk_len - diff)
        }
      );
  }
  return Hacl_Streaming_Types_Success;
}

/**
Feed an arbitrary amount of data into the hash. This function returns 0 for
success, or 1 if the combined length of all of the data passed to `update_256`
(since the last call to `reset_256`) exceeds 2^61-1 bytes.

This function is identical to the update function for SHA2_224.
*/
Hacl_Streaming_Types_error_code
Hacl_Hash_SHA2_update_256(
  Hacl_Streaming_MD_state_32 *state,
  uint8_t *input,
  uint32_t input_len
)
{
  return update_224_256(state, input, input_len);
}

/**
Write the resulting hash into `output`, an array of 32 bytes. The state remains
valid after a call to `digest_256`, meaning the user may feed more data into
the hash via `update_256`. (The digest_256 function operates on an internal copy of
the state and therefore does not invalidate the client-held state `p`.)
*/
void Hacl_Hash_SHA2_digest_256(Hacl_Streaming_MD_state_32 *state, uint8_t *output)
{
  Hacl_Streaming_MD_state_32 scrut = *state;
  uint32_t *block_state = scrut.block_state;
  uint8_t *buf_ = scrut.buf;
  uint64_t total_len = scrut.total_len;
  uint32_t r;
  if (total_len % (uint64_t)64U == 0ULL && total_len > 0ULL)
  {
    r = 64U;
  }
  else
  {
    r = (uint32_t)(total_len % (uint64_t)64U);
  }
  uint8_t *buf_1 = buf_;
  uint32_t tmp_block_state[8U] = { 0U };
  memcpy(tmp_block_state, block_state, 8U * sizeof (uint32_t));
  uint32_t ite;
  if (r % 64U == 0U && r > 0U)
  {
    ite = 64U;
  }
  else
  {
    ite = r % 64U;
  }
  uint8_t *buf_last = buf_1 + r - ite;
  uint8_t *buf_multi = buf_1;
  Hacl_Hash_SHA2_sha256_update_nblocks(0U, buf_multi, tmp_block_state);
  uint64_t prev_len_last = total_len - (uint64_t)r;
  Hacl_Hash_SHA2_sha256_update_last(prev_len_last + (uint64_t)r, r, buf_last, tmp_block_state);
  Hacl_Hash_SHA2_sha256_finish(tmp_block_state, output);
}

/**
Free a state allocated with `malloc_256`.

This function is identical to the free function for SHA2_224.
*/
void Hacl_Hash_SHA2_free_256(Hacl_Streaming_MD_state_32 *state)
{
  Hacl_Streaming_MD_state_32 scrut = *state;
  uint8_t *buf = scrut.buf;
  uint32_t *block_state = scrut.block_state;
  KRML_HOST_FREE(block_state);
  KRML_HOST_FREE(buf);
  KRML_HOST_FREE(state);
}

/**
Hash `input`, of len `input_len`, into `output`, an array of 32 bytes.
*/
void Hacl_Hash_SHA2_hash_256(uint8_t *output, uint8_t *input, uint32_t input_len)
{
  uint8_t *ib = input;
  uint8_t *rb = output;
  uint32_t st[8U] = { 0U };
  Hacl_Hash_SHA2_sha256_init(st);
  uint32_t rem = input_len % 64U;
  uint64_t len_ = (uint64_t)input_len;
  Hacl_Hash_SHA2_sha256_update_nblocks(input_len, ib, st);
  uint32_t rem1 = input_len % 64U;
  uint8_t *b0 = ib;
  uint8_t *lb = b0 + input_len - rem1;
  Hacl_Hash_SHA2_sha256_update_last(len_, rem, lb, st);
  Hacl_Hash_SHA2_sha256_finish(st, rb);
}

Hacl_Streaming_MD_state_32 *Hacl_Hash_SHA2_malloc_224(void)
{
  uint8_t *buf = (uint8_t *)KRML_HOST_CALLOC(64U, sizeof (uint8_t));
  if (buf == NULL)
  {
    return NULL;
  }
  uint8_t *buf1 = buf;
  uint32_t *b = (uint32_t *)KRML_HOST_CALLOC(8U, sizeof (uint32_t));
  Hacl_Streaming_Types_optional_32 block_state;
  if (b == NULL)
  {
    block_state = ((Hacl_Streaming_Types_optional_32){ .tag = Hacl_Streaming_Types_None });
  }
  else
  {
    block_state = ((Hacl_Streaming_Types_optional_32){ .tag = Hacl_Streaming_Types_Some, .v = b });
  }
  if (block_state.tag == Hacl_Streaming_Types_None)
  {
    KRML_HOST_FREE(buf1);
    return NULL;
  }
  if (block_state.tag == Hacl_Streaming_Types_Some)
  {
    uint32_t *block_state1 = block_state.v;
    Hacl_Streaming_Types_optional k_ = Hacl_Streaming_Types_Some;
    switch (k_)
    {
      case Hacl_Streaming_Types_None:
        {
          return NULL;
        }
      case Hacl_Streaming_Types_Some:
        {
          Hacl_Streaming_MD_state_32
          s = { .block_state = block_state1, .buf = buf1, .total_len = (uint64_t)0U };
          Hacl_Streaming_MD_state_32
          *p = (Hacl_Streaming_MD_state_32 *)KRML_HOST_MALLOC(sizeof (Hacl_Streaming_MD_state_32));
          if (p != NULL)
          {
            p[0U] = s;
          }
          if (p == NULL)
          {
            KRML_HOST_FREE(block_state1);
            KRML_HOST_FREE(buf1);
            return NULL;
          }
          Hacl_Hash_SHA2_sha224_init(block_state1);
          return p;
        }
      default:
        {
          KRML_HOST_EPRINTF("KaRaMeL incomplete match at %s:%d\n", __FILE__, __LINE__);
          KRML_HOST_EXIT(253U);
        }
    }
  }
  KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
    __FILE__,
    __LINE__,
    "unreachable (pattern matches are exhaustive in F*)");
  KRML_HOST_EXIT(255U);
}

void Hacl_Hash_SHA2_reset_224(Hacl_Streaming_MD_state_32 *state)
{
  Hacl_Streaming_MD_state_32 scrut = *state;
  uint8_t *buf = scrut.buf;
  uint32_t *block_state = scrut.block_state;
  Hacl_Hash_SHA2_sha224_init(block_state);
  Hacl_Streaming_MD_state_32
  tmp = { .block_state = block_state, .buf = buf, .total_len = (uint64_t)0U };
  state[0U] = tmp;
}

Hacl_Streaming_Types_error_code
Hacl_Hash_SHA2_update_224(
  Hacl_Streaming_MD_state_32 *state,
  uint8_t *input,
  uint32_t input_len
)
{
  return update_224_256(state, input, input_len);
}

/**
Write the resulting hash into `output`, an array of 28 bytes. The state remains
valid after a call to `digest_224`, meaning the user may feed more data into
the hash via `update_224`.
*/
void Hacl_Hash_SHA2_digest_224(Hacl_Streaming_MD_state_32 *state, uint8_t *output)
{
  Hacl_Streaming_MD_state_32 scrut = *state;
  uint32_t *block_state = scrut.block_state;
  uint8_t *buf_ = scrut.buf;
  uint64_t total_len = scrut.total_len;
  uint32_t r;
  if (total_len % (uint64_t)64U == 0ULL && total_len > 0ULL)
  {
    r = 64U;
  }
  else
  {
    r = (uint32_t)(total_len % (uint64_t)64U);
  }
  uint8_t *buf_1 = buf_;
  uint32_t tmp_block_state[8U] = { 0U };
  memcpy(tmp_block_state, block_state, 8U * sizeof (uint32_t));
  uint32_t ite;
  if (r % 64U == 0U && r > 0U)
  {
    ite = 64U;
  }
  else
  {
    ite = r % 64U;
  }
  uint8_t *buf_last = buf_1 + r - ite;
  uint8_t *buf_multi = buf_1;
  Hacl_Hash_SHA2_sha224_update_nblocks(0U, buf_multi, tmp_block_state);
  uint64_t prev_len_last = total_len - (uint64_t)r;
  Hacl_Hash_SHA2_sha224_update_last(prev_len_last + (uint64_t)r, r, buf_last, tmp_block_state);
  Hacl_Hash_SHA2_sha224_finish(tmp_block_state, output);
}

void Hacl_Hash_SHA2_free_224(Hacl_Streaming_MD_state_32 *state)
{
  Hacl_Hash_SHA2_free_256(state);
}

/**
Hash `input`, of len `input_len`, into `output`, an array of 28 bytes.
*/
void Hacl_Hash_SHA2_hash_224(uint8_t *output, uint8_t *input, uint32_t input_len)
{
  uint8_t *ib = input;
  uint8_t *rb = output;
  uint32_t st[8U] = { 0U };
  Hacl_Hash_SHA2_sha224_init(st);
  uint32_t rem = input_len % 64U;
  uint64_t len_ = (uint64_t)input_len;
  Hacl_Hash_SHA2_sha224_update_nblocks(input_len, ib, st);
  uint32_t rem1 = input_len % 64U;
  uint8_t *b0 = ib;
  uint8_t *lb = b0 + input_len - rem1;
  Hacl_Hash_SHA2_sha224_update_last(len_, rem, lb, st);
  Hacl_Hash_SHA2_sha224_finish(st, rb);
}

Hacl_Streaming_MD_state_64 *Hacl_Hash_SHA2_malloc_512(void)
{
  uint8_t *buf = (uint8_t *)KRML_HOST_CALLOC(128U, sizeof (uint8_t));
  if (buf == NULL)
  {
    return NULL;
  }
  uint8_t *buf1 = buf;
  uint64_t *b = (uint64_t *)KRML_HOST_CALLOC(8U, sizeof (uint64_t));
  Hacl_Streaming_Types_optional_64 block_state;
  if (b == NULL)
  {
    block_state = ((Hacl_Streaming_Types_optional_64){ .tag = Hacl_Streaming_Types_None });
  }
  else
  {
    block_state = ((Hacl_Streaming_Types_optional_64){ .tag = Hacl_Streaming_Types_Some, .v = b });
  }
  if (block_state.tag == Hacl_Streaming_Types_None)
  {
    KRML_HOST_FREE(buf1);
    return NULL;
  }
  if (block_state.tag == Hacl_Streaming_Types_Some)
  {
    uint64_t *block_state1 = block_state.v;
    Hacl_Streaming_Types_optional k_ = Hacl_Streaming_Types_Some;
    switch (k_)
    {
      case Hacl_Streaming_Types_None:
        {
          return NULL;
        }
      case Hacl_Streaming_Types_Some:
        {
          Hacl_Streaming_MD_state_64
          s = { .block_state = block_state1, .buf = buf1, .total_len = (uint64_t)0U };
          Hacl_Streaming_MD_state_64
          *p = (Hacl_Streaming_MD_state_64 *)KRML_HOST_MALLOC(sizeof (Hacl_Streaming_MD_state_64));
          if (p != NULL)
          {
            p[0U] = s;
          }
          if (p == NULL)
          {
            KRML_HOST_FREE(block_state1);
            KRML_HOST_FREE(buf1);
            return NULL;
          }
          Hacl_Hash_SHA2_sha512_init(block_state1);
          return p;
        }
      default:
        {
          KRML_HOST_EPRINTF("KaRaMeL incomplete match at %s:%d\n", __FILE__, __LINE__);
          KRML_HOST_EXIT(253U);
        }
    }
  }
  KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
    __FILE__,
    __LINE__,
    "unreachable (pattern matches are exhaustive in F*)");
  KRML_HOST_EXIT(255U);
}

/**
Copies the state passed as argument into a newly allocated state (deep copy).
The state is to be freed by calling `free_512`. Cloning the state this way is
useful, for instance, if your control-flow diverges and you need to feed
more (different) data into the hash in each branch.
*/
Hacl_Streaming_MD_state_64 *Hacl_Hash_SHA2_copy_512(Hacl_Streaming_MD_state_64 *state)
{
  Hacl_Streaming_MD_state_64 scrut = *state;
  uint64_t *block_state0 = scrut.block_state;
  uint8_t *buf0 = scrut.buf;
  uint64_t total_len0 = scrut.total_len;
  uint8_t *buf = (uint8_t *)KRML_HOST_CALLOC(128U, sizeof (uint8_t));
  if (buf == NULL)
  {
    return NULL;
  }
  memcpy(buf, buf0, 128U * sizeof (uint8_t));
  uint64_t *b = (uint64_t *)KRML_HOST_CALLOC(8U, sizeof (uint64_t));
  Hacl_Streaming_Types_optional_64 block_state;
  if (b == NULL)
  {
    block_state = ((Hacl_Streaming_Types_optional_64){ .tag = Hacl_Streaming_Types_None });
  }
  else
  {
    block_state = ((Hacl_Streaming_Types_optional_64){ .tag = Hacl_Streaming_Types_Some, .v = b });
  }
  if (block_state.tag == Hacl_Streaming_Types_None)
  {
    KRML_HOST_FREE(buf);
    return NULL;
  }
  if (block_state.tag == Hacl_Streaming_Types_Some)
  {
    uint64_t *block_state1 = block_state.v;
    memcpy(block_state1, block_state0, 8U * sizeof (uint64_t));
    Hacl_Streaming_Types_optional k_ = Hacl_Streaming_Types_Some;
    switch (k_)
    {
      case Hacl_Streaming_Types_None:
        {
          return NULL;
        }
      case Hacl_Streaming_Types_Some:
        {
          Hacl_Streaming_MD_state_64
          s = { .block_state = block_state1, .buf = buf, .total_len = total_len0 };
          Hacl_Streaming_MD_state_64
          *p = (Hacl_Streaming_MD_state_64 *)KRML_HOST_MALLOC(sizeof (Hacl_Streaming_MD_state_64));
          if (p != NULL)
          {
            p[0U] = s;
          }
          if (p == NULL)
          {
            KRML_HOST_FREE(block_state1);
            KRML_HOST_FREE(buf);
            return NULL;
          }
          return p;
        }
      default:
        {
          KRML_HOST_EPRINTF("KaRaMeL incomplete match at %s:%d\n", __FILE__, __LINE__);
          KRML_HOST_EXIT(253U);
        }
    }
  }
  KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
    __FILE__,
    __LINE__,
    "unreachable (pattern matches are exhaustive in F*)");
  KRML_HOST_EXIT(255U);
}

void Hacl_Hash_SHA2_reset_512(Hacl_Streaming_MD_state_64 *state)
{
  Hacl_Streaming_MD_state_64 scrut = *state;
  uint8_t *buf = scrut.buf;
  uint64_t *block_state = scrut.block_state;
  Hacl_Hash_SHA2_sha512_init(block_state);
  Hacl_Streaming_MD_state_64
  tmp = { .block_state = block_state, .buf = buf, .total_len = (uint64_t)0U };
  state[0U] = tmp;
}

static inline Hacl_Streaming_Types_error_code
update_384_512(Hacl_Streaming_MD_state_64 *state, uint8_t *chunk, uint32_t chunk_len)
{
  Hacl_Streaming_MD_state_64 s = *state;
  uint64_t total_len = s.total_len;
  if ((uint64_t)chunk_len > 18446744073709551615ULL - total_len)
  {
    return Hacl_Streaming_Types_MaximumLengthExceeded;
  }
  uint32_t sz;
  if (total_len % (uint64_t)128U == 0ULL && total_len > 0ULL)
  {
    sz = 128U;
  }
  else
  {
    sz = (uint32_t)(total_len % (uint64_t)128U);
  }
  if (chunk_len <= 128U - sz)
  {
    Hacl_Streaming_MD_state_64 s1 = *state;
    uint64_t *block_state1 = s1.block_state;
    uint8_t *buf = s1.buf;
    uint64_t total_len1 = s1.total_len;
    uint32_t sz1;
    if (total_len1 % (uint64_t)128U == 0ULL && total_len1 > 0ULL)
    {
      sz1 = 128U;
    }
    else
    {
      sz1 = (uint32_t)(total_len1 % (uint64_t)128U);
    }
    uint8_t *buf2 = buf + sz1;
    memcpy(buf2, chunk, chunk_len * sizeof (uint8_t));
    uint64_t total_len2 = total_len1 + (uint64_t)chunk_len;
    *state =
      (
        (Hacl_Streaming_MD_state_64){
          .block_state = block_state1,
          .buf = buf,
          .total_len = total_len2
        }
      );
  }
  else if (sz == 0U)
  {
    Hacl_Streaming_MD_state_64 s1 = *state;
    uint64_t *block_state1 = s1.block_state;
    uint8_t *buf = s1.buf;
    uint64_t total_len1 = s1.total_len;
    uint32_t sz1;
    if (total_len1 % (uint64_t)128U == 0ULL && total_len1 > 0ULL)
    {
      sz1 = 128U;
    }
    else
    {
      sz1 = (uint32_t)(total_len1 % (uint64_t)128U);
    }
    if (!(sz1 == 0U))
    {
      Hacl_Hash_SHA2_sha512_update_nblocks(128U, buf, block_state1);
    }
    uint32_t ite;
    if ((uint64_t)chunk_len % (uint64_t)128U == 0ULL && (uint64_t)chunk_len > 0ULL)
    {
      ite = 128U;
    }
    else
    {
      ite = (uint32_t)((uint64_t)chunk_len % (uint64_t)128U);
    }
    uint32_t n_blocks = (chunk_len - ite) / 128U;
    uint32_t data1_len = n_blocks * 128U;
    uint32_t data2_len = chunk_len - data1_len;
    uint8_t *data1 = chunk;
    uint8_t *data2 = chunk + data1_len;
    Hacl_Hash_SHA2_sha512_update_nblocks(data1_len / 128U * 128U, data1, block_state1);
    uint8_t *dst = buf;
    memcpy(dst, data2, data2_len * sizeof (uint8_t));
    *state =
      (
        (Hacl_Streaming_MD_state_64){
          .block_state = block_state1,
          .buf = buf,
          .total_len = total_len1 + (uint64_t)chunk_len
        }
      );
  }
  else
  {
    uint32_t diff = 128U - sz;
    uint8_t *chunk1 = chunk;
    uint8_t *chunk2 = chunk + diff;
    Hacl_Streaming_MD_state_64 s1 = *state;
    uint64_t *block_state10 = s1.block_state;
    uint8_t *buf0 = s1.buf;
    uint64_t total_len10 = s1.total_len;
    uint32_t sz10;
    if (total_len10 % (uint64_t)128U == 0ULL && total_len10 > 0ULL)
    {
      sz10 = 128U;
    }
    else
    {
      sz10 = (uint32_t)(total_len10 % (uint64_t)128U);
    }
    uint8_t *buf2 = buf0 + sz10;
    memcpy(buf2, chunk1, diff * sizeof (uint8_t));
    uint64_t total_len2 = total_len10 + (uint64_t)diff;
    *state =
      (
        (Hacl_Streaming_MD_state_64){
          .block_state = block_state10,
          .buf = buf0,
          .total_len = total_len2
        }
      );
    Hacl_Streaming_MD_state_64 s10 = *state;
    uint64_t *block_state1 = s10.block_state;
    uint8_t *buf = s10.buf;
    uint64_t total_len1 = s10.total_len;
    uint32_t sz1;
    if (total_len1 % (uint64_t)128U == 0ULL && total_len1 > 0ULL)
    {
      sz1 = 128U;
    }
    else
    {
      sz1 = (uint32_t)(total_len1 % (uint64_t)128U);
    }
    if (!(sz1 == 0U))
    {
      Hacl_Hash_SHA2_sha512_update_nblocks(128U, buf, block_state1);
    }
    uint32_t ite;
    if
    ((uint64_t)(chunk_len - diff) % (uint64_t)128U == 0ULL && (uint64_t)(chunk_len - diff) > 0ULL)
    {
      ite = 128U;
    }
    else
    {
      ite = (uint32_t)((uint64_t)(chunk_len - diff) % (uint64_t)128U);
    }
    uint32_t n_blocks = (chunk_len - diff - ite) / 128U;
    uint32_t data1_len = n_blocks * 128U;
    uint32_t data2_len = chunk_len - diff - data1_len;
    uint8_t *data1 = chunk2;
    uint8_t *data2 = chunk2 + data1_len;
    Hacl_Hash_SHA2_sha512_update_nblocks(data1_len / 128U * 128U, data1, block_state1);
    uint8_t *dst = buf;
    memcpy(dst, data2, data2_len * sizeof (uint8_t));
    *state =
      (
        (Hacl_Streaming_MD_state_64){
          .block_state = block_state1,
          .buf = buf,
          .total_len = total_len1 + (uint64_t)(chunk_len - diff)
        }
      );
  }
  return Hacl_Streaming_Types_Success;
}

/**
Feed an arbitrary amount of data into the hash. This function returns 0 for
success, or 1 if the combined length of all of the data passed to `update_512`
(since the last call to `reset_512`) exceeds 2^125-1 bytes.

This function is identical to the update function for SHA2_384.
*/
Hacl_Streaming_Types_error_code
Hacl_Hash_SHA2_update_512(
  Hacl_Streaming_MD_state_64 *state,
  uint8_t *input,
  uint32_t input_len
)
{
  return update_384_512(state, input, input_len);
}

/**
Write the resulting hash into `output`, an array of 64 bytes. The state remains
valid after a call to `digest_512`, meaning the user may feed more data into
the hash via `update_512`. (The digest_512 function operates on an internal copy of
the state and therefore does not invalidate the client-held state `p`.)
*/
void Hacl_Hash_SHA2_digest_512(Hacl_Streaming_MD_state_64 *state, uint8_t *output)
{
  Hacl_Streaming_MD_state_64 scrut = *state;
  uint64_t *block_state = scrut.block_state;
  uint8_t *buf_ = scrut.buf;
  uint64_t total_len = scrut.total_len;
  uint32_t r;
  if (total_len % (uint64_t)128U == 0ULL && total_len > 0ULL)
  {
    r = 128U;
  }
  else
  {
    r = (uint32_t)(total_len % (uint64_t)128U);
  }
  uint8_t *buf_1 = buf_;
  uint64_t tmp_block_state[8U] = { 0U };
  memcpy(tmp_block_state, block_state, 8U * sizeof (uint64_t));
  uint32_t ite;
  if (r % 128U == 0U && r > 0U)
  {
    ite = 128U;
  }
  else
  {
    ite = r % 128U;
  }
  uint8_t *buf_last = buf_1 + r - ite;
  uint8_t *buf_multi = buf_1;
  Hacl_Hash_SHA2_sha512_update_nblocks(0U, buf_multi, tmp_block_state);
  uint64_t prev_len_last = total_len - (uint64_t)r;
  Hacl_Hash_SHA2_sha512_update_last(FStar_UInt128_add(FStar_UInt128_uint64_to_uint128(prev_len_last),
      FStar_UInt128_uint64_to_uint128((uint64_t)r)),
    r,
    buf_last,
    tmp_block_state);
  Hacl_Hash_SHA2_sha512_finish(tmp_block_state, output);
}

/**
Free a state allocated with `malloc_512`.

This function is identical to the free function for SHA2_384.
*/
void Hacl_Hash_SHA2_free_512(Hacl_Streaming_MD_state_64 *state)
{
  Hacl_Streaming_MD_state_64 scrut = *state;
  uint8_t *buf = scrut.buf;
  uint64_t *block_state = scrut.block_state;
  KRML_HOST_FREE(block_state);
  KRML_HOST_FREE(buf);
  KRML_HOST_FREE(state);
}

/**
Hash `input`, of len `input_len`, into `output`, an array of 64 bytes.
*/
void Hacl_Hash_SHA2_hash_512(uint8_t *output, uint8_t *input, uint32_t input_len)
{
  uint8_t *ib = input;
  uint8_t *rb = output;
  uint64_t st[8U] = { 0U };
  Hacl_Hash_SHA2_sha512_init(st);
  uint32_t rem = input_len % 128U;
  FStar_UInt128_uint128 len_ = FStar_UInt128_uint64_to_uint128((uint64_t)input_len);
  Hacl_Hash_SHA2_sha512_update_nblocks(input_len, ib, st);
  uint32_t rem1 = input_len % 128U;
  uint8_t *b0 = ib;
  uint8_t *lb = b0 + input_len - rem1;
  Hacl_Hash_SHA2_sha512_update_last(len_, rem, lb, st);
  Hacl_Hash_SHA2_sha512_finish(st, rb);
}

Hacl_Streaming_MD_state_64 *Hacl_Hash_SHA2_malloc_384(void)
{
  uint8_t *buf = (uint8_t *)KRML_HOST_CALLOC(128U, sizeof (uint8_t));
  if (buf == NULL)
  {
    return NULL;
  }
  uint8_t *buf1 = buf;
  uint64_t *b = (uint64_t *)KRML_HOST_CALLOC(8U, sizeof (uint64_t));
  Hacl_Streaming_Types_optional_64 block_state;
  if (b == NULL)
  {
    block_state = ((Hacl_Streaming_Types_optional_64){ .tag = Hacl_Streaming_Types_None });
  }
  else
  {
    block_state = ((Hacl_Streaming_Types_optional_64){ .tag = Hacl_Streaming_Types_Some, .v = b });
  }
  if (block_state.tag == Hacl_Streaming_Types_None)
  {
    KRML_HOST_FREE(buf1);
    return NULL;
  }
  if (block_state.tag == Hacl_Streaming_Types_Some)
  {
    uint64_t *block_state1 = block_state.v;
    Hacl_Streaming_Types_optional k_ = Hacl_Streaming_Types_Some;
    switch (k_)
    {
      case Hacl_Streaming_Types_None:
        {
          return NULL;
        }
      case Hacl_Streaming_Types_Some:
        {
          Hacl_Streaming_MD_state_64
          s = { .block_state = block_state1, .buf = buf1, .total_len = (uint64_t)0U };
          Hacl_Streaming_MD_state_64
          *p = (Hacl_Streaming_MD_state_64 *)KRML_HOST_MALLOC(sizeof (Hacl_Streaming_MD_state_64));
          if (p != NULL)
          {
            p[0U] = s;
          }
          if (p == NULL)
          {
            KRML_HOST_FREE(block_state1);
            KRML_HOST_FREE(buf1);
            return NULL;
          }
          Hacl_Hash_SHA2_sha384_init(block_state1);
          return p;
        }
      default:
        {
          KRML_HOST_EPRINTF("KaRaMeL incomplete match at %s:%d\n", __FILE__, __LINE__);
          KRML_HOST_EXIT(253U);
        }
    }
  }
  KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n",
    __FILE__,
    __LINE__,
    "unreachable (pattern matches are exhaustive in F*)");
  KRML_HOST_EXIT(255U);
}

void Hacl_Hash_SHA2_reset_384(Hacl_Streaming_MD_state_64 *state)
{
  Hacl_Streaming_MD_state_64 scrut = *state;
  uint8_t *buf = scrut.buf;
  uint64_t *block_state = scrut.block_state;
  Hacl_Hash_SHA2_sha384_init(block_state);
  Hacl_Streaming_MD_state_64
  tmp = { .block_state = block_state, .buf = buf, .total_len = (uint64_t)0U };
  state[0U] = tmp;
}

Hacl_Streaming_Types_error_code
Hacl_Hash_SHA2_update_384(
  Hacl_Streaming_MD_state_64 *state,
  uint8_t *input,
  uint32_t input_len
)
{
  return update_384_512(state, input, input_len);
}

/**
Write the resulting hash into `output`, an array of 48 bytes. The state remains
valid after a call to `digest_384`, meaning the user may feed more data into
the hash via `update_384`.
*/
void Hacl_Hash_SHA2_digest_384(Hacl_Streaming_MD_state_64 *state, uint8_t *output)
{
  Hacl_Streaming_MD_state_64 scrut = *state;
  uint64_t *block_state = scrut.block_state;
  uint8_t *buf_ = scrut.buf;
  uint64_t total_len = scrut.total_len;
  uint32_t r;
  if (total_len % (uint64_t)128U == 0ULL && total_len > 0ULL)
  {
    r = 128U;
  }
  else
  {
    r = (uint32_t)(total_len % (uint64_t)128U);
  }
  uint8_t *buf_1 = buf_;
  uint64_t tmp_block_state[8U] = { 0U };
  memcpy(tmp_block_state, block_state, 8U * sizeof (uint64_t));
  uint32_t ite;
  if (r % 128U == 0U && r > 0U)
  {
    ite = 128U;
  }
  else
  {
    ite = r % 128U;
  }
  uint8_t *buf_last = buf_1 + r - ite;
  uint8_t *buf_multi = buf_1;
  Hacl_Hash_SHA2_sha384_update_nblocks(0U, buf_multi, tmp_block_state);
  uint64_t prev_len_last = total_len - (uint64_t)r;
  Hacl_Hash_SHA2_sha384_update_last(FStar_UInt128_add(FStar_UInt128_uint64_to_uint128(prev_len_last),
      FStar_UInt128_uint64_to_uint128((uint64_t)r)),
    r,
    buf_last,
    tmp_block_state);
  Hacl_Hash_SHA2_sha384_finish(tmp_block_state, output);
}

void Hacl_Hash_SHA2_free_384(Hacl_Streaming_MD_state_64 *state)
{
  Hacl_Hash_SHA2_free_512(state);
}

/**
Hash `input`, of len `input_len`, into `output`, an array of 48 bytes.
*/
void Hacl_Hash_SHA2_hash_384(uint8_t *output, uint8_t *input, uint32_t input_len)
{
  uint8_t *ib = input;
  uint8_t *rb = output;
  uint64_t st[8U] = { 0U };
  Hacl_Hash_SHA2_sha384_init(st);
  uint32_t rem = input_len % 128U;
  FStar_UInt128_uint128 len_ = FStar_UInt128_uint64_to_uint128((uint64_t)input_len);
  Hacl_Hash_SHA2_sha384_update_nblocks(input_len, ib, st);
  uint32_t rem1 = input_len % 128U;
  uint8_t *b0 = ib;
  uint8_t *lb = b0 + input_len - rem1;
  Hacl_Hash_SHA2_sha384_update_last(len_, rem, lb, st);
  Hacl_Hash_SHA2_sha384_finish(st, rb);
}

/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */




static inline void fsum(uint64_t *out, uint64_t *a, uint64_t *b)
{
  Hacl_Impl_Curve25519_Field51_fadd(out, a, b);
}

static inline void fdifference(uint64_t *out, uint64_t *a, uint64_t *b)
{
  Hacl_Impl_Curve25519_Field51_fsub(out, a, b);
}

void Hacl_Bignum25519_reduce_513(uint64_t *a)
{
  uint64_t f0 = a[0U];
  uint64_t f1 = a[1U];
  uint64_t f2 = a[2U];
  uint64_t f3 = a[3U];
  uint64_t f4 = a[4U];
  uint64_t l_ = f0 + 0ULL;
  uint64_t tmp0 = l_ & 0x7ffffffffffffULL;
  uint64_t c0 = l_ >> 51U;
  uint64_t l_0 = f1 + c0;
  uint64_t tmp1 = l_0 & 0x7ffffffffffffULL;
  uint64_t c1 = l_0 >> 51U;
  uint64_t l_1 = f2 + c1;
  uint64_t tmp2 = l_1 & 0x7ffffffffffffULL;
  uint64_t c2 = l_1 >> 51U;
  uint64_t l_2 = f3 + c2;
  uint64_t tmp3 = l_2 & 0x7ffffffffffffULL;
  uint64_t c3 = l_2 >> 51U;
  uint64_t l_3 = f4 + c3;
  uint64_t tmp4 = l_3 & 0x7ffffffffffffULL;
  uint64_t c4 = l_3 >> 51U;
  uint64_t l_4 = tmp0 + c4 * 19ULL;
  uint64_t tmp0_ = l_4 & 0x7ffffffffffffULL;
  uint64_t c5 = l_4 >> 51U;
  a[0U] = tmp0_;
  a[1U] = tmp1 + c5;
  a[2U] = tmp2;
  a[3U] = tmp3;
  a[4U] = tmp4;
}

static inline void fmul0(uint64_t *output, uint64_t *input, uint64_t *input2)
{
  FStar_UInt128_uint128 tmp[10U];
  for (uint32_t _i = 0U; _i < 10U; ++_i)
    tmp[_i] = FStar_UInt128_uint64_to_uint128(0ULL);
  Hacl_Impl_Curve25519_Field51_fmul(output, input, input2, tmp);
}

static inline void times_2(uint64_t *out, uint64_t *a)
{
  uint64_t a0 = a[0U];
  uint64_t a1 = a[1U];
  uint64_t a2 = a[2U];
  uint64_t a3 = a[3U];
  uint64_t a4 = a[4U];
  uint64_t o0 = 2ULL * a0;
  uint64_t o1 = 2ULL * a1;
  uint64_t o2 = 2ULL * a2;
  uint64_t o3 = 2ULL * a3;
  uint64_t o4 = 2ULL * a4;
  out[0U] = o0;
  out[1U] = o1;
  out[2U] = o2;
  out[3U] = o3;
  out[4U] = o4;
}

static inline void times_d(uint64_t *out, uint64_t *a)
{
  uint64_t d[5U] = { 0U };
  d[0U] = 0x00034dca135978a3ULL;
  d[1U] = 0x0001a8283b156ebdULL;
  d[2U] = 0x0005e7a26001c029ULL;
  d[3U] = 0x000739c663a03cbbULL;
  d[4U] = 0x00052036cee2b6ffULL;
  fmul0(out, d, a);
}

static inline void times_2d(uint64_t *out, uint64_t *a)
{
  uint64_t d2[5U] = { 0U };
  d2[0U] = 0x00069b9426b2f159ULL;
  d2[1U] = 0x00035050762add7aULL;
  d2[2U] = 0x0003cf44c0038052ULL;
  d2[3U] = 0x0006738cc7407977ULL;
  d2[4U] = 0x0002406d9dc56dffULL;
  fmul0(out, d2, a);
}

static inline void fsquare(uint64_t *out, uint64_t *a)
{
  FStar_UInt128_uint128 tmp[5U];
  for (uint32_t _i = 0U; _i < 5U; ++_i)
    tmp[_i] = FStar_UInt128_uint64_to_uint128(0ULL);
  Hacl_Impl_Curve25519_Field51_fsqr(out, a, tmp);
}

static inline void fsquare_times(uint64_t *output, uint64_t *input, uint32_t count)
{
  FStar_UInt128_uint128 tmp[5U];
  for (uint32_t _i = 0U; _i < 5U; ++_i)
    tmp[_i] = FStar_UInt128_uint64_to_uint128(0ULL);
  Hacl_Curve25519_51_fsquare_times(output, input, tmp, count);
}

static inline void fsquare_times_inplace(uint64_t *output, uint32_t count)
{
  FStar_UInt128_uint128 tmp[5U];
  for (uint32_t _i = 0U; _i < 5U; ++_i)
    tmp[_i] = FStar_UInt128_uint64_to_uint128(0ULL);
  Hacl_Curve25519_51_fsquare_times(output, output, tmp, count);
}

void Hacl_Bignum25519_inverse(uint64_t *out, uint64_t *a)
{
  FStar_UInt128_uint128 tmp[10U];
  for (uint32_t _i = 0U; _i < 10U; ++_i)
    tmp[_i] = FStar_UInt128_uint64_to_uint128(0ULL);
  Hacl_Curve25519_51_finv(out, a, tmp);
}

static inline void reduce(uint64_t *out)
{
  uint64_t o0 = out[0U];
  uint64_t o1 = out[1U];
  uint64_t o2 = out[2U];
  uint64_t o3 = out[3U];
  uint64_t o4 = out[4U];
  uint64_t l_ = o0 + 0ULL;
  uint64_t tmp0 = l_ & 0x7ffffffffffffULL;
  uint64_t c0 = l_ >> 51U;
  uint64_t l_0 = o1 + c0;
  uint64_t tmp1 = l_0 & 0x7ffffffffffffULL;
  uint64_t c1 = l_0 >> 51U;
  uint64_t l_1 = o2 + c1;
  uint64_t tmp2 = l_1 & 0x7ffffffffffffULL;
  uint64_t c2 = l_1 >> 51U;
  uint64_t l_2 = o3 + c2;
  uint64_t tmp3 = l_2 & 0x7ffffffffffffULL;
  uint64_t c3 = l_2 >> 51U;
  uint64_t l_3 = o4 + c3;
  uint64_t tmp4 = l_3 & 0x7ffffffffffffULL;
  uint64_t c4 = l_3 >> 51U;
  uint64_t l_4 = tmp0 + c4 * 19ULL;
  uint64_t tmp0_ = l_4 & 0x7ffffffffffffULL;
  uint64_t c5 = l_4 >> 51U;
  uint64_t f0 = tmp0_;
  uint64_t f1 = tmp1 + c5;
  uint64_t f2 = tmp2;
  uint64_t f3 = tmp3;
  uint64_t f4 = tmp4;
  uint64_t m0 = FStar_UInt64_gte_mask(f0, 0x7ffffffffffedULL);
  uint64_t m1 = FStar_UInt64_eq_mask(f1, 0x7ffffffffffffULL);
  uint64_t m2 = FStar_UInt64_eq_mask(f2, 0x7ffffffffffffULL);
  uint64_t m3 = FStar_UInt64_eq_mask(f3, 0x7ffffffffffffULL);
  uint64_t m4 = FStar_UInt64_eq_mask(f4, 0x7ffffffffffffULL);
  uint64_t mask = (((m0 & m1) & m2) & m3) & m4;
  uint64_t f0_ = f0 - (mask & 0x7ffffffffffedULL);
  uint64_t f1_ = f1 - (mask & 0x7ffffffffffffULL);
  uint64_t f2_ = f2 - (mask & 0x7ffffffffffffULL);
  uint64_t f3_ = f3 - (mask & 0x7ffffffffffffULL);
  uint64_t f4_ = f4 - (mask & 0x7ffffffffffffULL);
  uint64_t f01 = f0_;
  uint64_t f11 = f1_;
  uint64_t f21 = f2_;
  uint64_t f31 = f3_;
  uint64_t f41 = f4_;
  out[0U] = f01;
  out[1U] = f11;
  out[2U] = f21;
  out[3U] = f31;
  out[4U] = f41;
}

void Hacl_Bignum25519_load_51(uint64_t *output, uint8_t *input)
{
  uint64_t u64s[4U] = { 0U };
  KRML_MAYBE_FOR4(i,
    0U,
    4U,
    1U,
    uint64_t *os = u64s;
    uint8_t *bj = input + i * 8U;
    uint64_t u = load64_le(bj);
    uint64_t r = u;
    uint64_t x = r;
    os[i] = x;);
  uint64_t u64s3 = u64s[3U];
  u64s[3U] = u64s3 & 0x7fffffffffffffffULL;
  output[0U] = u64s[0U] & 0x7ffffffffffffULL;
  output[1U] = u64s[0U] >> 51U | (u64s[1U] & 0x3fffffffffULL) << 13U;
  output[2U] = u64s[1U] >> 38U | (u64s[2U] & 0x1ffffffULL) << 26U;
  output[3U] = u64s[2U] >> 25U | (u64s[3U] & 0xfffULL) << 39U;
  output[4U] = u64s[3U] >> 12U;
}

void Hacl_Bignum25519_store_51(uint8_t *output, uint64_t *input)
{
  uint64_t u64s[4U] = { 0U };
  Hacl_Impl_Curve25519_Field51_store_felem(u64s, input);
  KRML_MAYBE_FOR4(i, 0U, 4U, 1U, store64_le(output + i * 8U, u64s[i]););
}

void Hacl_Impl_Ed25519_PointDouble_point_double(uint64_t *out, uint64_t *p)
{
  uint64_t tmp[20U] = { 0U };
  uint64_t *tmp1 = tmp;
  uint64_t *tmp20 = tmp + 5U;
  uint64_t *tmp30 = tmp + 10U;
  uint64_t *tmp40 = tmp + 15U;
  uint64_t *x10 = p;
  uint64_t *y10 = p + 5U;
  uint64_t *z1 = p + 10U;
  fsquare(tmp1, x10);
  fsquare(tmp20, y10);
  fsum(tmp30, tmp1, tmp20);
  fdifference(tmp40, tmp1, tmp20);
  fsquare(tmp1, z1);
  times_2(tmp1, tmp1);
  uint64_t *tmp10 = tmp;
  uint64_t *tmp2 = tmp + 5U;
  uint64_t *tmp3 = tmp + 10U;
  uint64_t *tmp4 = tmp + 15U;
  uint64_t *x1 = p;
  uint64_t *y1 = p + 5U;
  fsum(tmp2, x1, y1);
  fsquare(tmp2, tmp2);
  Hacl_Bignum25519_reduce_513(tmp3);
  fdifference(tmp2, tmp3, tmp2);
  Hacl_Bignum25519_reduce_513(tmp10);
  Hacl_Bignum25519_reduce_513(tmp4);
  fsum(tmp10, tmp10, tmp4);
  uint64_t *tmp_f = tmp;
  uint64_t *tmp_e = tmp + 5U;
  uint64_t *tmp_h = tmp + 10U;
  uint64_t *tmp_g = tmp + 15U;
  uint64_t *x3 = out;
  uint64_t *y3 = out + 5U;
  uint64_t *z3 = out + 10U;
  uint64_t *t3 = out + 15U;
  fmul0(x3, tmp_e, tmp_f);
  fmul0(y3, tmp_g, tmp_h);
  fmul0(t3, tmp_e, tmp_h);
  fmul0(z3, tmp_f, tmp_g);
}

void Hacl_Impl_Ed25519_PointAdd_point_add(uint64_t *out, uint64_t *p, uint64_t *q)
{
  uint64_t tmp[30U] = { 0U };
  uint64_t *tmp1 = tmp;
  uint64_t *tmp20 = tmp + 5U;
  uint64_t *tmp30 = tmp + 10U;
  uint64_t *tmp40 = tmp + 15U;
  uint64_t *x1 = p;
  uint64_t *y1 = p + 5U;
  uint64_t *x2 = q;
  uint64_t *y2 = q + 5U;
  fdifference(tmp1, y1, x1);
  fdifference(tmp20, y2, x2);
  fmul0(tmp30, tmp1, tmp20);
  fsum(tmp1, y1, x1);
  fsum(tmp20, y2, x2);
  fmul0(tmp40, tmp1, tmp20);
  uint64_t *tmp10 = tmp;
  uint64_t *tmp2 = tmp + 5U;
  uint64_t *tmp3 = tmp + 10U;
  uint64_t *tmp4 = tmp + 15U;
  uint64_t *tmp5 = tmp + 20U;
  uint64_t *tmp6 = tmp + 25U;
  uint64_t *z1 = p + 10U;
  uint64_t *t1 = p + 15U;
  uint64_t *z2 = q + 10U;
  uint64_t *t2 = q + 15U;
  times_2d(tmp10, t1);
  fmul0(tmp10, tmp10, t2);
  times_2(tmp2, z1);
  fmul0(tmp2, tmp2, z2);
  fdifference(tmp5, tmp4, tmp3);
  fdifference(tmp6, tmp2, tmp10);
  fsum(tmp10, tmp2, tmp10);
  fsum(tmp2, tmp4, tmp3);
  uint64_t *tmp_g = tmp;
  uint64_t *tmp_h = tmp + 5U;
  uint64_t *tmp_e = tmp + 20U;
  uint64_t *tmp_f = tmp + 25U;
  uint64_t *x3 = out;
  uint64_t *y3 = out + 5U;
  uint64_t *z3 = out + 10U;
  uint64_t *t3 = out + 15U;
  fmul0(x3, tmp_e, tmp_f);
  fmul0(y3, tmp_g, tmp_h);
  fmul0(t3, tmp_e, tmp_h);
  fmul0(z3, tmp_f, tmp_g);
}

void Hacl_Impl_Ed25519_PointConstants_make_point_inf(uint64_t *b)
{
  uint64_t *x = b;
  uint64_t *y = b + 5U;
  uint64_t *z = b + 10U;
  uint64_t *t = b + 15U;
  x[0U] = 0ULL;
  x[1U] = 0ULL;
  x[2U] = 0ULL;
  x[3U] = 0ULL;
  x[4U] = 0ULL;
  y[0U] = 1ULL;
  y[1U] = 0ULL;
  y[2U] = 0ULL;
  y[3U] = 0ULL;
  y[4U] = 0ULL;
  z[0U] = 1ULL;
  z[1U] = 0ULL;
  z[2U] = 0ULL;
  z[3U] = 0ULL;
  z[4U] = 0ULL;
  t[0U] = 0ULL;
  t[1U] = 0ULL;
  t[2U] = 0ULL;
  t[3U] = 0ULL;
  t[4U] = 0ULL;
}

static inline void pow2_252m2(uint64_t *out, uint64_t *z)
{
  uint64_t buf[20U] = { 0U };
  uint64_t *a = buf;
  uint64_t *t00 = buf + 5U;
  uint64_t *b0 = buf + 10U;
  uint64_t *c0 = buf + 15U;
  fsquare_times(a, z, 1U);
  fsquare_times(t00, a, 2U);
  fmul0(b0, t00, z);
  fmul0(a, b0, a);
  fsquare_times(t00, a, 1U);
  fmul0(b0, t00, b0);
  fsquare_times(t00, b0, 5U);
  fmul0(b0, t00, b0);
  fsquare_times(t00, b0, 10U);
  fmul0(c0, t00, b0);
  fsquare_times(t00, c0, 20U);
  fmul0(t00, t00, c0);
  fsquare_times_inplace(t00, 10U);
  fmul0(b0, t00, b0);
  fsquare_times(t00, b0, 50U);
  uint64_t *a0 = buf;
  uint64_t *t0 = buf + 5U;
  uint64_t *b = buf + 10U;
  uint64_t *c = buf + 15U;
  fsquare_times(a0, z, 1U);
  fmul0(c, t0, b);
  fsquare_times(t0, c, 100U);
  fmul0(t0, t0, c);
  fsquare_times_inplace(t0, 50U);
  fmul0(t0, t0, b);
  fsquare_times_inplace(t0, 2U);
  fmul0(out, t0, a0);
}

static inline bool is_0(uint64_t *x)
{
  uint64_t x0 = x[0U];
  uint64_t x1 = x[1U];
  uint64_t x2 = x[2U];
  uint64_t x3 = x[3U];
  uint64_t x4 = x[4U];
  return x0 == 0ULL && x1 == 0ULL && x2 == 0ULL && x3 == 0ULL && x4 == 0ULL;
}

static inline void mul_modp_sqrt_m1(uint64_t *x)
{
  uint64_t sqrt_m1[5U] = { 0U };
  sqrt_m1[0U] = 0x00061b274a0ea0b0ULL;
  sqrt_m1[1U] = 0x0000d5a5fc8f189dULL;
  sqrt_m1[2U] = 0x0007ef5e9cbd0c60ULL;
  sqrt_m1[3U] = 0x00078595a6804c9eULL;
  sqrt_m1[4U] = 0x0002b8324804fc1dULL;
  fmul0(x, x, sqrt_m1);
}

static inline bool recover_x(uint64_t *x, uint64_t *y, uint64_t sign)
{
  uint64_t tmp[15U] = { 0U };
  uint64_t *x2 = tmp;
  uint64_t x00 = y[0U];
  uint64_t x1 = y[1U];
  uint64_t x21 = y[2U];
  uint64_t x30 = y[3U];
  uint64_t x4 = y[4U];
  bool
  b =
    x00 >= 0x7ffffffffffedULL && x1 == 0x7ffffffffffffULL && x21 == 0x7ffffffffffffULL &&
      x30 == 0x7ffffffffffffULL
    && x4 == 0x7ffffffffffffULL;
  bool res;
  if (b)
  {
    res = false;
  }
  else
  {
    uint64_t tmp1[20U] = { 0U };
    uint64_t *one = tmp1;
    uint64_t *y2 = tmp1 + 5U;
    uint64_t *dyyi = tmp1 + 10U;
    uint64_t *dyy = tmp1 + 15U;
    one[0U] = 1ULL;
    one[1U] = 0ULL;
    one[2U] = 0ULL;
    one[3U] = 0ULL;
    one[4U] = 0ULL;
    fsquare(y2, y);
    times_d(dyy, y2);
    fsum(dyy, dyy, one);
    Hacl_Bignum25519_reduce_513(dyy);
    Hacl_Bignum25519_inverse(dyyi, dyy);
    fdifference(x2, y2, one);
    fmul0(x2, x2, dyyi);
    reduce(x2);
    bool x2_is_0 = is_0(x2);
    uint8_t z;
    if (x2_is_0)
    {
      if (sign == 0ULL)
      {
        x[0U] = 0ULL;
        x[1U] = 0ULL;
        x[2U] = 0ULL;
        x[3U] = 0ULL;
        x[4U] = 0ULL;
        z = 1U;
      }
      else
      {
        z = 0U;
      }
    }
    else
    {
      z = 2U;
    }
    if (z == 0U)
    {
      res = false;
    }
    else if (z == 1U)
    {
      res = true;
    }
    else
    {
      uint64_t *x210 = tmp;
      uint64_t *x31 = tmp + 5U;
      uint64_t *t00 = tmp + 10U;
      pow2_252m2(x31, x210);
      fsquare(t00, x31);
      fdifference(t00, t00, x210);
      Hacl_Bignum25519_reduce_513(t00);
      reduce(t00);
      bool t0_is_0 = is_0(t00);
      if (!t0_is_0)
      {
        mul_modp_sqrt_m1(x31);
      }
      uint64_t *x211 = tmp;
      uint64_t *x3 = tmp + 5U;
      uint64_t *t01 = tmp + 10U;
      fsquare(t01, x3);
      fdifference(t01, t01, x211);
      Hacl_Bignum25519_reduce_513(t01);
      reduce(t01);
      bool z1 = is_0(t01);
      if (z1)
      {
        uint64_t *x32 = tmp + 5U;
        uint64_t *t0 = tmp + 10U;
        reduce(x32);
        uint64_t x0 = x32[0U];
        uint64_t x01 = x0 & 1ULL;
        if (!(x01 == sign))
        {
          t0[0U] = 0ULL;
          t0[1U] = 0ULL;
          t0[2U] = 0ULL;
          t0[3U] = 0ULL;
          t0[4U] = 0ULL;
          fdifference(x32, t0, x32);
          Hacl_Bignum25519_reduce_513(x32);
          reduce(x32);
        }
        memcpy(x, x32, 5U * sizeof (uint64_t));
        res = true;
      }
      else
      {
        res = false;
      }
    }
  }
  bool res0 = res;
  return res0;
}

bool Hacl_Impl_Ed25519_PointDecompress_point_decompress(uint64_t *out, uint8_t *s)
{
  uint64_t tmp[10U] = { 0U };
  uint64_t *y = tmp;
  uint64_t *x = tmp + 5U;
  uint8_t s31 = s[31U];
  uint8_t z = (uint32_t)s31 >> 7U;
  uint64_t sign = (uint64_t)z;
  Hacl_Bignum25519_load_51(y, s);
  bool z0 = recover_x(x, y, sign);
  bool res;
  if (z0)
  {
    uint64_t *outx = out;
    uint64_t *outy = out + 5U;
    uint64_t *outz = out + 10U;
    uint64_t *outt = out + 15U;
    memcpy(outx, x, 5U * sizeof (uint64_t));
    memcpy(outy, y, 5U * sizeof (uint64_t));
    outz[0U] = 1ULL;
    outz[1U] = 0ULL;
    outz[2U] = 0ULL;
    outz[3U] = 0ULL;
    outz[4U] = 0ULL;
    fmul0(outt, x, y);
    res = true;
  }
  else
  {
    res = false;
  }
  bool res0 = res;
  return res0;
}

void Hacl_Impl_Ed25519_PointCompress_point_compress(uint8_t *z, uint64_t *p)
{
  uint64_t tmp[15U] = { 0U };
  uint64_t *x = tmp + 5U;
  uint64_t *out = tmp + 10U;
  uint64_t *zinv1 = tmp;
  uint64_t *x1 = tmp + 5U;
  uint64_t *out1 = tmp + 10U;
  uint64_t *px = p;
  uint64_t *py = p + 5U;
  uint64_t *pz = p + 10U;
  Hacl_Bignum25519_inverse(zinv1, pz);
  fmul0(x1, px, zinv1);
  reduce(x1);
  fmul0(out1, py, zinv1);
  Hacl_Bignum25519_reduce_513(out1);
  uint64_t x0 = x[0U];
  uint64_t b = x0 & 1ULL;
  Hacl_Bignum25519_store_51(z, out);
  uint8_t xbyte = (uint8_t)b;
  uint8_t o31 = z[31U];
  z[31U] = (uint32_t)o31 + ((uint32_t)xbyte << 7U);
}

static inline void barrett_reduction(uint64_t *z, uint64_t *t)
{
  uint64_t t0 = t[0U];
  uint64_t t1 = t[1U];
  uint64_t t2 = t[2U];
  uint64_t t3 = t[3U];
  uint64_t t4 = t[4U];
  uint64_t t5 = t[5U];
  uint64_t t6 = t[6U];
  uint64_t t7 = t[7U];
  uint64_t t8 = t[8U];
  uint64_t t9 = t[9U];
  uint64_t m00 = 0x12631a5cf5d3edULL;
  uint64_t m10 = 0xf9dea2f79cd658ULL;
  uint64_t m20 = 0x000000000014deULL;
  uint64_t m30 = 0x00000000000000ULL;
  uint64_t m40 = 0x00000010000000ULL;
  uint64_t m0 = m00;
  uint64_t m1 = m10;
  uint64_t m2 = m20;
  uint64_t m3 = m30;
  uint64_t m4 = m40;
  uint64_t m010 = 0x9ce5a30a2c131bULL;
  uint64_t m110 = 0x215d086329a7edULL;
  uint64_t m210 = 0xffffffffeb2106ULL;
  uint64_t m310 = 0xffffffffffffffULL;
  uint64_t m410 = 0x00000fffffffffULL;
  uint64_t mu0 = m010;
  uint64_t mu1 = m110;
  uint64_t mu2 = m210;
  uint64_t mu3 = m310;
  uint64_t mu4 = m410;
  uint64_t y_ = (t5 & 0xffffffULL) << 32U;
  uint64_t x_ = t4 >> 24U;
  uint64_t z00 = x_ | y_;
  uint64_t y_0 = (t6 & 0xffffffULL) << 32U;
  uint64_t x_0 = t5 >> 24U;
  uint64_t z10 = x_0 | y_0;
  uint64_t y_1 = (t7 & 0xffffffULL) << 32U;
  uint64_t x_1 = t6 >> 24U;
  uint64_t z20 = x_1 | y_1;
  uint64_t y_2 = (t8 & 0xffffffULL) << 32U;
  uint64_t x_2 = t7 >> 24U;
  uint64_t z30 = x_2 | y_2;
  uint64_t y_3 = (t9 & 0xffffffULL) << 32U;
  uint64_t x_3 = t8 >> 24U;
  uint64_t z40 = x_3 | y_3;
  uint64_t q0 = z00;
  uint64_t q1 = z10;
  uint64_t q2 = z20;
  uint64_t q3 = z30;
  uint64_t q4 = z40;
  FStar_UInt128_uint128 xy000 = FStar_UInt128_mul_wide(q0, mu0);
  FStar_UInt128_uint128 xy010 = FStar_UInt128_mul_wide(q0, mu1);
  FStar_UInt128_uint128 xy020 = FStar_UInt128_mul_wide(q0, mu2);
  FStar_UInt128_uint128 xy030 = FStar_UInt128_mul_wide(q0, mu3);
  FStar_UInt128_uint128 xy040 = FStar_UInt128_mul_wide(q0, mu4);
  FStar_UInt128_uint128 xy100 = FStar_UInt128_mul_wide(q1, mu0);
  FStar_UInt128_uint128 xy110 = FStar_UInt128_mul_wide(q1, mu1);
  FStar_UInt128_uint128 xy120 = FStar_UInt128_mul_wide(q1, mu2);
  FStar_UInt128_uint128 xy130 = FStar_UInt128_mul_wide(q1, mu3);
  FStar_UInt128_uint128 xy14 = FStar_UInt128_mul_wide(q1, mu4);
  FStar_UInt128_uint128 xy200 = FStar_UInt128_mul_wide(q2, mu0);
  FStar_UInt128_uint128 xy210 = FStar_UInt128_mul_wide(q2, mu1);
  FStar_UInt128_uint128 xy220 = FStar_UInt128_mul_wide(q2, mu2);
  FStar_UInt128_uint128 xy23 = FStar_UInt128_mul_wide(q2, mu3);
  FStar_UInt128_uint128 xy24 = FStar_UInt128_mul_wide(q2, mu4);
  FStar_UInt128_uint128 xy300 = FStar_UInt128_mul_wide(q3, mu0);
  FStar_UInt128_uint128 xy310 = FStar_UInt128_mul_wide(q3, mu1);
  FStar_UInt128_uint128 xy32 = FStar_UInt128_mul_wide(q3, mu2);
  FStar_UInt128_uint128 xy33 = FStar_UInt128_mul_wide(q3, mu3);
  FStar_UInt128_uint128 xy34 = FStar_UInt128_mul_wide(q3, mu4);
  FStar_UInt128_uint128 xy400 = FStar_UInt128_mul_wide(q4, mu0);
  FStar_UInt128_uint128 xy41 = FStar_UInt128_mul_wide(q4, mu1);
  FStar_UInt128_uint128 xy42 = FStar_UInt128_mul_wide(q4, mu2);
  FStar_UInt128_uint128 xy43 = FStar_UInt128_mul_wide(q4, mu3);
  FStar_UInt128_uint128 xy44 = FStar_UInt128_mul_wide(q4, mu4);
  FStar_UInt128_uint128 z01 = xy000;
  FStar_UInt128_uint128 z11 = FStar_UInt128_add_mod(xy010, xy100);
  FStar_UInt128_uint128 z21 = FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy020, xy110), xy200);
  FStar_UInt128_uint128
  z31 =
    FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy030, xy120), xy210),
      xy300);
  FStar_UInt128_uint128
  z41 =
    FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy040,
            xy130),
          xy220),
        xy310),
      xy400);
  FStar_UInt128_uint128
  z5 =
    FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy14, xy23), xy32),
      xy41);
  FStar_UInt128_uint128 z6 = FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy24, xy33), xy42);
  FStar_UInt128_uint128 z7 = FStar_UInt128_add_mod(xy34, xy43);
  FStar_UInt128_uint128 z8 = xy44;
  FStar_UInt128_uint128 carry0 = FStar_UInt128_shift_right(z01, 56U);
  FStar_UInt128_uint128 c00 = carry0;
  FStar_UInt128_uint128 carry1 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z11, c00), 56U);
  FStar_UInt128_uint128 c10 = carry1;
  FStar_UInt128_uint128 carry2 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z21, c10), 56U);
  FStar_UInt128_uint128 c20 = carry2;
  FStar_UInt128_uint128 carry3 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z31, c20), 56U);
  FStar_UInt128_uint128 c30 = carry3;
  FStar_UInt128_uint128 carry4 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z41, c30), 56U);
  uint64_t
  t100 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(z41, c30)) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c40 = carry4;
  uint64_t t410 = t100;
  FStar_UInt128_uint128 carry5 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z5, c40), 56U);
  uint64_t
  t101 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(z5, c40)) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c5 = carry5;
  uint64_t t51 = t101;
  FStar_UInt128_uint128 carry6 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z6, c5), 56U);
  uint64_t
  t102 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(z6, c5)) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c6 = carry6;
  uint64_t t61 = t102;
  FStar_UInt128_uint128 carry7 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z7, c6), 56U);
  uint64_t
  t103 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(z7, c6)) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c7 = carry7;
  uint64_t t71 = t103;
  FStar_UInt128_uint128 carry8 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z8, c7), 56U);
  uint64_t
  t104 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(z8, c7)) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c8 = carry8;
  uint64_t t81 = t104;
  uint64_t t91 = FStar_UInt128_uint128_to_uint64(c8);
  uint64_t qmu4_ = t410;
  uint64_t qmu5_ = t51;
  uint64_t qmu6_ = t61;
  uint64_t qmu7_ = t71;
  uint64_t qmu8_ = t81;
  uint64_t qmu9_ = t91;
  uint64_t y_4 = (qmu5_ & 0xffffffffffULL) << 16U;
  uint64_t x_4 = qmu4_ >> 40U;
  uint64_t z02 = x_4 | y_4;
  uint64_t y_5 = (qmu6_ & 0xffffffffffULL) << 16U;
  uint64_t x_5 = qmu5_ >> 40U;
  uint64_t z12 = x_5 | y_5;
  uint64_t y_6 = (qmu7_ & 0xffffffffffULL) << 16U;
  uint64_t x_6 = qmu6_ >> 40U;
  uint64_t z22 = x_6 | y_6;
  uint64_t y_7 = (qmu8_ & 0xffffffffffULL) << 16U;
  uint64_t x_7 = qmu7_ >> 40U;
  uint64_t z32 = x_7 | y_7;
  uint64_t y_8 = (qmu9_ & 0xffffffffffULL) << 16U;
  uint64_t x_8 = qmu8_ >> 40U;
  uint64_t z42 = x_8 | y_8;
  uint64_t qdiv0 = z02;
  uint64_t qdiv1 = z12;
  uint64_t qdiv2 = z22;
  uint64_t qdiv3 = z32;
  uint64_t qdiv4 = z42;
  uint64_t r0 = t0;
  uint64_t r1 = t1;
  uint64_t r2 = t2;
  uint64_t r3 = t3;
  uint64_t r4 = t4 & 0xffffffffffULL;
  FStar_UInt128_uint128 xy00 = FStar_UInt128_mul_wide(qdiv0, m0);
  FStar_UInt128_uint128 xy01 = FStar_UInt128_mul_wide(qdiv0, m1);
  FStar_UInt128_uint128 xy02 = FStar_UInt128_mul_wide(qdiv0, m2);
  FStar_UInt128_uint128 xy03 = FStar_UInt128_mul_wide(qdiv0, m3);
  FStar_UInt128_uint128 xy04 = FStar_UInt128_mul_wide(qdiv0, m4);
  FStar_UInt128_uint128 xy10 = FStar_UInt128_mul_wide(qdiv1, m0);
  FStar_UInt128_uint128 xy11 = FStar_UInt128_mul_wide(qdiv1, m1);
  FStar_UInt128_uint128 xy12 = FStar_UInt128_mul_wide(qdiv1, m2);
  FStar_UInt128_uint128 xy13 = FStar_UInt128_mul_wide(qdiv1, m3);
  FStar_UInt128_uint128 xy20 = FStar_UInt128_mul_wide(qdiv2, m0);
  FStar_UInt128_uint128 xy21 = FStar_UInt128_mul_wide(qdiv2, m1);
  FStar_UInt128_uint128 xy22 = FStar_UInt128_mul_wide(qdiv2, m2);
  FStar_UInt128_uint128 xy30 = FStar_UInt128_mul_wide(qdiv3, m0);
  FStar_UInt128_uint128 xy31 = FStar_UInt128_mul_wide(qdiv3, m1);
  FStar_UInt128_uint128 xy40 = FStar_UInt128_mul_wide(qdiv4, m0);
  FStar_UInt128_uint128 carry9 = FStar_UInt128_shift_right(xy00, 56U);
  uint64_t t105 = FStar_UInt128_uint128_to_uint64(xy00) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c0 = carry9;
  uint64_t t010 = t105;
  FStar_UInt128_uint128
  carry10 =
    FStar_UInt128_shift_right(FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy01, xy10), c0),
      56U);
  uint64_t
  t106 =
    FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy01, xy10), c0)) &
      0xffffffffffffffULL;
  FStar_UInt128_uint128 c11 = carry10;
  uint64_t t110 = t106;
  FStar_UInt128_uint128
  carry11 =
    FStar_UInt128_shift_right(FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy02,
            xy11),
          xy20),
        c11),
      56U);
  uint64_t
  t107 =
    FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy02,
            xy11),
          xy20),
        c11))
    & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c21 = carry11;
  uint64_t t210 = t107;
  FStar_UInt128_uint128
  carry =
    FStar_UInt128_shift_right(FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy03,
              xy12),
            xy21),
          xy30),
        c21),
      56U);
  uint64_t
  t108 =
    FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy03,
              xy12),
            xy21),
          xy30),
        c21))
    & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c31 = carry;
  uint64_t t310 = t108;
  uint64_t
  t411 =
    FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy04,
                xy13),
              xy22),
            xy31),
          xy40),
        c31))
    & 0xffffffffffULL;
  uint64_t qmul0 = t010;
  uint64_t qmul1 = t110;
  uint64_t qmul2 = t210;
  uint64_t qmul3 = t310;
  uint64_t qmul4 = t411;
  uint64_t b5 = (r0 - qmul0) >> 63U;
  uint64_t t109 = (b5 << 56U) + r0 - qmul0;
  uint64_t c1 = b5;
  uint64_t t011 = t109;
  uint64_t b6 = (r1 - (qmul1 + c1)) >> 63U;
  uint64_t t1010 = (b6 << 56U) + r1 - (qmul1 + c1);
  uint64_t c2 = b6;
  uint64_t t111 = t1010;
  uint64_t b7 = (r2 - (qmul2 + c2)) >> 63U;
  uint64_t t1011 = (b7 << 56U) + r2 - (qmul2 + c2);
  uint64_t c3 = b7;
  uint64_t t211 = t1011;
  uint64_t b8 = (r3 - (qmul3 + c3)) >> 63U;
  uint64_t t1012 = (b8 << 56U) + r3 - (qmul3 + c3);
  uint64_t c4 = b8;
  uint64_t t311 = t1012;
  uint64_t b9 = (r4 - (qmul4 + c4)) >> 63U;
  uint64_t t1013 = (b9 << 40U) + r4 - (qmul4 + c4);
  uint64_t t412 = t1013;
  uint64_t s0 = t011;
  uint64_t s1 = t111;
  uint64_t s2 = t211;
  uint64_t s3 = t311;
  uint64_t s4 = t412;
  uint64_t m01 = 0x12631a5cf5d3edULL;
  uint64_t m11 = 0xf9dea2f79cd658ULL;
  uint64_t m21 = 0x000000000014deULL;
  uint64_t m31 = 0x00000000000000ULL;
  uint64_t m41 = 0x00000010000000ULL;
  uint64_t y0 = m01;
  uint64_t y1 = m11;
  uint64_t y2 = m21;
  uint64_t y3 = m31;
  uint64_t y4 = m41;
  uint64_t b10 = (s0 - y0) >> 63U;
  uint64_t t1014 = (b10 << 56U) + s0 - y0;
  uint64_t b0 = b10;
  uint64_t t01 = t1014;
  uint64_t b11 = (s1 - (y1 + b0)) >> 63U;
  uint64_t t1015 = (b11 << 56U) + s1 - (y1 + b0);
  uint64_t b1 = b11;
  uint64_t t11 = t1015;
  uint64_t b12 = (s2 - (y2 + b1)) >> 63U;
  uint64_t t1016 = (b12 << 56U) + s2 - (y2 + b1);
  uint64_t b2 = b12;
  uint64_t t21 = t1016;
  uint64_t b13 = (s3 - (y3 + b2)) >> 63U;
  uint64_t t1017 = (b13 << 56U) + s3 - (y3 + b2);
  uint64_t b3 = b13;
  uint64_t t31 = t1017;
  uint64_t b = (s4 - (y4 + b3)) >> 63U;
  uint64_t t10 = (b << 56U) + s4 - (y4 + b3);
  uint64_t b4 = b;
  uint64_t t41 = t10;
  uint64_t mask = b4 - 1ULL;
  uint64_t z03 = s0 ^ (mask & (s0 ^ t01));
  uint64_t z13 = s1 ^ (mask & (s1 ^ t11));
  uint64_t z23 = s2 ^ (mask & (s2 ^ t21));
  uint64_t z33 = s3 ^ (mask & (s3 ^ t31));
  uint64_t z43 = s4 ^ (mask & (s4 ^ t41));
  uint64_t z04 = z03;
  uint64_t z14 = z13;
  uint64_t z24 = z23;
  uint64_t z34 = z33;
  uint64_t z44 = z43;
  uint64_t o0 = z04;
  uint64_t o1 = z14;
  uint64_t o2 = z24;
  uint64_t o3 = z34;
  uint64_t o4 = z44;
  uint64_t z0 = o0;
  uint64_t z1 = o1;
  uint64_t z2 = o2;
  uint64_t z3 = o3;
  uint64_t z4 = o4;
  z[0U] = z0;
  z[1U] = z1;
  z[2U] = z2;
  z[3U] = z3;
  z[4U] = z4;
}

static inline void mul_modq(uint64_t *out, uint64_t *x, uint64_t *y)
{
  uint64_t tmp[10U] = { 0U };
  uint64_t x0 = x[0U];
  uint64_t x1 = x[1U];
  uint64_t x2 = x[2U];
  uint64_t x3 = x[3U];
  uint64_t x4 = x[4U];
  uint64_t y0 = y[0U];
  uint64_t y1 = y[1U];
  uint64_t y2 = y[2U];
  uint64_t y3 = y[3U];
  uint64_t y4 = y[4U];
  FStar_UInt128_uint128 xy00 = FStar_UInt128_mul_wide(x0, y0);
  FStar_UInt128_uint128 xy01 = FStar_UInt128_mul_wide(x0, y1);
  FStar_UInt128_uint128 xy02 = FStar_UInt128_mul_wide(x0, y2);
  FStar_UInt128_uint128 xy03 = FStar_UInt128_mul_wide(x0, y3);
  FStar_UInt128_uint128 xy04 = FStar_UInt128_mul_wide(x0, y4);
  FStar_UInt128_uint128 xy10 = FStar_UInt128_mul_wide(x1, y0);
  FStar_UInt128_uint128 xy11 = FStar_UInt128_mul_wide(x1, y1);
  FStar_UInt128_uint128 xy12 = FStar_UInt128_mul_wide(x1, y2);
  FStar_UInt128_uint128 xy13 = FStar_UInt128_mul_wide(x1, y3);
  FStar_UInt128_uint128 xy14 = FStar_UInt128_mul_wide(x1, y4);
  FStar_UInt128_uint128 xy20 = FStar_UInt128_mul_wide(x2, y0);
  FStar_UInt128_uint128 xy21 = FStar_UInt128_mul_wide(x2, y1);
  FStar_UInt128_uint128 xy22 = FStar_UInt128_mul_wide(x2, y2);
  FStar_UInt128_uint128 xy23 = FStar_UInt128_mul_wide(x2, y3);
  FStar_UInt128_uint128 xy24 = FStar_UInt128_mul_wide(x2, y4);
  FStar_UInt128_uint128 xy30 = FStar_UInt128_mul_wide(x3, y0);
  FStar_UInt128_uint128 xy31 = FStar_UInt128_mul_wide(x3, y1);
  FStar_UInt128_uint128 xy32 = FStar_UInt128_mul_wide(x3, y2);
  FStar_UInt128_uint128 xy33 = FStar_UInt128_mul_wide(x3, y3);
  FStar_UInt128_uint128 xy34 = FStar_UInt128_mul_wide(x3, y4);
  FStar_UInt128_uint128 xy40 = FStar_UInt128_mul_wide(x4, y0);
  FStar_UInt128_uint128 xy41 = FStar_UInt128_mul_wide(x4, y1);
  FStar_UInt128_uint128 xy42 = FStar_UInt128_mul_wide(x4, y2);
  FStar_UInt128_uint128 xy43 = FStar_UInt128_mul_wide(x4, y3);
  FStar_UInt128_uint128 xy44 = FStar_UInt128_mul_wide(x4, y4);
  FStar_UInt128_uint128 z00 = xy00;
  FStar_UInt128_uint128 z10 = FStar_UInt128_add_mod(xy01, xy10);
  FStar_UInt128_uint128 z20 = FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy02, xy11), xy20);
  FStar_UInt128_uint128
  z30 =
    FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy03, xy12), xy21),
      xy30);
  FStar_UInt128_uint128
  z40 =
    FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy04,
            xy13),
          xy22),
        xy31),
      xy40);
  FStar_UInt128_uint128
  z50 =
    FStar_UInt128_add_mod(FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy14, xy23), xy32),
      xy41);
  FStar_UInt128_uint128 z60 = FStar_UInt128_add_mod(FStar_UInt128_add_mod(xy24, xy33), xy42);
  FStar_UInt128_uint128 z70 = FStar_UInt128_add_mod(xy34, xy43);
  FStar_UInt128_uint128 z80 = xy44;
  FStar_UInt128_uint128 carry0 = FStar_UInt128_shift_right(z00, 56U);
  uint64_t t10 = FStar_UInt128_uint128_to_uint64(z00) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c0 = carry0;
  uint64_t t0 = t10;
  FStar_UInt128_uint128 carry1 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z10, c0), 56U);
  uint64_t
  t11 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(z10, c0)) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c1 = carry1;
  uint64_t t1 = t11;
  FStar_UInt128_uint128 carry2 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z20, c1), 56U);
  uint64_t
  t12 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(z20, c1)) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c2 = carry2;
  uint64_t t2 = t12;
  FStar_UInt128_uint128 carry3 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z30, c2), 56U);
  uint64_t
  t13 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(z30, c2)) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c3 = carry3;
  uint64_t t3 = t13;
  FStar_UInt128_uint128 carry4 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z40, c3), 56U);
  uint64_t
  t14 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(z40, c3)) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c4 = carry4;
  uint64_t t4 = t14;
  FStar_UInt128_uint128 carry5 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z50, c4), 56U);
  uint64_t
  t15 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(z50, c4)) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c5 = carry5;
  uint64_t t5 = t15;
  FStar_UInt128_uint128 carry6 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z60, c5), 56U);
  uint64_t
  t16 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(z60, c5)) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c6 = carry6;
  uint64_t t6 = t16;
  FStar_UInt128_uint128 carry7 = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z70, c6), 56U);
  uint64_t
  t17 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(z70, c6)) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c7 = carry7;
  uint64_t t7 = t17;
  FStar_UInt128_uint128 carry = FStar_UInt128_shift_right(FStar_UInt128_add_mod(z80, c7), 56U);
  uint64_t
  t = FStar_UInt128_uint128_to_uint64(FStar_UInt128_add_mod(z80, c7)) & 0xffffffffffffffULL;
  FStar_UInt128_uint128 c8 = carry;
  uint64_t t8 = t;
  uint64_t t9 = FStar_UInt128_uint128_to_uint64(c8);
  uint64_t z0 = t0;
  uint64_t z1 = t1;
  uint64_t z2 = t2;
  uint64_t z3 = t3;
  uint64_t z4 = t4;
  uint64_t z5 = t5;
  uint64_t z6 = t6;
  uint64_t z7 = t7;
  uint64_t z8 = t8;
  uint64_t z9 = t9;
  tmp[0U] = z0;
  tmp[1U] = z1;
  tmp[2U] = z2;
  tmp[3U] = z3;
  tmp[4U] = z4;
  tmp[5U] = z5;
  tmp[6U] = z6;
  tmp[7U] = z7;
  tmp[8U] = z8;
  tmp[9U] = z9;
  barrett_reduction(out, tmp);
}

static inline void add_modq(uint64_t *out, uint64_t *x, uint64_t *y)
{
  uint64_t x0 = x[0U];
  uint64_t x1 = x[1U];
  uint64_t x2 = x[2U];
  uint64_t x3 = x[3U];
  uint64_t x4 = x[4U];
  uint64_t y0 = y[0U];
  uint64_t y1 = y[1U];
  uint64_t y2 = y[2U];
  uint64_t y3 = y[3U];
  uint64_t y4 = y[4U];
  uint64_t carry0 = (x0 + y0) >> 56U;
  uint64_t t0 = (x0 + y0) & 0xffffffffffffffULL;
  uint64_t t00 = t0;
  uint64_t c0 = carry0;
  uint64_t carry1 = (x1 + y1 + c0) >> 56U;
  uint64_t t1 = (x1 + y1 + c0) & 0xffffffffffffffULL;
  uint64_t t10 = t1;
  uint64_t c1 = carry1;
  uint64_t carry2 = (x2 + y2 + c1) >> 56U;
  uint64_t t2 = (x2 + y2 + c1) & 0xffffffffffffffULL;
  uint64_t t20 = t2;
  uint64_t c2 = carry2;
  uint64_t carry = (x3 + y3 + c2) >> 56U;
  uint64_t t3 = (x3 + y3 + c2) & 0xffffffffffffffULL;
  uint64_t t30 = t3;
  uint64_t c3 = carry;
  uint64_t t4 = x4 + y4 + c3;
  uint64_t m0 = 0x12631a5cf5d3edULL;
  uint64_t m1 = 0xf9dea2f79cd658ULL;
  uint64_t m2 = 0x000000000014deULL;
  uint64_t m3 = 0x00000000000000ULL;
  uint64_t m4 = 0x00000010000000ULL;
  uint64_t y01 = m0;
  uint64_t y11 = m1;
  uint64_t y21 = m2;
  uint64_t y31 = m3;
  uint64_t y41 = m4;
  uint64_t b5 = (t00 - y01) >> 63U;
  uint64_t t5 = (b5 << 56U) + t00 - y01;
  uint64_t b0 = b5;
  uint64_t t01 = t5;
  uint64_t b6 = (t10 - (y11 + b0)) >> 63U;
  uint64_t t6 = (b6 << 56U) + t10 - (y11 + b0);
  uint64_t b1 = b6;
  uint64_t t11 = t6;
  uint64_t b7 = (t20 - (y21 + b1)) >> 63U;
  uint64_t t7 = (b7 << 56U) + t20 - (y21 + b1);
  uint64_t b2 = b7;
  uint64_t t21 = t7;
  uint64_t b8 = (t30 - (y31 + b2)) >> 63U;
  uint64_t t8 = (b8 << 56U) + t30 - (y31 + b2);
  uint64_t b3 = b8;
  uint64_t t31 = t8;
  uint64_t b = (t4 - (y41 + b3)) >> 63U;
  uint64_t t = (b << 56U) + t4 - (y41 + b3);
  uint64_t b4 = b;
  uint64_t t41 = t;
  uint64_t mask = b4 - 1ULL;
  uint64_t z00 = t00 ^ (mask & (t00 ^ t01));
  uint64_t z10 = t10 ^ (mask & (t10 ^ t11));
  uint64_t z20 = t20 ^ (mask & (t20 ^ t21));
  uint64_t z30 = t30 ^ (mask & (t30 ^ t31));
  uint64_t z40 = t4 ^ (mask & (t4 ^ t41));
  uint64_t z01 = z00;
  uint64_t z11 = z10;
  uint64_t z21 = z20;
  uint64_t z31 = z30;
  uint64_t z41 = z40;
  uint64_t o0 = z01;
  uint64_t o1 = z11;
  uint64_t o2 = z21;
  uint64_t o3 = z31;
  uint64_t o4 = z41;
  uint64_t z0 = o0;
  uint64_t z1 = o1;
  uint64_t z2 = o2;
  uint64_t z3 = o3;
  uint64_t z4 = o4;
  out[0U] = z0;
  out[1U] = z1;
  out[2U] = z2;
  out[3U] = z3;
  out[4U] = z4;
}

static inline bool gte_q(uint64_t *s)
{
  uint64_t s0 = s[0U];
  uint64_t s1 = s[1U];
  uint64_t s2 = s[2U];
  uint64_t s3 = s[3U];
  uint64_t s4 = s[4U];
  if (s4 > 0x00000010000000ULL)
  {
    return true;
  }
  if (s4 < 0x00000010000000ULL)
  {
    return false;
  }
  if (s3 > 0x00000000000000ULL || s2 > 0x000000000014deULL)
  {
    return true;
  }
  if (s2 < 0x000000000014deULL)
  {
    return false;
  }
  if (s1 > 0xf9dea2f79cd658ULL)
  {
    return true;
  }
  if (s1 < 0xf9dea2f79cd658ULL)
  {
    return false;
  }
  return s0 >= 0x12631a5cf5d3edULL;
}

static inline bool eq(uint64_t *a, uint64_t *b)
{
  uint64_t a0 = a[0U];
  uint64_t a1 = a[1U];
  uint64_t a2 = a[2U];
  uint64_t a3 = a[3U];
  uint64_t a4 = a[4U];
  uint64_t b0 = b[0U];
  uint64_t b1 = b[1U];
  uint64_t b2 = b[2U];
  uint64_t b3 = b[3U];
  uint64_t b4 = b[4U];
  return a0 == b0 && a1 == b1 && a2 == b2 && a3 == b3 && a4 == b4;
}

bool Hacl_Impl_Ed25519_PointEqual_point_equal(uint64_t *p, uint64_t *q)
{
  uint64_t tmp[20U] = { 0U };
  uint64_t *pxqz = tmp;
  uint64_t *qxpz = tmp + 5U;
  fmul0(pxqz, p, q + 10U);
  reduce(pxqz);
  fmul0(qxpz, q, p + 10U);
  reduce(qxpz);
  bool b = eq(pxqz, qxpz);
  if (b)
  {
    uint64_t *pyqz = tmp + 10U;
    uint64_t *qypz = tmp + 15U;
    fmul0(pyqz, p + 5U, q + 10U);
    reduce(pyqz);
    fmul0(qypz, q + 5U, p + 10U);
    reduce(qypz);
    return eq(pyqz, qypz);
  }
  return false;
}

void Hacl_Impl_Ed25519_PointNegate_point_negate(uint64_t *p, uint64_t *out)
{
  uint64_t zero[5U] = { 0U };
  zero[0U] = 0ULL;
  zero[1U] = 0ULL;
  zero[2U] = 0ULL;
  zero[3U] = 0ULL;
  zero[4U] = 0ULL;
  uint64_t *x = p;
  uint64_t *y = p + 5U;
  uint64_t *z = p + 10U;
  uint64_t *t = p + 15U;
  uint64_t *x1 = out;
  uint64_t *y1 = out + 5U;
  uint64_t *z1 = out + 10U;
  uint64_t *t1 = out + 15U;
  fdifference(x1, zero, x);
  Hacl_Bignum25519_reduce_513(x1);
  memcpy(y1, y, 5U * sizeof (uint64_t));
  memcpy(z1, z, 5U * sizeof (uint64_t));
  fdifference(t1, zero, t);
  Hacl_Bignum25519_reduce_513(t1);
}

void Hacl_Impl_Ed25519_Ladder_point_mul(uint64_t *out, uint8_t *scalar, uint64_t *q)
{
  uint64_t bscalar[4U] = { 0U };
  KRML_MAYBE_FOR4(i,
    0U,
    4U,
    1U,
    uint64_t *os = bscalar;
    uint8_t *bj = scalar + i * 8U;
    uint64_t u = load64_le(bj);
    uint64_t r = u;
    uint64_t x = r;
    os[i] = x;);
  uint64_t table[320U] = { 0U };
  uint64_t tmp[20U] = { 0U };
  uint64_t *t0 = table;
  uint64_t *t1 = table + 20U;
  Hacl_Impl_Ed25519_PointConstants_make_point_inf(t0);
  memcpy(t1, q, 20U * sizeof (uint64_t));
  KRML_MAYBE_FOR7(i,
    0U,
    7U,
    1U,
    uint64_t *t11 = table + (i + 1U) * 20U;
    Hacl_Impl_Ed25519_PointDouble_point_double(tmp, t11);
    memcpy(table + (2U * i + 2U) * 20U, tmp, 20U * sizeof (uint64_t));
    uint64_t *t2 = table + (2U * i + 2U) * 20U;
    Hacl_Impl_Ed25519_PointAdd_point_add(tmp, q, t2);
    memcpy(table + (2U * i + 3U) * 20U, tmp, 20U * sizeof (uint64_t)););
  Hacl_Impl_Ed25519_PointConstants_make_point_inf(out);
  uint64_t tmp0[20U] = { 0U };
  for (uint32_t i0 = 0U; i0 < 64U; i0++)
  {
    KRML_MAYBE_FOR4(i, 0U, 4U, 1U, Hacl_Impl_Ed25519_PointDouble_point_double(out, out););
    uint32_t k = 256U - 4U * i0 - 4U;
    uint64_t bits_l = Hacl_Bignum_Lib_bn_get_bits_u64(4U, bscalar, k, 4U);
    memcpy(tmp0, (uint64_t *)table, 20U * sizeof (uint64_t));
    KRML_MAYBE_FOR15(i1,
      0U,
      15U,
      1U,
      uint64_t c = FStar_UInt64_eq_mask(bits_l, (uint64_t)(i1 + 1U));
      const uint64_t *res_j = table + (i1 + 1U) * 20U;
      for (uint32_t i = 0U; i < 20U; i++)
      {
        uint64_t *os = tmp0;
        uint64_t x = (c & res_j[i]) | (~c & tmp0[i]);
        os[i] = x;
      });
    Hacl_Impl_Ed25519_PointAdd_point_add(out, out, tmp0);
  }
}

static inline void precomp_get_consttime(const uint64_t *table, uint64_t bits_l, uint64_t *tmp)
{
  memcpy(tmp, (uint64_t *)table, 20U * sizeof (uint64_t));
  KRML_MAYBE_FOR15(i0,
    0U,
    15U,
    1U,
    uint64_t c = FStar_UInt64_eq_mask(bits_l, (uint64_t)(i0 + 1U));
    const uint64_t *res_j = table + (i0 + 1U) * 20U;
    for (uint32_t i = 0U; i < 20U; i++)
    {
      uint64_t *os = tmp;
      uint64_t x = (c & res_j[i]) | (~c & tmp[i]);
      os[i] = x;
    });
}

static inline void point_mul_g(uint64_t *out, uint8_t *scalar)
{
  uint64_t bscalar[4U] = { 0U };
  KRML_MAYBE_FOR4(i,
    0U,
    4U,
    1U,
    uint64_t *os = bscalar;
    uint8_t *bj = scalar + i * 8U;
    uint64_t u = load64_le(bj);
    uint64_t r = u;
    uint64_t x = r;
    os[i] = x;);
  uint64_t q1[20U] = { 0U };
  uint64_t *gx = q1;
  uint64_t *gy = q1 + 5U;
  uint64_t *gz = q1 + 10U;
  uint64_t *gt = q1 + 15U;
  gx[0U] = 0x00062d608f25d51aULL;
  gx[1U] = 0x000412a4b4f6592aULL;
  gx[2U] = 0x00075b7171a4b31dULL;
  gx[3U] = 0x0001ff60527118feULL;
  gx[4U] = 0x000216936d3cd6e5ULL;
  gy[0U] = 0x0006666666666658ULL;
  gy[1U] = 0x0004ccccccccccccULL;
  gy[2U] = 0x0001999999999999ULL;
  gy[3U] = 0x0003333333333333ULL;
  gy[4U] = 0x0006666666666666ULL;
  gz[0U] = 1ULL;
  gz[1U] = 0ULL;
  gz[2U] = 0ULL;
  gz[3U] = 0ULL;
  gz[4U] = 0ULL;
  gt[0U] = 0x00068ab3a5b7dda3ULL;
  gt[1U] = 0x00000eea2a5eadbbULL;
  gt[2U] = 0x0002af8df483c27eULL;
  gt[3U] = 0x000332b375274732ULL;
  gt[4U] = 0x00067875f0fd78b7ULL;
  uint64_t
  q2[20U] =
    {
      13559344787725ULL, 2051621493703448ULL, 1947659315640708ULL, 626856790370168ULL,
      1592804284034836ULL, 1781728767459187ULL, 278818420518009ULL, 2038030359908351ULL,
      910625973862690ULL, 471887343142239ULL, 1298543306606048ULL, 794147365642417ULL,
      129968992326749ULL, 523140861678572ULL, 1166419653909231ULL, 2009637196928390ULL,
      1288020222395193ULL, 1007046974985829ULL, 208981102651386ULL, 2074009315253380ULL
    };
  uint64_t
  q3[20U] =
    {
      557549315715710ULL, 196756086293855ULL, 846062225082495ULL, 1865068224838092ULL,
      991112090754908ULL, 522916421512828ULL, 2098523346722375ULL, 1135633221747012ULL,
      858420432114866ULL, 186358544306082ULL, 1044420411868480ULL, 2080052304349321ULL,
      557301814716724ULL, 1305130257814057ULL, 2126012765451197ULL, 1441004402875101ULL,
      353948968859203ULL, 470765987164835ULL, 1507675957683570ULL, 1086650358745097ULL
    };
  uint64_t
  q4[20U] =
    {
      1129953239743101ULL, 1240339163956160ULL, 61002583352401ULL, 2017604552196030ULL,
      1576867829229863ULL, 1508654942849389ULL, 270111619664077ULL, 1253097517254054ULL,
      721798270973250ULL, 161923365415298ULL, 828530877526011ULL, 1494851059386763ULL,
      662034171193976ULL, 1315349646974670ULL, 2199229517308806ULL, 497078277852673ULL,
      1310507715989956ULL, 1881315714002105ULL, 2214039404983803ULL, 1331036420272667ULL
    };
  uint64_t *r1 = bscalar;
  uint64_t *r2 = bscalar + 1U;
  uint64_t *r3 = bscalar + 2U;
  uint64_t *r4 = bscalar + 3U;
  Hacl_Impl_Ed25519_PointConstants_make_point_inf(out);
  uint64_t tmp[20U] = { 0U };
  KRML_MAYBE_FOR16(i,
    0U,
    16U,
    1U,
    KRML_MAYBE_FOR4(i0, 0U, 4U, 1U, Hacl_Impl_Ed25519_PointDouble_point_double(out, out););
    uint32_t k = 64U - 4U * i - 4U;
    uint64_t bits_l = Hacl_Bignum_Lib_bn_get_bits_u64(1U, r4, k, 4U);
    precomp_get_consttime(Hacl_Ed25519_PrecompTable_precomp_g_pow2_192_table_w4, bits_l, tmp);
    Hacl_Impl_Ed25519_PointAdd_point_add(out, out, tmp);
    uint32_t k0 = 64U - 4U * i - 4U;
    uint64_t bits_l0 = Hacl_Bignum_Lib_bn_get_bits_u64(1U, r3, k0, 4U);
    precomp_get_consttime(Hacl_Ed25519_PrecompTable_precomp_g_pow2_128_table_w4, bits_l0, tmp);
    Hacl_Impl_Ed25519_PointAdd_point_add(out, out, tmp);
    uint32_t k1 = 64U - 4U * i - 4U;
    uint64_t bits_l1 = Hacl_Bignum_Lib_bn_get_bits_u64(1U, r2, k1, 4U);
    precomp_get_consttime(Hacl_Ed25519_PrecompTable_precomp_g_pow2_64_table_w4, bits_l1, tmp);
    Hacl_Impl_Ed25519_PointAdd_point_add(out, out, tmp);
    uint32_t k2 = 64U - 4U * i - 4U;
    uint64_t bits_l2 = Hacl_Bignum_Lib_bn_get_bits_u64(1U, r1, k2, 4U);
    precomp_get_consttime(Hacl_Ed25519_PrecompTable_precomp_basepoint_table_w4, bits_l2, tmp);
    Hacl_Impl_Ed25519_PointAdd_point_add(out, out, tmp););
  KRML_MAYBE_UNUSED_VAR(q2);
  KRML_MAYBE_UNUSED_VAR(q3);
  KRML_MAYBE_UNUSED_VAR(q4);
}

static inline void
point_mul_g_double_vartime(uint64_t *out, uint8_t *scalar1, uint8_t *scalar2, uint64_t *q2)
{
  uint64_t tmp[28U] = { 0U };
  uint64_t *g = tmp;
  uint64_t *bscalar1 = tmp + 20U;
  uint64_t *bscalar2 = tmp + 24U;
  uint64_t *gx = g;
  uint64_t *gy = g + 5U;
  uint64_t *gz = g + 10U;
  uint64_t *gt = g + 15U;
  gx[0U] = 0x00062d608f25d51aULL;
  gx[1U] = 0x000412a4b4f6592aULL;
  gx[2U] = 0x00075b7171a4b31dULL;
  gx[3U] = 0x0001ff60527118feULL;
  gx[4U] = 0x000216936d3cd6e5ULL;
  gy[0U] = 0x0006666666666658ULL;
  gy[1U] = 0x0004ccccccccccccULL;
  gy[2U] = 0x0001999999999999ULL;
  gy[3U] = 0x0003333333333333ULL;
  gy[4U] = 0x0006666666666666ULL;
  gz[0U] = 1ULL;
  gz[1U] = 0ULL;
  gz[2U] = 0ULL;
  gz[3U] = 0ULL;
  gz[4U] = 0ULL;
  gt[0U] = 0x00068ab3a5b7dda3ULL;
  gt[1U] = 0x00000eea2a5eadbbULL;
  gt[2U] = 0x0002af8df483c27eULL;
  gt[3U] = 0x000332b375274732ULL;
  gt[4U] = 0x00067875f0fd78b7ULL;
  KRML_MAYBE_FOR4(i,
    0U,
    4U,
    1U,
    uint64_t *os = bscalar1;
    uint8_t *bj = scalar1 + i * 8U;
    uint64_t u = load64_le(bj);
    uint64_t r = u;
    uint64_t x = r;
    os[i] = x;);
  KRML_MAYBE_FOR4(i,
    0U,
    4U,
    1U,
    uint64_t *os = bscalar2;
    uint8_t *bj = scalar2 + i * 8U;
    uint64_t u = load64_le(bj);
    uint64_t r = u;
    uint64_t x = r;
    os[i] = x;);
  uint64_t table2[640U] = { 0U };
  uint64_t tmp1[20U] = { 0U };
  uint64_t *t0 = table2;
  uint64_t *t1 = table2 + 20U;
  Hacl_Impl_Ed25519_PointConstants_make_point_inf(t0);
  memcpy(t1, q2, 20U * sizeof (uint64_t));
  KRML_MAYBE_FOR15(i,
    0U,
    15U,
    1U,
    uint64_t *t11 = table2 + (i + 1U) * 20U;
    Hacl_Impl_Ed25519_PointDouble_point_double(tmp1, t11);
    memcpy(table2 + (2U * i + 2U) * 20U, tmp1, 20U * sizeof (uint64_t));
    uint64_t *t2 = table2 + (2U * i + 2U) * 20U;
    Hacl_Impl_Ed25519_PointAdd_point_add(tmp1, q2, t2);
    memcpy(table2 + (2U * i + 3U) * 20U, tmp1, 20U * sizeof (uint64_t)););
  uint64_t tmp10[20U] = { 0U };
  uint32_t i0 = 255U;
  uint64_t bits_c = Hacl_Bignum_Lib_bn_get_bits_u64(4U, bscalar1, i0, 5U);
  uint32_t bits_l32 = (uint32_t)bits_c;
  const
  uint64_t
  *a_bits_l = Hacl_Ed25519_PrecompTable_precomp_basepoint_table_w5 + bits_l32 * 20U;
  memcpy(out, (uint64_t *)a_bits_l, 20U * sizeof (uint64_t));
  uint32_t i1 = 255U;
  uint64_t bits_c0 = Hacl_Bignum_Lib_bn_get_bits_u64(4U, bscalar2, i1, 5U);
  uint32_t bits_l320 = (uint32_t)bits_c0;
  const uint64_t *a_bits_l0 = table2 + bits_l320 * 20U;
  memcpy(tmp10, (uint64_t *)a_bits_l0, 20U * sizeof (uint64_t));
  Hacl_Impl_Ed25519_PointAdd_point_add(out, out, tmp10);
  uint64_t tmp11[20U] = { 0U };
  for (uint32_t i = 0U; i < 51U; i++)
  {
    KRML_MAYBE_FOR5(i2, 0U, 5U, 1U, Hacl_Impl_Ed25519_PointDouble_point_double(out, out););
    uint32_t k = 255U - 5U * i - 5U;
    uint64_t bits_l = Hacl_Bignum_Lib_bn_get_bits_u64(4U, bscalar2, k, 5U);
    uint32_t bits_l321 = (uint32_t)bits_l;
    const uint64_t *a_bits_l1 = table2 + bits_l321 * 20U;
    memcpy(tmp11, (uint64_t *)a_bits_l1, 20U * sizeof (uint64_t));
    Hacl_Impl_Ed25519_PointAdd_point_add(out, out, tmp11);
    uint32_t k0 = 255U - 5U * i - 5U;
    uint64_t bits_l0 = Hacl_Bignum_Lib_bn_get_bits_u64(4U, bscalar1, k0, 5U);
    uint32_t bits_l322 = (uint32_t)bits_l0;
    const
    uint64_t
    *a_bits_l2 = Hacl_Ed25519_PrecompTable_precomp_basepoint_table_w5 + bits_l322 * 20U;
    memcpy(tmp11, (uint64_t *)a_bits_l2, 20U * sizeof (uint64_t));
    Hacl_Impl_Ed25519_PointAdd_point_add(out, out, tmp11);
  }
}

static inline void
point_negate_mul_double_g_vartime(
  uint64_t *out,
  uint8_t *scalar1,
  uint8_t *scalar2,
  uint64_t *q2
)
{
  uint64_t q2_neg[20U] = { 0U };
  Hacl_Impl_Ed25519_PointNegate_point_negate(q2, q2_neg);
  point_mul_g_double_vartime(out, scalar1, scalar2, q2_neg);
}

static inline void store_56(uint8_t *out, uint64_t *b)
{
  uint64_t b0 = b[0U];
  uint64_t b1 = b[1U];
  uint64_t b2 = b[2U];
  uint64_t b3 = b[3U];
  uint64_t b4 = b[4U];
  uint32_t b4_ = (uint32_t)b4;
  uint8_t *b8 = out;
  store64_le(b8, b0);
  uint8_t *b80 = out + 7U;
  store64_le(b80, b1);
  uint8_t *b81 = out + 14U;
  store64_le(b81, b2);
  uint8_t *b82 = out + 21U;
  store64_le(b82, b3);
  store32_le(out + 28U, b4_);
}

static inline void load_64_bytes(uint64_t *out, uint8_t *b)
{
  uint8_t *b80 = b;
  uint64_t u = load64_le(b80);
  uint64_t z = u;
  uint64_t b0 = z & 0xffffffffffffffULL;
  uint8_t *b81 = b + 7U;
  uint64_t u0 = load64_le(b81);
  uint64_t z0 = u0;
  uint64_t b1 = z0 & 0xffffffffffffffULL;
  uint8_t *b82 = b + 14U;
  uint64_t u1 = load64_le(b82);
  uint64_t z1 = u1;
  uint64_t b2 = z1 & 0xffffffffffffffULL;
  uint8_t *b83 = b + 21U;
  uint64_t u2 = load64_le(b83);
  uint64_t z2 = u2;
  uint64_t b3 = z2 & 0xffffffffffffffULL;
  uint8_t *b84 = b + 28U;
  uint64_t u3 = load64_le(b84);
  uint64_t z3 = u3;
  uint64_t b4 = z3 & 0xffffffffffffffULL;
  uint8_t *b85 = b + 35U;
  uint64_t u4 = load64_le(b85);
  uint64_t z4 = u4;
  uint64_t b5 = z4 & 0xffffffffffffffULL;
  uint8_t *b86 = b + 42U;
  uint64_t u5 = load64_le(b86);
  uint64_t z5 = u5;
  uint64_t b6 = z5 & 0xffffffffffffffULL;
  uint8_t *b87 = b + 49U;
  uint64_t u6 = load64_le(b87);
  uint64_t z6 = u6;
  uint64_t b7 = z6 & 0xffffffffffffffULL;
  uint8_t *b8 = b + 56U;
  uint64_t u7 = load64_le(b8);
  uint64_t z7 = u7;
  uint64_t b88 = z7 & 0xffffffffffffffULL;
  uint8_t b63 = b[63U];
  uint64_t b9 = (uint64_t)b63;
  out[0U] = b0;
  out[1U] = b1;
  out[2U] = b2;
  out[3U] = b3;
  out[4U] = b4;
  out[5U] = b5;
  out[6U] = b6;
  out[7U] = b7;
  out[8U] = b88;
  out[9U] = b9;
}

static inline void load_32_bytes(uint64_t *out, uint8_t *b)
{
  uint8_t *b80 = b;
  uint64_t u0 = load64_le(b80);
  uint64_t z = u0;
  uint64_t b0 = z & 0xffffffffffffffULL;
  uint8_t *b81 = b + 7U;
  uint64_t u1 = load64_le(b81);
  uint64_t z0 = u1;
  uint64_t b1 = z0 & 0xffffffffffffffULL;
  uint8_t *b82 = b + 14U;
  uint64_t u2 = load64_le(b82);
  uint64_t z1 = u2;
  uint64_t b2 = z1 & 0xffffffffffffffULL;
  uint8_t *b8 = b + 21U;
  uint64_t u3 = load64_le(b8);
  uint64_t z2 = u3;
  uint64_t b3 = z2 & 0xffffffffffffffULL;
  uint32_t u = load32_le(b + 28U);
  uint32_t b4 = u;
  uint64_t b41 = (uint64_t)b4;
  out[0U] = b0;
  out[1U] = b1;
  out[2U] = b2;
  out[3U] = b3;
  out[4U] = b41;
}

static inline void sha512_pre_msg(uint8_t *hash, uint8_t *prefix, uint32_t len, uint8_t *input)
{
  uint8_t buf[128U] = { 0U };
  uint64_t block_state[8U] = { 0U };
  Hacl_Streaming_MD_state_64
  s = { .block_state = block_state, .buf = buf, .total_len = (uint64_t)0U };
  Hacl_Streaming_MD_state_64 p = s;
  Hacl_Hash_SHA2_sha512_init(block_state);
  Hacl_Streaming_MD_state_64 *st = &p;
  Hacl_Streaming_Types_error_code err0 = Hacl_Hash_SHA2_update_512(st, prefix, 32U);
  Hacl_Streaming_Types_error_code err1 = Hacl_Hash_SHA2_update_512(st, input, len);
  KRML_MAYBE_UNUSED_VAR(err0);
  KRML_MAYBE_UNUSED_VAR(err1);
  Hacl_Hash_SHA2_digest_512(st, hash);
}

static inline void
sha512_pre_pre2_msg(
  uint8_t *hash,
  uint8_t *prefix,
  uint8_t *prefix2,
  uint32_t len,
  uint8_t *input
)
{
  uint8_t buf[128U] = { 0U };
  uint64_t block_state[8U] = { 0U };
  Hacl_Streaming_MD_state_64
  s = { .block_state = block_state, .buf = buf, .total_len = (uint64_t)0U };
  Hacl_Streaming_MD_state_64 p = s;
  Hacl_Hash_SHA2_sha512_init(block_state);
  Hacl_Streaming_MD_state_64 *st = &p;
  Hacl_Streaming_Types_error_code err0 = Hacl_Hash_SHA2_update_512(st, prefix, 32U);
  Hacl_Streaming_Types_error_code err1 = Hacl_Hash_SHA2_update_512(st, prefix2, 32U);
  Hacl_Streaming_Types_error_code err2 = Hacl_Hash_SHA2_update_512(st, input, len);
  KRML_MAYBE_UNUSED_VAR(err0);
  KRML_MAYBE_UNUSED_VAR(err1);
  KRML_MAYBE_UNUSED_VAR(err2);
  Hacl_Hash_SHA2_digest_512(st, hash);
}

static inline void
sha512_modq_pre(uint64_t *out, uint8_t *prefix, uint32_t len, uint8_t *input)
{
  uint64_t tmp[10U] = { 0U };
  uint8_t hash[64U] = { 0U };
  sha512_pre_msg(hash, prefix, len, input);
  load_64_bytes(tmp, hash);
  barrett_reduction(out, tmp);
}

static inline void
sha512_modq_pre_pre2(
  uint64_t *out,
  uint8_t *prefix,
  uint8_t *prefix2,
  uint32_t len,
  uint8_t *input
)
{
  uint64_t tmp[10U] = { 0U };
  uint8_t hash[64U] = { 0U };
  sha512_pre_pre2_msg(hash, prefix, prefix2, len, input);
  load_64_bytes(tmp, hash);
  barrett_reduction(out, tmp);
}

static inline void point_mul_g_compress(uint8_t *out, uint8_t *s)
{
  uint64_t tmp[20U] = { 0U };
  point_mul_g(tmp, s);
  Hacl_Impl_Ed25519_PointCompress_point_compress(out, tmp);
}

static inline void secret_expand(uint8_t *expanded, uint8_t *secret)
{
  Hacl_Hash_SHA2_hash_512(expanded, secret, 32U);
  uint8_t *h_low = expanded;
  uint8_t h_low0 = h_low[0U];
  uint8_t h_low31 = h_low[31U];
  h_low[0U] = (uint32_t)h_low0 & 0xf8U;
  h_low[31U] = ((uint32_t)h_low31 & 127U) | 64U;
}

/********************************************************************************
  Verified C library for EdDSA signing and verification on the edwards25519 curve.
********************************************************************************/


/**
Compute the public key from the private key.

  @param[out] public_key Points to 32 bytes of valid memory, i.e., `uint8_t[32]`. Must not overlap the memory location of `private_key`.
  @param[in] private_key Points to 32 bytes of valid memory containing the private key, i.e., `uint8_t[32]`.
*/
void Hacl_Ed25519_secret_to_public(uint8_t *public_key, uint8_t *private_key)
{
  uint8_t expanded_secret[64U] = { 0U };
  secret_expand(expanded_secret, private_key);
  uint8_t *a = expanded_secret;
  point_mul_g_compress(public_key, a);
}

/**
Compute the expanded keys for an Ed25519 signature.

  @param[out] expanded_keys Points to 96 bytes of valid memory, i.e., `uint8_t[96]`. Must not overlap the memory location of `private_key`.
  @param[in] private_key Points to 32 bytes of valid memory containing the private key, i.e., `uint8_t[32]`.

  If one needs to sign several messages under the same private key, it is more efficient
  to call `expand_keys` only once and `sign_expanded` multiple times, for each message.
*/
void Hacl_Ed25519_expand_keys(uint8_t *expanded_keys, uint8_t *private_key)
{
  uint8_t *public_key = expanded_keys;
  uint8_t *s_prefix = expanded_keys + 32U;
  uint8_t *s = expanded_keys + 32U;
  secret_expand(s_prefix, private_key);
  point_mul_g_compress(public_key, s);
}

/**
Create an Ed25519 signature with the (precomputed) expanded keys.

  @param[out] signature Points to 64 bytes of valid memory, i.e., `uint8_t[64]`. Must not overlap the memory locations of `expanded_keys` nor `msg`.
  @param[in] expanded_keys Points to 96 bytes of valid memory, i.e., `uint8_t[96]`, containing the expanded keys obtained by invoking `expand_keys`.
  @param[in] msg_len Length of `msg`.
  @param[in] msg Points to `msg_len` bytes of valid memory containing the message, i.e., `uint8_t[msg_len]`.

  If one needs to sign several messages under the same private key, it is more efficient
  to call `expand_keys` only once and `sign_expanded` multiple times, for each message.
*/
void
Hacl_Ed25519_sign_expanded(
  uint8_t *signature,
  uint8_t *expanded_keys,
  uint32_t msg_len,
  uint8_t *msg
)
{
  uint8_t *rs = signature;
  uint8_t *ss = signature + 32U;
  uint64_t rq[5U] = { 0U };
  uint64_t hq[5U] = { 0U };
  uint8_t rb[32U] = { 0U };
  uint8_t *public_key = expanded_keys;
  uint8_t *s = expanded_keys + 32U;
  uint8_t *prefix = expanded_keys + 64U;
  sha512_modq_pre(rq, prefix, msg_len, msg);
  store_56(rb, rq);
  point_mul_g_compress(rs, rb);
  sha512_modq_pre_pre2(hq, rs, public_key, msg_len, msg);
  uint64_t aq[5U] = { 0U };
  load_32_bytes(aq, s);
  mul_modq(aq, hq, aq);
  add_modq(aq, rq, aq);
  store_56(ss, aq);
}

/**
Create an Ed25519 signature.

  @param[out] signature Points to 64 bytes of valid memory, i.e., `uint8_t[64]`. Must not overlap the memory locations of `private_key` nor `msg`.
  @param[in] private_key Points to 32 bytes of valid memory containing the private key, i.e., `uint8_t[32]`.
  @param[in] msg_len Length of `msg`.
  @param[in] msg Points to `msg_len` bytes of valid memory containing the message, i.e., `uint8_t[msg_len]`.

  The function first calls `expand_keys` and then invokes `sign_expanded`.

  If one needs to sign several messages under the same private key, it is more efficient
  to call `expand_keys` only once and `sign_expanded` multiple times, for each message.
*/
void
Hacl_Ed25519_sign(uint8_t *signature, uint8_t *private_key, uint32_t msg_len, uint8_t *msg)
{
  uint8_t expanded_keys[96U] = { 0U };
  Hacl_Ed25519_expand_keys(expanded_keys, private_key);
  Hacl_Ed25519_sign_expanded(signature, expanded_keys, msg_len, msg);
}

/**
Verify an Ed25519 signature.

  @param public_key Points to 32 bytes of valid memory containing the public key, i.e., `uint8_t[32]`.
  @param msg_len Length of `msg`.
  @param msg Points to `msg_len` bytes of valid memory containing the message, i.e., `uint8_t[msg_len]`.
  @param signature Points to 64 bytes of valid memory containing the signature, i.e., `uint8_t[64]`.

  @return Returns `true` if the signature is valid and `false` otherwise.
*/
bool
Hacl_Ed25519_verify(uint8_t *public_key, uint32_t msg_len, uint8_t *msg, uint8_t *signature)
{
  uint64_t a_[20U] = { 0U };
  bool b = Hacl_Impl_Ed25519_PointDecompress_point_decompress(a_, public_key);
  if (b)
  {
    uint64_t r_[20U] = { 0U };
    uint8_t *rs = signature;
    bool b_ = Hacl_Impl_Ed25519_PointDecompress_point_decompress(r_, rs);
    if (b_)
    {
      uint8_t hb[32U] = { 0U };
      uint8_t *rs1 = signature;
      uint8_t *sb = signature + 32U;
      uint64_t tmp[5U] = { 0U };
      load_32_bytes(tmp, sb);
      bool b1 = gte_q(tmp);
      bool b10 = b1;
      if (b10)
      {
        return false;
      }
      uint64_t tmp0[5U] = { 0U };
      sha512_modq_pre_pre2(tmp0, rs1, public_key, msg_len, msg);
      store_56(hb, tmp0);
      uint64_t exp_d[20U] = { 0U };
      point_negate_mul_double_g_vartime(exp_d, sb, hb, a_);
      bool b2 = Hacl_Impl_Ed25519_PointEqual_point_equal(exp_d, r_);
      return b2;
    }
    return false;
  }
  return false;
}

// OMEMO Additions

void Hacl_Ed25519_sign_modified(
  uint8_t *signature,
  uint8_t *public_key,
  uint8_t *s,
  uint8_t *msg,
  uint32_t msg_len
)
{
  signature[0] = 0xfe;
  memset(signature + 1, 0xff, 31);
  memcpy(signature + 32, s, 32);
  uint8_t *rs = signature;
  uint8_t *ss = signature + 32U;
  uint64_t rq[5U] = { 0U };
  uint64_t hq[5U] = { 0U };
  uint8_t rb[32U] = { 0U };
  sha512_modq_pre_pre2(rq, signature, signature + 32, msg_len + 64, msg);
  store_56(rb, rq);
  point_mul_g_compress(rs, rb);
  sha512_modq_pre_pre2(hq, rs, public_key, msg_len, msg);
  uint64_t aq[5U] = { 0U };
  load_32_bytes(aq, s);
  mul_modq(aq, hq, aq);
  add_modq(aq, rq, aq);
  store_56(ss, aq);
}

void Hacl_Ed25519_pub_from_Curve25519_priv(
    uint8_t *pub,
    uint8_t *sec) {
  point_mul_g_compress(pub, sec);
}

void Hacl_Ed25519_seed_to_pub_priv(uint8_t *public_key, uint8_t *private_key, uint8_t *seed)
{
  uint8_t expanded_secret[64U] = { 0U };
  secret_expand(expanded_secret, seed);
  uint8_t *a = expanded_secret;
  point_mul_g_compress(public_key, a);
  memcpy(private_key, a, 32);
}

bool Hacl_Ed25519_pub_to_Curve25519_pub(
    uint8_t *m,
    uint8_t *e) {
  uint64_t o[20], um[5], yplus[5], yminus[5], one[5] = {1,0,0,0,0};
  if (!Hacl_Impl_Ed25519_PointDecompress_point_decompress(o, e))
    return false;
  fdifference(yplus, one, o+5);
  Hacl_Bignum25519_inverse(yminus, yplus);
  fsum(yplus, one, o+5);
  fmul0(um, yplus, yminus);
  Hacl_Bignum25519_store_51(m, um);
  return true;
}

void Hacl_Curve25519_pub_to_Ed25519_pub(
    uint8_t *e,
    uint8_t *m) {
  uint64_t ey[5], mx[5], n[5], d[5], one[5] = {1,0,0,0,0};
  Hacl_Bignum25519_load_51(mx, m);
  fsum(n, mx, one);
  Hacl_Bignum25519_inverse(d, n);
  fdifference(n, mx, one);
  fmul0(ey, n, d);
  Hacl_Bignum25519_store_51(e, ey);
}
