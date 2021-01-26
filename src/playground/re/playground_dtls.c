/**
 * @file dtls.c DTLS Testcode
 *
 * Copyright (C) 2010 Creytiv.com
 * Copyright (C) 2020 TLM Partners Inc.
 */

#include "rawr/Error.h"

#include "mn/log.h"

#include <re.h>
#include <string.h>

#define DEBUG_MODULE "dtls_test"
#define DEBUG_LEVEL 5
#include <re_dbg.h>

#define ESKIPPED (-1000)

#define TEST_EQUALS(expected, actual)                         \
    if ((expected) != (actual)) {                             \
        mn_log_error("TEST_EQUALS: %s:%u: %s():"              \
                     " expected=%d(0x%x), actual=%d(0x%x)\n", \
                     __FILE__,                                \
                     __LINE__,                                \
                     __func__,                                \
                     (expected),                              \
                     (expected),                              \
                     (actual),                                \
                     (actual));                               \
        err = EINVAL;                                         \
        goto out;                                             \
    }

#define TEST_NOT_EQUALS(expected, actual)           \
    if ((expected) == (actual)) {                   \
        mn_log_error("TEST_NOT_EQUALS: %s:%u:"      \
                     " expected=%d != actual=%d\n", \
                     __FILE__,                      \
                     __LINE__,                      \
                     (expected),                    \
                     (actual));                     \
        err = EINVAL;                               \
        goto out;                                   \
    }

#define TEST_MEMCMP(expected, expn, actual, actn)                \
    if (expn != actn ||                                          \
        0 != memcmp((expected), (actual), (expn))) {             \
        mn_log_error("TEST_MEMCMP: %s:%u:"                       \
                     " %s(): failed\n",                          \
                     __FILE__,                                   \
                     __LINE__,                                   \
                     __func__);                                  \
        test_hexdump_dual(stderr, expected, expn, actual, actn); \
        err = EINVAL;                                            \
        goto out;                                                \
    }

#define TEST_STRCMP(expected, expn, actual, actn)                 \
    if (expn != actn ||                                           \
        0 != memcmp((expected), (actual), (expn))) {              \
        mn_log_error("TEST_STRCMP: %s:%u:"                        \
                     " failed\n",                                 \
                     __FILE__,                                    \
                     __LINE__);                                   \
        (void)re_fprintf(stderr, "expected string: (%zu bytes)\n" \
                                 "\"%b\"\n",                      \
                         (size_t)(expn),                          \
                         (expected),                              \
                         (size_t)(expn));                         \
        (void)re_fprintf(stderr, "actual string: (%zu bytes)\n"   \
                                 "\"%b\"\n",                      \
                         (size_t)(actn),                          \
                         (actual),                                \
                         (size_t)(actn));                         \
        err = EINVAL;                                             \
        goto out;                                                 \
    }

#define TEST_ASSERT(actual)                \
    if (!(actual)) {                       \
        mn_log_error("TEST_ASSERT: %s:%u:" \
                     " actual=%d\n",       \
                     __FILE__,             \
                     __LINE__,             \
                     (actual));            \
        err = EINVAL;                      \
        goto out;                          \
    }

#define TEST_ERR(err)                   \
    if ((err)) {                        \
        mn_log_error("TEST_ERR: %s:%u:" \
                     " (%m)\n",         \
                     __FILE__,          \
                     __LINE__,          \
                     (err));            \
        goto out;                       \
    }

#define TEST_SACMP(expect, actual, flags)          \
    if (!sa_cmp((expect), (actual), (flags))) {    \
                                                   \
        mn_log_error("TEST_SACMP: %s:%u:"          \
                     " %s(): failed\n",            \
                     __FILE__,                     \
                     __LINE__,                     \
                     __func__);                    \
        DEBUG_WARNING("expected: %J\n", (expect)); \
        DEBUG_WARNING("actual:   %J\n", (actual)); \
        err = EADDRNOTAVAIL;                       \
        goto out;                                  \
    }

/* 2048-bit */
const char test_certificate_rsa[] =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDgTCCAmmgAwIBAgIJAIcN6jBFyKjhMA0GCSqGSIb3DQEBCwUAMFYxCzAJBgNV\r\n"
    "BAYTAk5PMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX\r\n"
    "aWRnaXRzIFB0eSBMdGQxDzANBgNVBAMMBnJldGVzdDAgFw0xODEwMjcxODA1MzRa\r\n"
    "GA8yMTE4MTAwMzE4MDUzNFowVjELMAkGA1UEBhMCTk8xEzARBgNVBAgMClNvbWUt\r\n"
    "U3RhdGUxITAfBgNVBAoMGEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDEPMA0GA1UE\r\n"
    "AwwGcmV0ZXN0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEArc68J+if\r\n"
    "L8chKZmA3CwSgYJJE3Oy5bBQZP5X2U+YHbQ8en74TeZW7cA5ZFQFQZptZXBwhFIY\r\n"
    "61fiHLtB8R9v7hG/SDYbb1gNpXiluDUnpzK89YmEBSEOPxmZvoeMabNNBNOizSFh\r\n"
    "4B8mlmLdlE+vUuJYugbZfumwjWRXQrEmGc0idg/tQmu8MCIKGWzyrRJ9ZYwUjFtb\r\n"
    "9FHYHjVd5BhWHVuLYYx8lLt96s5GCv1FBpnPZZ3khN2vXGDque2YWVLJ++i2A+HU\r\n"
    "UBQErQx+Id2t0F8hvlOoiwdfnftao9rrugn1xJL0zEqNoZbW87+xV644v6o5q21R\r\n"
    "fO/SPQdvY0RweQIDAQABo1AwTjAdBgNVHQ4EFgQUCB22b0XV8mVOgFnUKhbXCWV1\r\n"
    "showHwYDVR0jBBgwFoAUCB22b0XV8mVOgFnUKhbXCWV1showDAYDVR0TBAUwAwEB\r\n"
    "/zANBgkqhkiG9w0BAQsFAAOCAQEAX7ISa9OBdEjUGy81IU5G5LJKBuoFQNnTyOnJ\r\n"
    "ZQlEOCKJIvWbb0WSA0L03VqPT1PpgMd8rtjo5M9hBkldoTxLa9Am1jN5IYlCBXlr\r\n"
    "F6w0teySuWCdHbGOp4YYqga0yn/8pfZVZlWWp4Q0q+FSew4p9BFpSp5y5yLeKUZI\r\n"
    "R/SkSNWCEsIZM/+iGOeTs97FghJsZKUFiSDO0pKuJHGxcDSwOQpP//FuGh+u5Lve\r\n"
    "ZYxcrBTDjqq4p1MP8ypVgaRraXVeiHFrX5yVL/Nsd1xmOKLBS1Dm2GlAPFgTGOy6\r\n"
    "5rE/+p+RoTySim+1ZN9ojcKryPKhyf75KtS02sQUTef1T2XO1A==\r\n"
    "-----END CERTIFICATE-----\r\n"
    "-----BEGIN PRIVATE KEY-----\r\n"
    "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCtzrwn6J8vxyEp\r\n"
    "mYDcLBKBgkkTc7LlsFBk/lfZT5gdtDx6fvhN5lbtwDlkVAVBmm1lcHCEUhjrV+Ic\r\n"
    "u0HxH2/uEb9INhtvWA2leKW4NSenMrz1iYQFIQ4/GZm+h4xps00E06LNIWHgHyaW\r\n"
    "Yt2UT69S4li6Btl+6bCNZFdCsSYZzSJ2D+1Ca7wwIgoZbPKtEn1ljBSMW1v0Udge\r\n"
    "NV3kGFYdW4thjHyUu33qzkYK/UUGmc9lneSE3a9cYOq57ZhZUsn76LYD4dRQFASt\r\n"
    "DH4h3a3QXyG+U6iLB1+d+1qj2uu6CfXEkvTMSo2hltbzv7FXrji/qjmrbVF879I9\r\n"
    "B29jRHB5AgMBAAECggEBAIvl/p8k55TedHv2ebk+pDqoMse8df/ZViykaPOa1Hb8\r\n"
    "Tz3OG3EgeVHvSoLN+lkewvVGdtqa9kHgQDkeJOq/gimfEVc/bf/GYV2SadmGt38m\r\n"
    "IOCGKsSyIbR6l7y7gDLIRrMe4ki4mP58NGQR+gZZyWYumHpL7x7vXNPCM1aUHnXe\r\n"
    "yy3lIHz4FZCRXxbALbFlmGHf6jwByWnKuRh/b8PSnnwlaTCaXuXYMWMelDF9J7qY\r\n"
    "YqGBSowCQhVOLJj3DdRSS9iCBSNl5vhW88FYQgEgOiLoZ0gnmM1w+3YON2xDzEkD\r\n"
    "gI1BynnC1nucfxrXA0vRhraP2Z3EE5vDOzB43QTsKHECgYEA2MNmTkVnRky9UhWd\r\n"
    "K7HtJsnMwib7ynph9jjFk2YT88uK5ZYjJbDDmb34xRNYB8BKh8VKgTgzkn8qp6rk\r\n"
    "YiRoH4E/QiBqYtcpsDO47qhBF7ePvJPo11wvovSqCILzZUA1Trc0AYjsRvUN6Sh2\r\n"
    "OGYnxz9A5hIiy42N8CdCfNvgavUCgYEAzUTPEJnoK3qkWRb/wbF+2RBr5MMzbaTW\r\n"
    "OGjN88Z+oR0RgtQnYobkdXUhDvXrxEuzZ9JP2E70wFMYO2yfaak/PjW9jPY/WIRf\r\n"
    "pogbycgmvC17MyNsuD5H97ecfIbZnSybKUsFh8bH4/ZFcw901BBhnwRxp0agdUVM\r\n"
    "6eKeWqGhRPUCgYBqpnFOr30prJY1rea/2fI59G4nVLDsJZzPXY1wgXftqsbzQRSX\r\n"
    "9cm3ei3NIUBdx/GjraGDxJgzSxg8mKt30jvczGXIblSJvx2G0Vv7KJOmTK2O9iNI\r\n"
    "2tWhUsnaGDwTJC1WRnNzEeBW5Tlr73mDNFf8A5Y13NR73HDqqRZggnp/hQKBgQCN\r\n"
    "FSIMkvvUBnM3GGuowUoh/vtpPBD45zalhsMnLeKS8du7Q/3d5kDXyi1yjuwA+tbQ\r\n"
    "IOjoDzyBg5tAHKRkhwMEywMBA67+M91aJGqVAZA9/jSTLWHoMEZeqEBSBo1DTglH\r\n"
    "FF00uRdiQz3wm0r9BlVSakeDZTOb5om6pxuXx0eEkQKBgH1GDEMzNR0pQXAAdqBh\r\n"
    "hw4IgJR4OH/oy1RIsb0Cb5XwqV+3ELwidkwA72vSe79+w9/k+vApFBSF6hNrt3Vd\r\n"
    "jsyQBooGg094utKUC/HCJHZVXpoDhdOq+P7wvcD49lkRwElK1TYUMsYuq2RX/h9U\r\n"
    "hJKcMvPh9yea4qLzdmMGjiMl\r\n"
    "-----END PRIVATE KEY-----\r\n";

struct dtls_test {
    bool dtls_srtp;
    struct dtls_sock *sock_cli, *sock_srv;
    struct tls_conn *conn_cli, *conn_srv;
    struct tls *tls;
    int err;

    struct {
        enum srtp_suite suite;
        uint8_t cli_key[30];
        uint8_t srv_key[30];
    } cli, srv;

    uint8_t fp[20];
    char cn[64];
    unsigned n_srv_estab;
    unsigned n_srv_recv;
    unsigned n_cli_estab;
    unsigned n_cli_recv;
    unsigned n_conn;
};

static const char *common_name = "retest";
static const char *payload_str = "hello from a cute DTLS client";

void test_hexdump_dual(FILE *f, const void *ep, size_t elen, const void *ap, size_t alen);

static void abort_test(struct dtls_test *t, int err)
{
    t->err = err;
    re_cancel();
}

static int send_data(struct dtls_test *t, const char *data)
{
    struct mbuf mb;
    int err;

    TEST_ASSERT(t->conn_cli != NULL);

    mb.buf = (void *)data;
    mb.pos = 0;
    mb.end = str_len(data);
    mb.size = str_len(data);

    err = dtls_send(t->conn_cli, &mb);
    if (err)
        goto out;

out:
    return err;
}

static void srv_estab_handler(void *arg)
{
    struct dtls_test *t = arg;
    int err = 0;

    ++t->n_srv_estab;

    if (t->dtls_srtp) {
        err = tls_srtp_keyinfo(t->conn_srv, &t->srv.suite, t->srv.cli_key, sizeof(t->srv.cli_key), t->srv.srv_key, sizeof(t->srv.srv_key));
        TEST_EQUALS(0, err);
    }

out:
    if (err)
        abort_test(t, err);
}

static void srv_recv_handler(struct mbuf *mb, void *arg)
{
    struct dtls_test *t = arg;
    int err;

    ++t->n_srv_recv;

    /* echo */
    err = dtls_send(t->conn_srv, mb);
    if (err) {
        abort_test(t, err);
    }
}

static void srv_close_handler(int err, void *arg)
{
    struct dtls_test *t = arg;
    (void)err;

    t->conn_srv = mem_deref(t->conn_srv);
}

static void cli_estab_handler(void *arg)
{
    struct dtls_test *t = arg;
    int err;

    ++t->n_cli_estab;

    err = tls_peer_fingerprint(t->conn_cli, TLS_FINGERPRINT_SHA1, t->fp, sizeof(t->fp));
    TEST_EQUALS(0, err);

    err = tls_peer_common_name(t->conn_cli, t->cn, sizeof(t->cn));
    TEST_EQUALS(0, err);

    if (t->dtls_srtp) {

        err = tls_srtp_keyinfo(t->conn_cli, &t->cli.suite, t->cli.cli_key, sizeof(t->cli.cli_key), t->cli.srv_key, sizeof(t->cli.srv_key));
        TEST_EQUALS(0, err);
    }

    err = send_data(t, payload_str);

out:
    if (err)
        abort_test(t, err);
}

static void cli_recv_handler(struct mbuf *mb, void *arg)
{
    struct dtls_test *t = arg;
    int err = 0;

    ++t->n_cli_recv;

    TEST_STRCMP(payload_str, strlen(payload_str), mbuf_buf(mb), mbuf_get_left(mb));

out:
    abort_test(t, err);
}

static void cli_close_handler(int err, void *arg)
{
    struct dtls_test *t = arg;
    (void)err;

    t->conn_cli = mem_deref(t->conn_cli);
}

static void conn_handler(const struct sa *src, void *arg)
{
    struct dtls_test *t = arg;
    int err;
    (void)src;

    ++t->n_conn;

    TEST_ASSERT(t->conn_srv == NULL);

    err = dtls_accept(&t->conn_srv, t->tls, t->sock_srv, srv_estab_handler, srv_recv_handler, srv_close_handler, t);
    if (err) {
        if (err == EPROTO)
            err = ENOMEM;
        goto out;
    }

out:
    if (err)
        abort_test(t, err);
}

static int test_dtls_srtp_base(enum tls_method method, bool dtls_srtp)
{
    static const char *srtp_suites =
        "SRTP_AES128_CM_SHA1_80:"
        "SRTP_AES128_CM_SHA1_32";
    struct dtls_test test;
    struct udp_sock *us = NULL;
    struct sa cli, srv;
    uint8_t fp[20];
    int err;

    memset(&test, 0, sizeof(test));

    test.dtls_srtp = dtls_srtp;

    err = tls_alloc(&test.tls, method, NULL, NULL);
    if (err)
        goto out;

    err = tls_set_certificate(test.tls, test_certificate_rsa, strlen(test_certificate_rsa));
    if (err)
        goto out;

    if (dtls_srtp) {
        err = tls_set_srtp(test.tls, srtp_suites);

        /* SRTP not supported */
        if (err == ENOSYS) {
            err = 0;
            goto out;
        }

        TEST_ERR(err);
    }

    err = tls_fingerprint(test.tls, TLS_FINGERPRINT_SHA1, fp, sizeof(fp));
    TEST_EQUALS(0, err);

    (void)sa_set_str(&cli, "127.0.0.1", 0);
    (void)sa_set_str(&srv, "127.0.0.1", 0);

    err = udp_listen(&us, &srv, NULL, NULL);
    if (err)
        goto out;

    err = udp_local_get(us, &srv);
    if (err)
        goto out;

    err = dtls_listen(&test.sock_srv, NULL, us, 4, 0, conn_handler, &test);
    if (err)
        goto out;

    err = dtls_listen(&test.sock_cli, &cli, NULL, 4, 0, NULL, NULL);
    if (err)
        goto out;

    err = dtls_connect(&test.conn_cli, test.tls, test.sock_cli, &srv, cli_estab_handler, cli_recv_handler, cli_close_handler, &test);
    if (err) {
        if (err == EPROTO)
            err = ENOMEM;
        goto out;
    }

    err = re_main_timeout(800);
    if (err)
        goto out;

    if (test.err) {
        err = test.err;
        goto out;
    }

    /* verify result after test is complete */
    TEST_EQUALS(1, test.n_srv_estab);
    TEST_EQUALS(1, test.n_srv_recv);
    TEST_EQUALS(1, test.n_cli_estab);
    TEST_EQUALS(1, test.n_cli_recv);
    TEST_EQUALS(1, test.n_conn);

    TEST_MEMCMP(fp, sizeof(fp), test.fp, sizeof(test.fp));
    TEST_STRCMP(common_name, strlen(common_name), test.cn, strlen(test.cn));

    if (dtls_srtp) {

        TEST_EQUALS(test.cli.suite, test.srv.suite);
        TEST_MEMCMP(test.cli.cli_key, sizeof(test.cli.cli_key), test.srv.cli_key, sizeof(test.srv.cli_key));
        TEST_MEMCMP(test.cli.srv_key, sizeof(test.cli.srv_key), test.srv.srv_key, sizeof(test.srv.srv_key));
    }

out:
    test.conn_cli = mem_deref(test.conn_cli);
    test.conn_srv = mem_deref(test.conn_srv);
    test.sock_cli = mem_deref(test.sock_cli);
    test.sock_srv = mem_deref(test.sock_srv);
    test.tls = mem_deref(test.tls);
    mem_deref(us);

    return err;
}

static bool have_dtls_support(enum tls_method method)
{
    struct tls *tls = NULL;
    int err;

    err = tls_alloc(&tls, method, NULL, NULL);

    mem_deref(tls);

    return err != ENOSYS;
}

void test_hexdump_dual(FILE *f, const void *ep, size_t elen, const void *ap, size_t alen)
{
    const uint8_t *ebuf = ep;
    const uint8_t *abuf = ap;
    size_t i, j, len;
#define WIDTH 8

    if (!f || !ep || !ap)
        return;

    len = max(elen, alen);

    (void)re_fprintf(f, "\nOffset:   Expected (%zu bytes):    "
                        "   Actual (%zu bytes):\n",
                     elen,
                     alen);

    for (i = 0; i < len; i += WIDTH) {

        (void)re_fprintf(f, "0x%04zx   ", i);

        for (j = 0; j < WIDTH; j++) {
            const size_t pos = i + j;
            if (pos < elen) {
                bool wrong = pos >= alen;

                if (wrong)
                    (void)re_fprintf(f, "\x1b[35m");
                (void)re_fprintf(f, " %02x", ebuf[pos]);
                if (wrong)
                    (void)re_fprintf(f, "\x1b[;m");
            } else
                (void)re_fprintf(f, "   ");
        }

        (void)re_fprintf(f, "    ");

        for (j = 0; j < WIDTH; j++) {
            const size_t pos = i + j;
            if (pos < alen) {
                bool wrong;

                if (pos < elen)
                    wrong = ebuf[pos] != abuf[pos];
                else
                    wrong = true;

                if (wrong)
                    (void)re_fprintf(f, "\x1b[33m");
                (void)re_fprintf(f, " %02x", abuf[pos]);
                if (wrong)
                    (void)re_fprintf(f, "\x1b[;m");
            } else
                (void)re_fprintf(f, "   ");
        }

        (void)re_fprintf(f, "\n");
    }

    (void)re_fprintf(f, "\n");
}


static void oom_watchdog_timeout(void *arg)
{
    int *err = arg;

    *err = ETIMEDOUT;

    re_cancel();
}

static uint32_t timeout_override;
int re_main_timeout(uint32_t timeout_ms)
{
    struct tmr tmr;
    int err = 0;

    tmr_init(&tmr);

    if (timeout_override != 0)
        timeout_ms = timeout_override;

#ifdef TEST_TIMEOUT
    timeout_ms = TEST_TIMEOUT;
#endif

    tmr_start(&tmr, timeout_ms, oom_watchdog_timeout, &err);
    (void)re_main(NULL);

    tmr_cancel(&tmr);
    return err;
}

int test_dtls(void)
{
    int err = 0;

    /* NOTE: DTLS v1.0 should be available on all
	 *       supported platforms.
	 */
    if (!have_dtls_support(TLS_METHOD_DTLSV1)) {
        (void)re_printf("skip DTLS 1.0 tests\n");
        return ESKIPPED;
    } else {
        err = test_dtls_srtp_base(TLS_METHOD_DTLSV1, false);
        if (err)
            return err;
    }

    return 0;
}

int main(void)
{
    int err = 0;

    libre_init();

    if (!have_dtls_support(TLS_METHOD_DTLSV1)) {
        (void)re_printf("skip DTLS tests\n");
        return ESKIPPED;
    }

    err = test_dtls_srtp_base(TLS_METHOD_DTLSV1, true);
    if (err)
        return err;


    libre_close();

    return 0;
}
