#include "mn/log.h"

#if _MSC_VER
#    include <windows.h>
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "aws/common/condition_variable.h"
#include "aws/common/mutex.h"
#include "aws/common/thread.h"

static const char *level_names[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
};

#ifdef MN_LOG_USE_COLOR
static const char *level_colors[] = {
    "\033[90m",
    "\033[36m",
    "\033[32m",
    "\033[33m",
    "\033[31m",
    "\033[35m",
};
#endif

typedef struct aws_mutex aws_mutex_t;
aws_mutex_t mn_log_mtx;
FILE *mn_log_fp = NULL;
int mn_log_ready = 0;

typedef struct mn_log_s {
    void *udata;
    mn_log_lock_fn lock;
    FILE *fp;
    int level;
    int quiet;
    int color;
} mn_log_t;

static mn_log_t mn_log_ctx;

// private ------------------------------------------------------------------------------------------------------
void mn_log_lock_impl(void *udata, int lock)
{
    aws_mutex_t *mtx = (aws_mutex_t *)udata;
    if (lock)
        aws_mutex_lock(mtx);
    else
        aws_mutex_unlock(mtx);
}

// --------------------------------------------------------------------------------------------------------------
void mn_log_setup(void)
{
#ifndef MN_LOG_DISABLE
    if (mn_log_ready) return;
    mn_log_ready = 1;

    memset(&mn_log_ctx, 0, sizeof(mn_log_ctx));

    mn_log_color(1);

    aws_mutex_init(&mn_log_mtx);
    mn_log_set_udata(&mn_log_mtx);
    mn_log_set_lock(mn_log_lock_impl);

#    ifdef MN_LOG_FILENAME
    mn_log_fp = fopen(MN_LOG_FILENAME, "a+");
    if (mn_log_fp) mn_log_set_fp(mn_log_fp);
#    endif

    mn_log_trace("Application logging started ... ");

    atexit(mn_log_cleanup);
#endif
}

// --------------------------------------------------------------------------------------------------------------
void mn_log_cleanup(void)
{
#ifndef MN_LOG_DISABLE
    mn_log_trace("Application logging completed");

    if (!mn_log_ready) return;
    mn_log_ready = 0;
    aws_mutex_clean_up(&mn_log_mtx);
    if (mn_log_fp) fclose(mn_log_fp);
#endif
}

void mn_log_color(int enable)
{
#ifdef MN_LOG_USE_COLOR
    if (enable) {
#    if _MSC_VER
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) {
            enable = 0;
        }

        DWORD dwMode = 0;
        if (!GetConsoleMode(hOut, &dwMode)) {
            enable = 0;
        }

        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (!SetConsoleMode(hOut, dwMode)) {
            enable = 0;
        }
#    endif
    }

    mn_log_ctx.color = enable;
#endif
}

// --------------------------------------------------------------------------------------------------------------
static void mn_log_lock(void)
{
    if (mn_log_ctx.lock) {
        mn_log_ctx.lock(mn_log_ctx.udata, 1);
    }
}

// --------------------------------------------------------------------------------------------------------------
static void mn_log_unlock(void)
{
    if (mn_log_ctx.lock) {
        mn_log_ctx.lock(mn_log_ctx.udata, 0);
    }
}

// --------------------------------------------------------------------------------------------------------------
void mn_log_set_udata(void *udata)
{
    mn_log_ctx.udata = udata;
}

// --------------------------------------------------------------------------------------------------------------
void mn_log_set_lock(mn_log_lock_fn fn)
{
    mn_log_ctx.lock = fn;
}

// --------------------------------------------------------------------------------------------------------------
void mn_log_set_fp(FILE *fp)
{
    mn_log_ctx.fp = fp;
}

// --------------------------------------------------------------------------------------------------------------
void mn_log_set_level(int level)
{
    mn_log_ctx.level = level;
}

// --------------------------------------------------------------------------------------------------------------
void mn_log_set_quiet(int enable)
{
    mn_log_ctx.quiet = enable ? 1 : 0;
}

// --------------------------------------------------------------------------------------------------------------
void mn_log_log(int level, const char *func, const char *file, int line, uint64_t thread_id, const char *fmt, ...)
{
    if (!mn_log_ready) mn_log_setup();

    if (level < mn_log_ctx.level) return;

    mn_log_lock();

    time_t tstamp = time(NULL);
    struct tm *local_time = localtime(&tstamp);

    if (!mn_log_ctx.quiet) {
        va_list args;

        char time_buf[32];
        time_buf[strftime(time_buf, sizeof(time_buf), "%H:%M:%S", local_time)] = '\0';

        if (mn_log_ctx.color) {
            fprintf(stderr, "%s %s%-5s\033[0m \033[90m%zu:%s:%d - %s: \033[0m ", time_buf, level_colors[level], level_names[level], thread_id, file, line, func);
        } else {
            fprintf(stderr, "%s %-5s %zu:%s:%d - %s: ", time_buf, level_names[level], thread_id, file, line, func);
        }

        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
        fflush(stderr);
    }

    if (mn_log_ctx.fp) {
        va_list args;
        char time_buf[64];
        time_buf[strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", local_time)] = '\0';
        fprintf(mn_log_ctx.fp, "%s %-5s %zu:%s:%d - %s: ", time_buf, level_names[level], thread_id, file, line, func);
        va_start(args, fmt);
        vfprintf(mn_log_ctx.fp, fmt, args);
        va_end(args);
        fprintf(mn_log_ctx.fp, "\n");
        fflush(mn_log_ctx.fp);
    }

    mn_log_unlock();
}
