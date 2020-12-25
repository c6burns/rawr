cmake_minimum_required(VERSION 3.15)
project(strophe)

# libxml2 - dependency
set(LIBXML2_WITH_ICONV OFF CACHE INTERNAL "")
set(LIBXML2_WITH_LZMA OFF CACHE INTERNAL "")
set(LIBXML2_WITH_ZLIB OFF CACHE INTERNAL "")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libxml2 ${CMAKE_CURRENT_BINARY_DIR}/libxml2)

set(STROPHE_SRC
    src/auth.c
	src/conn.c
	src/crypto.c
	src/ctx.c
	src/event.c
	src/handler.c
	src/hash.c
	src/jid.c
	src/md5.c
    src/parser_libxml2.c
	src/rand.c
	src/resolver.c
	src/sasl.c
	src/scram.c
	src/sha1.c
	src/sha256.c
	src/sha512.c
	src/snprintf.c
	src/sock.c
	src/stanza.c
	src/util.c
	src/uuid.c
    src/common.h
	src/hash.h
	src/md5.h
	src/ostypes.h
	src/parser.h
	src/rand.h
	src/resolver.h
	src/sasl.h
	src/scram.h
	src/sha.h
	src/sha1.h
	src/sha256.h
	src/sha512.h
	src/snprintf.h
	src/sock.h
	src/tls.h
	src/util.h
)

if (WIN32)
	# secure channels on windows
    set(STROPHE_SRC_TLS src/tls_schannel.c)
else()
	# openssl everywhere else
    set(STROPHE_SRC_TLS src/tls_openssl.c)
endif()


add_library(strophe ${STROPHE_SRC} ${STROPHE_SRC_TLS} ${STROPHE_SRC_XML})
target_link_libraries(strophe LibXml2)
target_include_directories(strophe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

macro(sq_add_executable name sources)
	add_executable(${name} ${sources})
	target_link_libraries(${name} strophe)
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
endmacro()

sq_add_executable(active examples/active.c)
sq_add_executable(basic examples/basic.c)
sq_add_executable(bot examples/bot.c)
sq_add_executable(component examples/component.c)
sq_add_executable(roster examples/roster.c)
sq_add_executable(uuid examples/uuid.c)
sq_add_executable(register examples/register.c)
if (NOT WIN32)
    sq_add_executable(vcard examples/vcard.c)
endif()

