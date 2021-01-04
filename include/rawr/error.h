#ifndef RAWR_ERROR_H
#define RAWR_ERROR_H

#include <errno.h>

#define RAWR_OK 0
#define RAWR_ERR -1

#ifdef RAWR_C_MISSING_ASSERT
#    define RAWR_ASSERT(expr) ((void)0)
#else
#    include <assert.h>
#    define RAWR_ASSERT(expr) assert(expr)
#endif

#define RAWR_GUARD(expr) \
    if ((expr)) return RAWR_ERR
#define RAWR_GUARD_NULL(expr) \
    if (!(expr)) return RAWR_ERR
#define RAWR_GUARD_GOTO(lbl, expr) \
    {                            \
        if ((expr)) goto lbl;    \
    }
#define RAWR_GUARD_CLEANUP(expr) RAWR_GUARD_GOTO(cleanup, expr)
#define RAWR_GUARD_NULL_CLEANUP(expr) RAWR_GUARD_GOTO(cleanup, !(expr))

typedef enum rawr_ErrorCode {
    rawr_Success = 0,
    rawr_Error = -1,
} rawr_ErrorCode;

#endif
