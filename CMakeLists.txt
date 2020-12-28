cmake_minimum_required(VERSION 3.15)
project(rawr LANGUAGES C)
include(ExternalProject)
enable_testing()

#
# setup

set(RAWR_INCLUDE_DIRS include)
set(RAWR_LINK_LIBS)

set(RAWR_MBEDTLS ON CACHE BOOL "Builds and links mbedtls as a dependency")
set(RAWR_LWS ON CACHE BOOL "Builds and links libwebsockets as a dependency")
set(RAWR_PORTAUDIO ON CACHE BOOL "Builds and links portaudio as a dependency")
set(RAWR_LIBSRTP ON CACHE BOOL "Builds and links libsrtp as a dependency")
set(RAWR_AWSCOMMON ON CACHE BOOL "Builds and links aws-c-common as a dependency")
set(RAWR_LIBUV ON CACHE BOOL "Builds and links libuv as a dependency")
set(RAWR_MN ON CACHE BOOL "Builds and links libmn as a dependency")
set(RAWR_OPUS ON CACHE BOOL "Builds and links opus as a dependency")
set(RAWR_STROPHE ON CACHE BOOL "Builds and links libstrophe as a dependency")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

if (NOT WIN32)
	set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
	find_package(OpenSSL REQUIRED)
endif()

#
# dependencies

# mbedtls
if (RAWR_MBEDTLS)
    ExternalProject_Add(mbedtls
	    GIT_REPOSITORY    https://github.com/ARMmbed/mbedtls.git
	    GIT_TAG           v2.25.0
        PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/deps
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/deps/install
            -DENABLE_TESTING=0
            -DENABLE_ZLIB_SUPPORT=0
            -DENABLE_PROGRAMS=0
	    TEST_COMMAND      ""
    )
    set(RAWR_MBEDTLS_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/deps/install/include)
    set(RAWR_EXT_DIR ${CMAKE_CURRENT_BINARY_DIR}/deps/install/lib)
    set(RAWR_EXT_CRYPTO ${CMAKE_STATIC_LIBRARY_PREFIX}mbedcrypto${CMAKE_STATIC_LIBRARY_SUFFIX})
    set(RAWR_EXT_TLS ${CMAKE_STATIC_LIBRARY_PREFIX}mbedtls${CMAKE_STATIC_LIBRARY_SUFFIX})
    set(RAWR_EXT_X509 ${CMAKE_STATIC_LIBRARY_PREFIX}mbedx509${CMAKE_STATIC_LIBRARY_SUFFIX})
    list(APPEND RAWR_MBEDTLS_LIBRARIES ${RAWR_EXT_DIR}/${RAWR_EXT_CRYPTO})
    list(APPEND RAWR_MBEDTLS_LIBRARIES ${RAWR_EXT_DIR}/${RAWR_EXT_TLS})
    list(APPEND RAWR_MBEDTLS_LIBRARIES ${RAWR_EXT_DIR}/${RAWR_EXT_X509})

    list(APPEND RAWR_INCLUDE_DIRS ${RAWR_MBEDTLS_INCLUDE_DIRS})
    list(APPEND RAWR_LINK_LIBS ${RAWR_MBEDTLS_LIBRARIES})

    message(${RAWR_MBEDTLS_LIBRARIES})
    message(${RAWR_MBEDTLS_INCLUDE_DIRS})
endif()

# libwebsockets
if (RAWR_LWS)
    set(EXT_NAME libwebsockets)
    ExternalProject_Add(libwebsockets
        DEPENDS           mbedtls
        GIT_REPOSITORY    https://github.com/warmcat/libwebsockets.git
        GIT_TAG           v3.2.3
        PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/deps
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/deps/install
            -DLWS_WITH_HTTP2=1
            -DLWS_HAVE_HMAC_CTX_new=1
            -DLWS_HAVE_SSL_EXTRA_CHAIN_CERTS=1
            -DLWS_HAVE_OPENSSL_ECDH_H=1
            -DLWS_HAVE_EVP_MD_CTX_free=1
            -DLWS_WITHOUT_SERVER=1
            -DLWS_WITHOUT_TESTAPPS=1
            -DLWS_WITH_THREADPOOL=1
            -DLWS_WITHOUT_TEST_SERVER_EXTPOLL=1
            -DLWS_WITHOUT_TEST_PING=1
            -DLWS_WITHOUT_TEST_CLIENT=1
            -DLWS_WITH_STATIC=1
            -DLWS_WITH_SHARED=0
            -DLWS_STATIC_PIC=1
            -DLWS_WITH_ZLIB=0
            -DLWS_HAVE_EVENTFD=0
            -DLWS_WITH_MBEDTLS=1
            -DLWS_MBEDTLS_LIBRARIES=${RAWR_MBEDTLS_LIBRARIES}
            -DLWS_MBEDTLS_INCLUDE_DIRS=${RAWR_MBEDTLS_INCLUDE_DIRS}
            -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        TEST_COMMAND ""
    )
    list(APPEND RAWR_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/deps/install/include)
    set(RAWR_EXT_DIR ${CMAKE_CURRENT_BINARY_DIR}/deps/install/lib)
    set(RAWR_EXT_FILE ${CMAKE_STATIC_LIBRARY_PREFIX}websockets${CMAKE_STATIC_LIBRARY_SUFFIX})
    list(APPEND RAWR_LINK_LIBS ${RAWR_EXT_DIR}/${RAWR_EXT_FILE})
endif()

# aws-c-common
if (RAWR_AWSCOMMON)
	add_subdirectory(deps/aws-c-common)
	list(APPEND RAWR_LINK_LIBS aws-c-common)
	list(APPEND RAWR_INCLUDE_DIRS deps/aws-c-common/include)
endif()

# libuv
if (RAWR_LIBUV)
	add_subdirectory(deps/libuv)
	list(APPEND RAWR_LINK_LIBS uv)
	list(APPEND RAWR_INCLUDE_DIRS deps/libuv/include)
endif()

# mn
if (RAWR_MN)
	add_subdirectory(deps/mn)
	list(APPEND RAWR_LINK_LIBS mn)
	list(APPEND RAWR_INCLUDE_DIRS deps/mn/include)
endif()

# opus
if (RAWR_OPUS)
	add_subdirectory(deps/opus)
	list(APPEND RAWR_LINK_LIBS opus)
	list(APPEND RAWR_INCLUDE_DIRS deps/opus/include)
endif()

# portaudio
if (RAWR_PORTAUDIO)
	set(PA_BUILD_SHARED ON CACHE INTERNAL "")
	set(PA_BUILD_STATIC ON CACHE INTERNAL "")
	set(PA_DISABLE_INSTALL ON CACHE INTERNAL "")
	set(PA_BUILD_TESTS ON CACHE INTERNAL "")
	set(PA_BUILD_EXAMPLES ON CACHE INTERNAL "")
	add_subdirectory(deps/portaudio)
	list(APPEND RAWR_LINK_LIBS portaudio)
	list(APPEND RAWR_INCLUDE_DIRS deps/opus/include)
endif()

# libsrtp
if (RAWR_LIBSRTP)
	add_subdirectory(deps/libsrtp)
	list(APPEND RAWR_LINK_LIBS srtp2)
	list(APPEND RAWR_INCLUDE_DIRS deps/libsrtp/include deps/libsrtp/test ${CMAKE_BINARY_DIR}/deps/libsrtp)
endif()

# libstrophe-xep
if (RAWR_STROPHE)
	add_subdirectory(deps/libstrophe-xep)
    list(APPEND RAWR_LINK_LIBS strophe)
	list(APPEND RAWR_INCLUDE_DIRS deps/libstrophe)
endif()

#
# library

macro(rawr_add_library rname)
    add_library(${rname} ${ARGN})
    target_link_libraries(${rname} ${RAWR_LINK_LIBS})
    target_include_directories(${rname} PRIVATE ${RAWR_INCLUDE_DIRS})
endmacro()

rawr_add_library(rawr
	include/rawr/app.h
	include/rawr/buf.h
	include/rawr/pa.h
	include/rawr/term.h
	src/app.c
	src/buf.c
	src/pa.c
	src/term.c
)

#
# executable

macro(rawr_add_executable rname)
    add_executable(${rname} ${ARGN})
	target_link_libraries(${rname} rawr ${RAWR_LINK_LIBS})
    target_include_directories(${rname} PRIVATE ${RAWR_INCLUDE_DIRS})
	
	if (TARGET portaudio)
		add_custom_command(TARGET ${rname} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:portaudio> $<TARGET_FILE_DIR:${rname}>
		)
	endif()
	
	if (TARGET uv)
		add_custom_command(TARGET ${rname} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:uv> $<TARGET_FILE_DIR:${rname}>
		)
	endif()
endmacro()

# playground: user interface
if (TARGET mn)
    rawr_add_executable(playground_ui src/playground/ui/playground_ui.c)
endif()

# playground: portaudio + opus
if (TARGET portaudio AND TARGET opus)
    rawr_add_executable(playground_pa src/playground/pa/playground_pa.c)
endif()

# playground: srtp
if (TARGET portaudio AND TARGET opus AND TARGET srtp2)
	set(RAWR_SRTP_SRC_COMMON
		src/playground/srtp/rtp.c
        src/playground/srtp/util.c
        src/playground/srtp/playground_srtp.c
	)

	rawr_add_executable(playground_srtp_sendrecv
        ${RAWR_SRTP_SRC_COMMON}
        src/playground/srtp/playground_srtp_sendrecv.c
    )
    target_include_directories(playground_srtp_sendrecv PRIVATE ${CMAKE_BINARY_DIR}/deps/libsrtp)

    rawr_add_executable(playground_srtp_send
        ${RAWR_SRTP_SRC_COMMON}
        src/playground/srtp/playground_srtp_send.c
    )
    target_include_directories(playground_srtp_send PRIVATE ${CMAKE_BINARY_DIR}/deps/libsrtp)

    rawr_add_executable(playground_srtp_recv
        ${RAWR_SRTP_SRC_COMMON}
        src/playground/srtp/playground_srtp_recv.c
    )
    target_include_directories(playground_srtp_recv PRIVATE ${CMAKE_BINARY_DIR}/deps/libsrtp)
	
	if (WIN32)
		target_compile_definitions(playground_srtp_sendrecv PRIVATE HAVE_WINSOCK2_H)
		target_compile_definitions(playground_srtp_send PRIVATE HAVE_WINSOCK2_H)
		target_compile_definitions(playground_srtp_recv PRIVATE HAVE_WINSOCK2_H)
	endif()
endif()

# playground: strophe
if (TARGET strophe)
    rawr_add_executable(playground_strophe src/playground/strophe/playground_strophe.c)
endif()

# playground: libwebsockets
if (TARGET libwebsockets)
	rawr_add_executable(playground_lws src/playground/lws/playground_lws.c)
	add_dependencies(playground_lws libwebsockets)
endif()

#
# cmake target

add_custom_target(CMake SOURCES
	CMakeLists.txt
	cmake/rawr_folders.cmake
)

include(cmake/rawr_folders.cmake)
