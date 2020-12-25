#ifndef MN_LOG_H
#define MN_LOG_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mn/thread.h"

#if _MSC_VER
#    define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#    define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define MN_LOG_FILENAME "MonstersLink.log"
//#define MN_LOG_DISABLE
#define MN_LOG_USE_COLOR
#define MN_THREAD_ID mn_thread_id()

typedef void (*mn_log_lock_fn)(void *udata, int lock);

typedef enum mn_log_level_e {
    MN_LOG_LEVEL_TRACE,
    MN_LOG_LEVEL_DEBUG,
    MN_LOG_LEVEL_INFO,
    MN_LOG_LEVEL_WARN,
    MN_LOG_LEVEL_ERROR,
    MN_LOG_LEVEL_FATAL,
} mn_log_level_t;

#ifdef MN_LOG_DISABLE
#    define mn_log(...)
#    define mn_log_trace(...)
#    define mn_log_debug(...)
#    define mn_log_info(...)
#    define mn_log_warn(...)
#    define mn_log_error(...)
#    define mn_log_fatal(...)

#    define mn_log_enable(b)
#    define mn_log_disable(b)
#    define mn_log_level(l)
#    define mn_log_file(f)
#else
#    define mn_log(...) mn_log_log(MN_LOG_LEVEL_DEBUG, __FUNCTION__, __FILENAME__, __LINE__, MN_THREAD_ID, __VA_ARGS__)
#    define mn_log_trace(...) mn_log_log(MN_LOG_LEVEL_TRACE, __FUNCTION__, __FILENAME__, __LINE__, MN_THREAD_ID, __VA_ARGS__)
#    define mn_log_debug(...) mn_log_log(MN_LOG_LEVEL_DEBUG, __FUNCTION__, __FILENAME__, __LINE__, MN_THREAD_ID, __VA_ARGS__)
#    define mn_log_info(...) mn_log_log(MN_LOG_LEVEL_INFO, __FUNCTION__, __FILENAME__, __LINE__, MN_THREAD_ID, __VA_ARGS__)
#    define mn_log_warn(...) mn_log_log(MN_LOG_LEVEL_WARN, __FUNCTION__, __FILENAME__, __LINE__, MN_THREAD_ID, __VA_ARGS__)
#    define mn_log_warning(...) mn_log_log(MN_LOG_LEVEL_WARN, __FUNCTION__, __FILENAME__, __LINE__, MN_THREAD_ID, __VA_ARGS__)
#    define mn_log_error(...) mn_log_log(MN_LOG_LEVEL_ERROR, __FUNCTION__, __FILENAME__, __LINE__, MN_THREAD_ID, __VA_ARGS__)
#    define mn_log_fatal(...) mn_log_log(MN_LOG_LEVEL_FATAL, __FUNCTION__, __FILENAME__, __LINE__, MN_THREAD_ID, __VA_ARGS__)

#    define mn_log_enable(b) mn_log_set_quiet(0);
#    define mn_log_disable(b) mn_log_set_quiet(1);
#    define mn_log_level(l) mn_log_set_level(l);
#    define mn_log_file(f) mn_log_set_fp(f);
#endif

//#ifndef MN_BUILD_DEBUG
//#	undef mn_log_trace
//#	define mn_log_trace(...) ((void)0)
//#endif

void mn_log_setup(void);
void mn_log_cleanup(void);

// TODO: udata and locking should probably be private
void mn_log_color(int enable);
void mn_log_set_udata(void *udata);
void mn_log_set_lock(mn_log_lock_fn fn);
void mn_log_set_fp(FILE *fp);
void mn_log_set_level(int level);
void mn_log_set_quiet(int enable);

void mn_log_log(int level, const char *func, const char *file, int line, uint64_t thread_id, const char *fmt, ...);

#endif
