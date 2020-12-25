#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include <strophe.h>
#include "common.h"

#include "wksxep0047.h"
#include "xmpp_ibb.h"

#include "mn/error.h"
#include "mn/atomic.h"
#include "mn/thread.h"

xmpp_conn_t*  XMPP_Init(char* jid, char* pass, char* host, xmpp_ctx_t  **pctx);
void XMPP_Close(xmpp_ctx_t *ctx,  xmpp_conn_t *conn);

xmpp_conn_t* conn = NULL;
xmpp_ctx_t *ctx = NULL;


typedef enum xepthread_state_e {
    XEPTHREAD_STATE_NONE,
    XEPTHREAD_STATE_STARTING,
    XEPTHREAD_STATE_STARTED,
    XEPTHREAD_STATE_STOPPING,
    XEPTHREAD_STATE_STOPPED,
} xepthread_state_t;

typedef struct xepthread_s {
    mn_thread_t thread;
    mn_atomic_t state;
} xepthread_t;

xepthread_t xepthread = {
    .thread = { 0 },
    .state = MN_ATOMIC_INIT(XEPTHREAD_STATE_NONE),
};

// --------------------------------------------------------------------------------------------------------------
int xepthread_setup(xepthread_t *xepthread)
{
    MN_ASSERT(xepthread);

    mn_atomic_store(&xepthread->state, XEPTHREAD_STATE_NONE);

    MN_GUARD(mn_thread_setup(&xepthread->thread));

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void xepthread_cleanup(xepthread_t *xepthread)
{
    MN_ASSERT(xepthread);

    mn_thread_cleanup(&xepthread->thread);
}

// --------------------------------------------------------------------------------------------------------------
xepthread_state_t xepthread_state_get(xepthread_t* xepthread)
{
    MN_ASSERT(xepthread);
    return (xepthread_state_t)mn_atomic_load(&xepthread->state);
}

// private ------------------------------------------------------------------------------------------------------
void xepthread_state_set(xepthread_t* xepthread, xepthread_state_t state)
{
    MN_ASSERT(xepthread);
    mn_atomic_store(&xepthread->state, state);
}

// --------------------------------------------------------------------------------------------------------------
void xepthread_run_func(xepthread_t* xepthread)
{
    MN_ASSERT(xepthread);

    if (xepthread_state_get(xepthread) == XEPTHREAD_STATE_STARTING) {
        xepthread_state_set(xepthread, XEPTHREAD_STATE_STARTED);
        while (xepthread_state_get(xepthread) == XEPTHREAD_STATE_STARTED) {
            mn_thread_sleep_s(1);

            if (XMPP_IBB_Get_Recv() != NULL)
            {
                printf("Get IBB Data=[%s]\n", XMPP_IBB_Get_Recv());
                printf("main gStanza=[%zx]\n", (uintptr_t)(void *)XMPP_IBB_Get_gStanza());
                if (XMPP_IBB_Get_gStanza() != NULL) {
                    // echo test
                    XMPP_IBB_SendPayload(conn, XMPP_IBB_Get_gStanza(), (void*)ctx, XMPP_IBB_Get_Recv());
                }

                XMPP_IBB_Reset_Recv();
                XMPP_IBB_Reset_gStanza();
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------
int xepthread_start(xepthread_t* xepthread)
{
    MN_ASSERT(xepthread);
    
    xepthread_state_t state = xepthread_state_get(xepthread);
    if (state == XEPTHREAD_STATE_STARTING || state == XEPTHREAD_STATE_STARTED) return MN_SUCCESS;

    MN_GUARD(state != XEPTHREAD_STATE_STOPPING && state != XEPTHREAD_STATE_STOPPED);

    xepthread_state_set(xepthread, XEPTHREAD_STATE_STARTING);
    return mn_thread_launch(&xepthread->thread, xepthread_run_func, NULL);
}

// --------------------------------------------------------------------------------------------------------------
int xepthread_stop(xepthread_t* xepthread)
{
    MN_ASSERT(xepthread);

    xepthread_state_t state = xepthread_state_get(xepthread);
    if (state == XEPTHREAD_STATE_NONE || state == XEPTHREAD_STATE_STOPPING || state == XEPTHREAD_STATE_STOPPED) return MN_SUCCESS;

    MN_GUARD(state != XEPTHREAD_STATE_STARTING);

    xepthread_state_set(xepthread, XEPTHREAD_STATE_STOPPING);
    MN_GUARD(mn_thread_join(&xepthread->thread));

    xepthread_state_set(xepthread, XEPTHREAD_STATE_STOPPED);

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int  main(int argc, char **argv)
{
 
 //   xmpp_conn_t* conn = NULL;
 //   xmpp_ctx_t *ctx = NULL;    
    char *jid, *pass, *host;	
    int errornum=0;
    void *ptr;

    /* take a jid and password on the command line */
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: xep0047-test  <jid> <pass> [<host>]\n\n");
        return 0;
    }

    jid = argv[1];
    pass = argv[2];
    host = NULL;

    if (argc == 4) {
        host = argv[3];
    }

    conn = XMPP_Init(jid , pass, host, &ctx);


    xepthread_setup(&xepthread);
    xepthread_start(&xepthread);

    if (ctx != NULL) {
        xmpp_run(ctx);
    }
 
    xmpp_stop(ctx);
    
    mn_thread_sleep_s(1);

    XMPP_Close(ctx, conn);

    xepthread_stop(&xepthread);
    xepthread_cleanup(&xepthread);

    return 0;
 
}

