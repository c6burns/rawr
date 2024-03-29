cmake_minimum_required(VERSION 3.15)
project(re LANGUAGES C)

# detect PS5 toolchain
if (__COMPILER_PS5)
	set(PS5 ON)
	add_definitions(-DPS5)
endif()

set(APPLE_SYSCONFIG)
if (APPLE)
	find_library(APPLE_SYSCONFIG SystemConfiguration)
	find_library(APPLE_CORE CoreFoundation)
	set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
endif()

if (NOT PS5)
    find_package(OpenSSL REQUIRED)
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12)

set(RE_SRC
	src/hmac/hmac.c
	src/hmac/hmac_sha1.c
	src/hmac/openssl/hmac.c

	src/md5/wrap.c
	src/md5/md5.c

	src/net/netstr.c
	src/net/if.c
	src/net/rt.c
	src/net/sockopt.c
	src/net/sock.c
	src/net/net.c

	src/stun/msg.c
	src/stun/keepalive.c
	src/stun/hdr.c
	src/stun/addr.c
	src/stun/dnsdisc.c
	src/stun/ind.c
	src/stun/stunstr.c
	src/stun/rep.c
	src/stun/ctrans.c
	src/stun/attr.c
	src/stun/req.c
	src/stun/stun.c

	src/jbuf/jbuf.c

	src/sipsess/request.c
	src/sipsess/info.c
	src/sipsess/listen.c
	src/sipsess/accept.c
	src/sipsess/reply.c
	src/sipsess/connect.c
	src/sipsess/modify.c
	src/sipsess/ack.c
	src/sipsess/close.c
	src/sipsess/sess.c

	src/udp/mcast.c

	src/msg/param.c
	src/msg/ctype.c

	src/hash/func.c
	src/hash/hash.c

	src/natbd/genalg.c
	src/natbd/lifetime.c
	src/natbd/hairpinning.c
	src/natbd/natstr.c
	src/natbd/mapping.c
	src/natbd/filtering.c

	src/fmt/str_error.c
	src/fmt/unicode.c
	src/fmt/ch.c
	src/fmt/print.c
	src/fmt/pl.c
	src/fmt/str.c
	src/fmt/hexdump.c
	src/fmt/time.c
	src/fmt/prm.c
	src/fmt/regex.c

	src/base64/b64.c

	src/dbg/dbg.c

	src/sipreg/reg.c

	src/ice/comp.c
	src/ice/icesdp.c
	src/ice/util.c
	src/ice/icestr.c
	src/ice/ice.c
	src/ice/candpair.c
	src/ice/stunsrv.c
	src/ice/cand.c
	src/ice/chklist.c
	src/ice/icem.c
	src/ice/connchk.c

	src/sipevent/msg.c
	src/sipevent/subscribe.c
	src/sipevent/listen.c
	src/sipevent/notify.c

	src/bfcp/msg.c
	src/bfcp/request.c
	src/bfcp/conn.c
	src/bfcp/attr.c
	src/bfcp/reply.c

	src/websock/websock.c

	src/httpauth/basic.c
	src/httpauth/digest.c

	src/crc32/crc32.c

	src/turn/chan.c
	src/turn/turnc.c
	src/turn/perm.c

	src/tmr/tmr.c

	src/odict/odict.c
	src/odict/get.c
	src/odict/type.c
	src/odict/entry.c

	src/sa/ntop.c
	src/sa/printaddr.c
	src/sa/pton.c
	src/sa/sa.c

	src/telev/telev.c

	src/sys/rand.c
	src/sys/sys.c
	src/sys/fs.c
	src/sys/sleep.c
	src/sys/daemon.c
	src/sys/endian.c

	src/json/decode_odict.c
	src/json/decode.c
	src/json/encode.c

    # no OpenSSL for now
	src/tls/openssl/tls_tcp.c
	src/tls/openssl/tls_udp.c
	src/tls/openssl/tls.c

	src/tcp/tcp_high.c

	src/http/msg.c
	src/http/auth.c
	src/http/request.c
	src/http/server.c
	src/http/client.c
	src/http/chunk.c

	src/list/list.c

	src/sdp/session.c
	src/sdp/msg.c
	src/sdp/util.c
	src/sdp/format.c
	src/sdp/str.c
	src/sdp/attr.c
	src/sdp/media.c

	src/rtp/ntp.c
	src/rtp/fb.c
	src/rtp/rr.c
	src/rtp/pkt.c
	src/rtp/rtcp.c
	src/rtp/member.c
	src/rtp/sdes.c
	src/rtp/source.c
	src/rtp/rtp.c
	src/rtp/sess.c

	src/sha/sha1.c

	src/rtmp/amf_dec.c
	src/rtmp/dechunk.c
	src/rtmp/hdr.c
	src/rtmp/conn.c
	src/rtmp/amf_enc.c
	src/rtmp/ctrans.c
	src/rtmp/control.c
	src/rtmp/chunk.c
	src/rtmp/amf.c
	src/rtmp/stream.c

	src/sip/strans.c
	src/sip/contact.c
	src/sip/msg.c
	src/sip/auth.c
	src/sip/request.c
	src/sip/keepalive.c
	src/sip/transp.c
	src/sip/addr.c
	src/sip/ctrans.c
	src/sip/cseq.c
	src/sip/reply.c
	src/sip/via.c
	src/sip/dialog.c
	src/sip/sip.c
	src/sip/keepalive_udp.c

	src/mbuf/mbuf.c

	src/main/init.c
	src/main/main.c
	src/main/method.c
	src/main/openssl.c

	src/dns/rr.c
	src/dns/hdr.c
	src/dns/cstr.c
	src/dns/rrlist.c
	src/dns/dname.c
	src/dns/client.c
	src/dns/ns.c

	src/srtp/replay.c
	src/srtp/srtcp.c
	src/srtp/srtp.c
	src/srtp/misc.c
	src/srtp/stream.c

	src/uri/uric.c
	src/uri/uri.c

	src/conf/conf.c

	src/mod/mod.c

	src/mem/mem.c
	src/mem/secure.c

	src/aes/stub.c
	src/aes/openssl/aes.c
)
set(RE_SRC_POSIX
	src/net/posix/pif.c
	src/net/ifaddrs.c
    src/mod/dl.c
	src/lock/lock.c
	src/lock/rwlock.c
)
set(RE_SRC_OSX
	src/net/bsd/brt.c
    src/aes/apple/aes.c
    src/hmac/apple/hmac.c
	src/dns/darwin/srv.c
    src/udp/udp.c
    src/dns/res.c
    src/tcp/tcp.c
)
set(RE_SRC_WIN
	src/net/win32/wif.c
	src/lock/win32/lock.c
    src/mqueue/mqueue.c
    src/mqueue/win32/pipe.c
	src/dns/win32/srv.c
	src/mod/win32/dll.c
    src/udp/udp.c
    src/tcp/tcp.c
)
set(RE_SRC_LIN
	src/net/linux/rt.c
	src/main/epoll.c
    src/udp/udp.c
    src/dns/res.c
    src/tcp/tcp.c
)
set(RE_SRC_PS5
    src/lock/lock.c
	src/lock/rwlock.c
	src/udp/udp_ps5.c
	src/tcp/tcp_ps5.c
)

#
# defines - crucial to the operation of re

# common
set(RE_DEFINES HAVE_STDBOOL_H HAVE_SELECT)
set(RE_LIBRARIES)

if (NOT PS5)
    list(APPEND RE_DEFINES USE_OPENSSL_SRTP USE_OPENSSL_DTLS)
endif()

# posix
if (UNIX)
    list(APPEND RE_LIBRARIES resolv)
	list(APPEND RE_DEFINES
		HAVE_ATOMIC
		HAVE_SIGNAL
		HAVE_PTHREAD
		HAVE_RESOLV
		HAVE_UNISTD_H
		HAVE_INET_NTOP
		HAVE_INET_PTON
		HAVE_SYS_TIME_H
		HAVE_INTTYPES_H
		HAVE_STRINGS_H
		HAVE_STRERROR_R
		HAVE_GETOPT
		HAVE_NET_ROUTE_H
		HAVE_ROUTE_LIST
		HAVE_SYS_SYSCTL_H
		HAVE_SYSLOG
		HAVE_FORK
		HAVE_SETRLIMIT
		HAVE_UNAME
		HAVE_SELECT_H
	)
    set(RE_LIBRARIES)
endif()

# osx
if (APPLE)
    list(APPEND RE_DEFINES _DARWIN_C_SOURCE DARWIN USE_ZLIB HAVE_STRINGS_H HAVE_KQUEUE)
	set(RE_SRC ${RE_SRC} ${RE_SRC_OSX} ${RE_SRC_POSIX})

# linux
elseif (UNIX)
    list(APPEND RE_DEFINES LINUX HAVE_EPOLL HAVE_POLL)
	set(RE_SRC ${RE_SRC} ${RE_SRC_LIN} ${RE_SRC_POSIX})

# windows
elseif (WIN32)
    list(APPEND RE_DEFINES WIN32 _CONSOLE _CRT_SECURE_NO_DEPRECATE HAVE_IO_H USE_TLS HAVE_INTTYPES_H)
    list(APPEND RE_LIBRARIES Iphlpapi.lib)
	set(RE_SRC ${RE_SRC} ${RE_SRC_WIN})

elseif (PS5)
    list(APPEND RE_DEFINES LINUX HAVE_SELECT_H HAVE_UNISTD_H HAVE_SYS_TIME_H)
	list(APPEND RE_LIBRARIES libScePosix_stub_weak.a libSceNetCtl_stub_weak.a)
	set(RE_SRC ${RE_SRC} ${RE_SRC_PS5})

# um no ...
else()
	message(FATAL_ERROR "minre: unidentified build platform")
endif()

#
# library target

add_library(re ${RE_SRC})
target_compile_definitions(re PRIVATE
	${RE_DEFINES}
)
target_include_directories(re PRIVATE
	include
    ${OPENSSL_INCLUDE_DIR}
)
target_link_libraries(re
	${OPENSSL_LIBRARIES}
	${APPLE_CORE}
	${APPLE_SYSCONFIG}
    ${RE_LIBRARIES}
)

# push these values up to the parent scope, in case we're the child
set(RE_DEFINES ${RE_DEFINES} PARENT_SCOPE)
set(RE_INCLUDE_DIRS ${RE_INCLUDE_DIRS} PARENT_SCOPE)
set(RE_LIBRARIES ${RE_LIBRARIES} PARENT_SCOPE)
