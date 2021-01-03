#ifndef RAWR_PLATFORM_H
#define RAWR_PLATFORM_H

/*
 * These defines can force compliance with exotic platforms and C
 * implementations. To use them optimally, add them to your project's
 * compiler defines or alternatively, force a platform and add needed
 * defines under the RAWR_PLATFORM_*** define you've forced
 *
 * #define RAWR_SKIP_STATIC_ASSERT
 * #define RAWR_SKIP_GUARDS
 * #define RAWR_C_MISSING_ASSERT
 * #define RAWR_C_MISSING_STDBOOL
 * #define RAWR_C_MISSING_STDINT
 * #define RAWR_FORCE_64
 * #define RAWR_FORCE_32
 * #define RAWR_FORCE_PLATFORM_NONE
 * #define RAWR_FORCE_PLATFORM_PS4
 * #define RAWR_FORCE_PLATFORM_XBONE
 * #define RAWR_FORCE_PLATFORM_SWITCH
 * #define RAWR_FORCE_C_NONE
 */

/* determine platform */
#if defined(RAWR_FORCE_PLATFORM_NONE)
#    define RAWR_PLATFORM_NONE 1
#elif defined(RAWR_FORCE_PLATFORM_PS4)
#    define RAWR_PLATFORM_PS4 1
#elif defined(RAWR_FORCE_PLATFORM_XBONE)
#    define RAWR_PLATFORM_XBONE 1
#elif defined(RAWR_FORCE_PLATFORM_SWITCH)
#    define RAWR_PLATFORM_SWITCH 1
#elif defined(__CYGWIN32__) || defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || defined(WINAPI_FAMILY)
#    define RAWR_PLATFORM_WINDOWS 1
#elif __APPLE__
#    include <TargetConditionals.h>
#    if TARGET_IPHONE_SIMULATOR
#        define RAWR_PLATFORM_IOS 1
#    elif TARGET_OS_IPHONE
#        define RAWR_PLATFORM_IOS 1
#    elif TARGET_OS_MAC
#        define RAWR_PLATFORM_OSX 1
#    else
#    endif
#elif __ANDROID__
#    define RAWR_PLATFORM_ANDROID 1
#elif __linux__
#    define RAWR_PLATFORM_POSIX 1
#elif __unix__
#    define RAWR_PLATFORM_POSIX 1
#elif defined(_POSIX_VERSION)
#    define RAWR_PLATFORM_POSIX 1
#else
#endif

/* determine socket API */
#if RAWR_PLATFORM_WINDOWS || RAWR_PLATFORM_XBONE
#    define RAWR_SOCK_API_WINSOCK 1
#    ifndef WINSOCK_VERSION
#        include <winsock2.h>
#        include <ws2ipdef.h>
#    endif
#    define RAWR_SOCK_TYPE SOCKET
#elif RAWR_PLATFORM_POSIX || RAWR_PLATFORM_OSX || RAWR_PLATFORM_IOS || RAWR_PLATFORM_ANDROID
#    define RAWR_SOCK_API_POSIX 1
#    include <fcntl.h>
#    include <netinet/in.h>
#    include <netinet/udp.h>
#    include <sys/socket.h>
#    include <unistd.h>
#    define RAWR_SOCK_TYPE int
#elif RAWR_PLATFORM_SWITCH
#    define RAWR_SOCK_API_SWITCH 1
#elif RAWR_PLATFORM_PS4
#    define RAWR_SOCK_API_PS4 1
#endif

/* determine C compiler */
#if !defined(RAWR_FORCE_C_NONE) && defined(_MSC_VER)
#    define RAWR_C_MSC 1
#elif !defined(RAWR_FORCE_C_NONE) && defined(__GNUC__)
#    define RAWR_C_GCC 1
#elif !defined(RAWR_FORCE_C_NONE) && defined(__clang__)
#    define RAWR_C_CLANG 1
#else
#    define RAWR_C_NONE 1
#endif

#define RAWR_INLINE_IMPL static inline

#if RAWR_C_MSC
#    if defined(RAWR_C_MISSING_STDBOOL) || defined(RAWR_C_MISSING_STDINT)
#        error "Please file an issue if you are truly missing stdint or stdbool in any MSFT environment: https://github.com/c6burns/socklynx/issues"
#    endif
#    define RAWR_CALL __cdecl
#    ifdef RAWR_EXPORTS
#        define RAWR_API __declspec(dllexport)
#    else
#        define RAWR_API __declspec(dllimport)
#    endif
#else
#    define RAWR_CALL
#    define RAWR_API
#endif

/* 32 or 64 bit compile */
#if !defined(RAWR_FORCE_32) && (defined(RAWR_FORCE_64) || defined(__x86_64__) || defined(_M_AMD64) || defined(__aarch64__) || defined(__ia64__) || defined(__powerpc64__))
#    define RAWR_64 1
#else
#    define RAWR_32 1
#endif

#ifndef RAWR_C_MISSING_STDBOOL
#    include <stdbool.h>
#elif RAWR_C_GCC && !defined(__STRICT_ANSI__)
#    define _Bool bool
#endif

#ifndef RAWR_C_MISSING_STDINT
#    include <stdint.h>
#    include <stdlib.h>
#    ifdef RAWR_PLATFORM_ANDROID
#        include <limits.h>
#    endif
#else

typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
#    if RAWR_64
typedef long int int64_t;
#    else
typedef long long int int64_t;
#    endif

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;

typedef unsigned int uint32_t;

#    if RAWR_64
typedef unsigned long int uint64_t;
#    else
typedef unsigned long long int uint64_t;
#    endif

typedef long int intptr_t;
typedef unsigned long int uintptr_t;

typedef unsigned long int size_t;

#    if RAWR_64 && !defined(__INT64_C)
#        define __INT64_C(c) c##L
#        define __UINT64_C(c) c##UL
#    elif !defined(__INT64_C)
#        define __INT64_C(c) c##LL
#        define __UINT64_C(c) c##ULL
#    endif

#    define INT8_MIN (-128)
#    define INT16_MIN (-32767 - 1)
#    define INT32_MIN (-2147483647 - 1)
#    define INT64_MIN (-__INT64_C(9223372036854775807) - 1)
#    define INT8_MAX (127)
#    define INT16_MAX (32767)
#    define INT32_MAX (2147483647)
#    define INT64_MAX (__INT64_C(9223372036854775807))
#    define UINT8_MAX (255)
#    define UINT16_MAX (65535)
#    define UINT32_MAX (4294967295U)
#    define UINT64_MAX (__UINT64_C(18446744073709551615))

#endif

#ifdef RAWR_SKIP_STATIC_ASSERT
#    define RAWR_STATIC_ASSERT(cond)
#else
#    define RAWR_CONCAT(A, B) A##B
#    define RAWR_STATIC_ASSERT0(cond, msg) typedef char RAWR_CONCAT(static_assertion_, msg)[(!!(cond)) * 2 - 1]
#    define RAWR_STATIC_ASSERT1(cond, line) RAWR_STATIC_ASSERT0(cond, RAWR_CONCAT(at_line_, line))
#    define RAWR_STATIC_ASSERT(cond) RAWR_STATIC_ASSERT1(cond, __LINE__)
#endif

RAWR_STATIC_ASSERT(sizeof(uint64_t) == 8);
RAWR_STATIC_ASSERT(sizeof(uint32_t) == 4);
RAWR_STATIC_ASSERT(sizeof(uint16_t) == 2);
RAWR_STATIC_ASSERT(sizeof(uint8_t) == 1);
RAWR_STATIC_ASSERT(sizeof(int64_t) == 8);
RAWR_STATIC_ASSERT(sizeof(int32_t) == 4);
RAWR_STATIC_ASSERT(sizeof(int16_t) == 2);
RAWR_STATIC_ASSERT(sizeof(int8_t) == 1);
RAWR_STATIC_ASSERT(sizeof(uintptr_t) == sizeof(void *));
RAWR_STATIC_ASSERT(sizeof(intptr_t) == sizeof(void *));
RAWR_STATIC_ASSERT(sizeof(size_t) == sizeof(void *));
RAWR_STATIC_ASSERT(sizeof(char) == 1);

#endif
