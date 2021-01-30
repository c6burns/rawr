#include "playground_srtp.h"

#include "mn/atomic.h"
#include "mn/error.h"
#include "mn/log.h"
#include "mn/thread.h"
#include "mn/time.h"

#include "srtp.h"
#include "portaudio.h"

#ifdef HAVE_UNISTD_H
#    include <unistd.h> /* for close()         */
#elif defined(_MSC_VER)
#    include <io.h> /* for _close()        */
#    define close _close
#endif
#ifdef HAVE_SYS_SOCKET_H
#    include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#    include <netinet/in.h>
#elif defined(_WIN32)
#    include <winsock2.h>
#    include <ws2tcpip.h>
#    define RTPW_USE_WINSOCK2 1
#endif
#ifdef HAVE_ARPA_INET_H
#    include <arpa/inet.h>
#endif

#include <stdlib.h>

mn_thread_t thread_recv;
mn_thread_t thread_send;

int main(void)
{
    srtp_err_status_t status;
    int ret, err;

    mn_log_setup();

    err = Pa_Initialize();
    if (err != paNoError) return -1;

#ifdef RTPW_USE_WINSOCK2
    WORD wVersionRequested = MAKEWORD(2, 0);
    WSADATA wsaData;

    ret = WSAStartup(wVersionRequested, &wsaData);
    if (ret != 0) {
        mn_log_error("error: WSAStartup() failed: %d", ret);
        exit(1);
    }
#endif

    mn_log_trace("Using %s [0x%x]", srtp_get_version_string(), srtp_get_version());

    /* initialize srtp library */
    status = srtp_init();
    if (status) {
        mn_log_trace("error: srtp initialization failed with error code %d", status);
        exit(1);
    }

    mn_thread_setup(&thread_recv);
    mn_thread_launch(&thread_recv, rtp_session_run_recv, NULL);

    mn_thread_setup(&thread_send);
    mn_thread_launch(&thread_send, rtp_session_run_send, NULL);

    while (!rtp_session_get_exiting() && !rtp_session_get_done()) {
        mn_thread_sleep_ms(50);
    }

    rtp_session_set_exiting(1);

    mn_thread_join(&thread_send);
    mn_thread_join(&thread_recv);

    mn_thread_cleanup(&thread_send);
    mn_thread_cleanup(&thread_recv);

    status = srtp_shutdown();
    if (status) {
        mn_log_trace("error: srtp shutdown failed with error code %d", status);
        exit(1);
    }

#ifdef RTPW_USE_WINSOCK2
    WSACleanup();
#endif

    return 0;
}
