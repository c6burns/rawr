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
    "MIID6zCCAtOgAwIBAgIJAJiC/z2kWmcQMA0GCSqGSIb3DQEBBQUAMDYxGzAZBgNV\r\n"
    "BAMMEnNpcC5zZXJ2ZXJseW54Lm5ldDEXMBUGA1UECgwOc2VydmVybHlueC5uZXQw\r\n"
    "HhcNMjAxMjMxMjIzODQyWhcNMjYxMjMwMjIzODQyWjA2MRswGQYDVQQDDBJzaXAu\r\n"
    "c2VydmVybHlueC5uZXQxFzAVBgNVBAoMDnNlcnZlcmx5bngubmV0MIIBIjANBgkq\r\n"
    "hkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA8w9AhM/XnrNqR7Dnq3liqroOElcofihd\r\n"
    "K3VEW9mdAUjbUEsGvjL8VX58HL2vxmf6hUSL8AJjr4LLnxhd980E47yTVL90yOiY\r\n"
    "gWeCZfd7FgdMqVOVzN8ygRKn7z/1+qWIPWfcdOFqX/t/LaqJLpHb+BSWavoOn3YY\r\n"
    "V05A0neRJ8NvbZX0GfV4zPkOWS+lrREedEiAiRH2Imvmggu20MlRIILfpkvgBpEP\r\n"
    "ZFBGflfSzUnzMHIBnT+SP0WYqQ8tIujWfpLWQt15wl94i7BsRX1OZmFqCQ23QwkD\r\n"
    "H4igwLPSXUAiFTeYC+kACFkChIoQA7TqhHSkvSRS3gymOxMGtjbAawIDAQABo4H7\r\n"
    "MIH4MB0GCWCGSAGG+EIBDQQQFg5GUyBTZXJ2ZXIgQ2VydDAJBgNVHRMEAjAAMB0G\r\n"
    "A1UdDgQWBBT4W1DfvKK1uZBh77I8FClD8NptSjBmBgNVHSMEXzBdgBSJBeZiuqvw\r\n"
    "q5NX0jRRkN7QdjVvXKE6pDgwNjEbMBkGA1UEAwwSc2lwLnNlcnZlcmx5bngubmV0\r\n"
    "MRcwFQYDVQQKDA5zZXJ2ZXJseW54Lm5ldIIJAMp2mxnkXvmUMB0GA1UdEQQWMBSC\r\n"
    "EnNpcC5zZXJ2ZXJseW54Lm5ldDARBglghkgBhvhCAQEEBAMCBkAwEwYDVR0lBAww\r\n"
    "CgYIKwYBBQUHAwEwDQYJKoZIhvcNAQEFBQADggEBAH61Yhj3uMT6dvYJ8GdP5wDi\r\n"
    "1lVkAFNuLES+rS6ynXSplWZ8JzZxmoL2mi5zY9r1yFv3O4dIh5wvLjNh0Rweaua1\r\n"
    "egQFJr524IXBFuS+N2p2CfuIjkYOXZ07NM1v7iQVWzq4V7ftc2HWO8McbFA6X3fF\r\n"
    "9E+/6h08KZ2npHmV1g38DulQXhPCTigRX8fL3cVNOdUhMaCLH6i/z5NqvlccTyb/\r\n"
    "DHdAN2h4Tj4Cna3d5B7IrmldBnnF94JqgOYXe6ehbdE/K3dWMolaDTt3Oshj8I7n\r\n"
    "BYcj8K54CZOszdiGc26/O5TVzzkG1/n/hS1e/1XLZXfkA2yEFvoacy0CTaGW2+g=\r\n"
    "-----END CERTIFICATE-----\r\n"
    "-----BEGIN PRIVATE KEY-----\r\n"
    "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDzD0CEz9ees2pH\r\n"
    "sOereWKqug4SVyh+KF0rdURb2Z0BSNtQSwa+MvxVfnwcva/GZ/qFRIvwAmOvgsuf\r\n"
    "GF33zQTjvJNUv3TI6JiBZ4Jl93sWB0ypU5XM3zKBEqfvP/X6pYg9Z9x04Wpf+38t\r\n"
    "qokukdv4FJZq+g6fdhhXTkDSd5Enw29tlfQZ9XjM+Q5ZL6WtER50SICJEfYia+aC\r\n"
    "C7bQyVEggt+mS+AGkQ9kUEZ+V9LNSfMwcgGdP5I/RZipDy0i6NZ+ktZC3XnCX3iL\r\n"
    "sGxFfU5mYWoJDbdDCQMfiKDAs9JdQCIVN5gL6QAIWQKEihADtOqEdKS9JFLeDKY7\r\n"
    "Ewa2NsBrAgMBAAECggEAdQw7Pbn5U+uCmtOOPP1PjnfanupqDZbSh0MJkFHTUfML\r\n"
    "6ja3IJDdAy7OBeky5JDeC59e5E3CQ5rxA8RwPAz29dSx/nXUf4vDJU37da8gDxOk\r\n"
    "z0X5NZemcpHRXV2nzvQ4D5ociAyldWNfc1ZUiaCkFWuUpB+XOyNbxW9ht/jsZgC/\r\n"
    "OlS6ZniP+SJAeY5q1QKFfxXNmfBllJwIze7rUvAHbdW2L0KFllZAKBBaHJcvmJnH\r\n"
    "JKjADM3pe1MeXvuyamEczXcpR0Flyd06JTgot556ai4n3bTG9EmXfMTuQKZx8p6o\r\n"
    "VNl9yioANr3iztsu3UTSUvNBihnkVy6ge3oVld5bQQKBgQD87a62p71Q40bZqYbW\r\n"
    "9Aj0l7ROYeBYw9h4qrpfXWqoghH7RDKzWqFfPHn7sZ34wZGGj8bQGSELS3YDMMs6\r\n"
    "WvGnyc0YlgG5l2cis1NutLEeF58Swfvrz5JVeJf1QSUea8jrsfujHNCWqfD7vXtz\r\n"
    "0sF0UQ3ZqZTH48AgfN67Kfk1pwKBgQD2AuOAtqgbkdBeHxaicD6F3afBX1eEBVX/\r\n"
    "P6qwJLFxJ0HQCBZHqOuMFtTIMkWYgZ5W912b5PR9qaqleys2i1Pa8RcPpaI3FsOc\r\n"
    "O8JvEGHg8GaySkbGHwjI/Cw3bPXLr2aC3f7fKlf8mnJ2KLx5mZekeIgL7NtqIi7/\r\n"
    "ycyWsYV/nQKBgQDgb2B7QDkjj6mM93tpPj68G+mpK/zRh2eNG6IpgVFlmZWvKwL6\r\n"
    "V8+eHKH5j9CnrcweZXJ7sfC6fwmHJ0MO0yhgRRezW1jIgOrJxeqg78HC/B7xnCSZ\r\n"
    "SSWGpm3g+R+g8O/nBZZPVQBa0Q2/tJHZYwi62Dm8DViyTwxrR6K03jf/PQKBgBw0\r\n"
    "tJTbXGbczwEbm2LAb8q1YTiAj+4pFnUPfah4bIfGsnsBklxg97C2JWtWqDgWFGtw\r\n"
    "LSFknMuTmmciug+k5dZicfxvRyv9xiuxhldpj29U4NFsRrUMddtlXkR0j7HsyFoU\r\n"
    "zdYUasYhhyIZBZMDkyleUGrdm0KN7MmS/4v/iojZAoGARYAhcPmRj2sLsBv9Hr64\r\n"
    "hQsHILkgqeIcmkDOFeJi0+wCtZH9g6cbuT9f2sz2mtoRNNp68k5/OwOT9AvRZpfo\r\n"
    "B+ShA8s0lyK9YSfDeWH8dIS0h/8nG1I2pC8zBnDT7T5hlEOIdHjzNDWznburU147\r\n"
    "cmULGZDK+I65u5CZvhqBThk=\r\n"
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
