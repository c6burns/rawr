#ifndef MN_ERROR_H
#define MN_ERROR_H

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

/* general return status */
#define MN_SUCCESS 0
#define MN_ERROR -1
#define MN_ERROR_AGAIN EAGAIN
#define MN_ERROR_NOMEM ENOMEM
#define MN_ERROR_INVAL EINVAL

/* queue return status */
#define MN_QUEUE_EMPTY -100
#define MN_QUEUE_FULL -101
#define MN_QUEUE_AGAIN -102

/* event return status */
#define MN_EVENT_ERROR -200
#define MN_EVENT_NOCHAN -201
#define MN_EVENT_NOSPAN -202
#define MN_EVENT_SPANREAD -203

/* send / IO write return status */
#define MN_SEND_ERROR -300
#define MN_SEND_NOREQ -301
#define MN_SEND_NOBUF -302
#define MN_SEND_PUSH -303

#define MN_RECV_ERROR -400
#define MN_RECV_EBUF -401
#define MN_RECV_EVTPOP -402
#define MN_RECV_EVTPUSH -403
#define MN_RECV_E2BIG -404

#define MN_CLIENT_ECONN -500

#define MN_STATIC_ASSERT(cond) AWS_STATIC_ASSERT(cond)
#define MN_ASSERT(expr) assert(expr)
#define MN_GUARD(expr) \
    if ((expr)) return MN_ERROR
#define MN_GUARD_LOG(expr, msg) \
    if ((expr)) {               \
        mn_log_error(msg);      \
        return MN_ERROR;        \
    }
#define MN_GUARD_NULL(expr) \
    if (!(expr)) return MN_ERROR
#define MN_GUARD_GOTO(lbl, expr) \
    {                            \
        if ((expr)) goto lbl;    \
    }
#define MN_GUARD_CLEANUP(expr) MN_GUARD_GOTO(cleanup, expr)
#define MN_GUARD_NULL_CLEANUP(expr) MN_GUARD_GOTO(cleanup, !(expr))

#ifdef _WIN32
#    define MN_PLATFORM_WINDOWS 1

#    ifdef _WIN64
#    else
#    endif
#elif __APPLE__
#    include <TargetConditionals.h>
#    if TARGET_IPHONE_SIMULATOR
#        define MN_PLATFORM_IOS 1
#    elif TARGET_OS_IPHONE
#        define MN_PLATFORM_IOS 1
#    elif TARGET_OS_MAC
#        define MN_PLATFORM_OSX 1
#    else
#        error "Unknown Apple platform"
#    endif
#elif __ANDROID__
#    define MN_PLATFORM_ANDROID 1
#elif __linux__
#    define MN_PLATFORM_POSIX 1
#elif __unix__ // all unices not caught above
#    define MN_PLATFORM_POSIX 1
#elif defined(_POSIX_VERSION)
#    define MN_PLATFORM_POSIX 1
#else
#    error "Unknown compiler"
#endif

#ifdef MN_EXPORTING
#    if defined(__CYGWIN32__) || defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || defined(WINAPI_FAMILY)
#        define MN_CONVENTION __cdecl
#        define MN_EXPORT __declspec(dllexport)
#    else
#        define MN_CONVENTION
#        define MN_EXPORT
#    endif
#endif

#endif
