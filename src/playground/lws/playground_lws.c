/*
 * lws-minimal-ws-client
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates the a minimal ws client using lws.
 *
 * It connects to https://libwebsockets.org/ and makes a
 * wss connection to the dumb-increment protocol there.  While
 * connected, it prints the numbers it is being sent by
 * dumb-increment protocol.
 */

#include <libwebsockets.h>
#include <signal.h>
#include <string.h>

static int interrupted, rx_seen, test;
static struct lws *client_wsi;

#define PAYLOAD_MAX 4096
//static char *message = "                {\"action\": \"sendMessage\", \"username\": \"test-websocket-user\", \"content\": \"Testing the Websocket API.\"}";
static char *message = "                {\"action\": \"getPresence\"}";
static int write_once = 0;


static int callback_aws_api_gateway(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    int m;
    size_t mlen;

    switch (reason) {
    /* because we are protocols[0] ... */
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char *)in : "(null)");
        client_wsi = NULL;
        break;

    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        lwsl_user("connection established\n", __func__);
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        lwsl_user("RX: %s\n", (const char *)in);
        rx_seen++;
        if (test && rx_seen == 10)
            interrupted = 1;
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        if (!write_once) {
            write_once = 1;

            mlen = strlen(message) - LWS_PRE;
            m = lws_write(wsi, ((unsigned char *)message) + LWS_PRE, mlen, LWS_WRITE_TEXT);
            if (m < (int)mlen) {
                lwsl_err("ERROR %d writing to ws socket\n", m);
                return -1;
            }
        }
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        client_wsi = NULL;
        break;

    default:
        break;
    }

    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
    {
        "aws-api-gateway",
        callback_aws_api_gateway,
        0,
        0,
    },
    {NULL, NULL, 0, 0}
};

static void sigint_handler(int sig)
{
    interrupted = 1;
}

int main(int argc, const char **argv)
{
    struct lws_context_creation_info info;
    struct lws_client_connect_info i;
    struct lws_context *context;
    const char *p;
    int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
        /* for LLL_ verbosity above NOTICE to be built into lws, lws
                 * must have been configured with -DCMAKE_BUILD_TYPE=DEBUG
                 * instead of =RELEASE */
        /* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
        /* | LLL_EXT */ /* | LLL_CLIENT */  /* | LLL_LATENCY */
        /* | LLL_DEBUG */;

    signal(SIGINT, sigint_handler);
    if ((p = lws_cmdline_option(argc, argv, "-d")))
        logs = atoi(p);

    test = !!lws_cmdline_option(argc, argv, "-t");

    lws_set_log_level(logs, NULL);
    lwsl_user("LWS minimal ws client rx [-d <logs>] [--h2] [-t (test)]\n");

    memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
    info.protocols = protocols;
    info.timeout_secs = 10;
#if defined(LWS_WITH_MBEDTLS) || defined(USE_WOLFSSL)
    /*
         * OpenSSL uses the system trust store.  mbedTLS has to be told which
         * CA to trust explicitly.
         */
    info.client_ssl_ca_filepath = "./AmazonRootCA1.cer";
#endif

    /*
         * since we know this lws context is only ever going to be used with
         * one client wsis / fds / sockets at a time, let lws know it doesn't
         * have to use the default allocations for fd tables up to ulimit -n.
         * It will just allocate for 1 internal and 1 (+ 1 http2 nwsi) that we
         * will use.
         */
    info.fd_limit_per_thread = 1 + 1 + 1;

    context = lws_create_context(&info);
    if (!context) {
        lwsl_err("lws init failed\n");
        return 1;
    }

    memset(&i, 0, sizeof i); /* otherwise uninitialized garbage */
    i.context = context;
    i.port = 443;
    i.address = "api.serverlynx.net";
    i.path = "/?Username=c6burns";
    i.host = i.address;
    i.origin = i.address;
    i.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_INSECURE;
    i.protocol = protocols[0].name; /* "dumb-increment-protocol" */
    i.pwsi = &client_wsi;

    if (lws_cmdline_option(argc, argv, "--h2"))
        i.alpn = "h2";

    lws_client_connect_via_info(&i);

    while (n >= 0 && client_wsi && !interrupted)
        n = lws_service(context, 0);

    lws_context_destroy(context);

    lwsl_user("Completed %s\n", rx_seen > 10 ? "OK" : "Failed");

    return rx_seen > 10;
}
