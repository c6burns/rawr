cmake_minimum_required(VERSION 3.15)
project(rawr LANGUAGES C)
include(ExternalProject)
enable_testing()

#
# setup

# detect PS5 toolchain
if (__COMPILER_PS5)
	set(PS5 ON)
	add_definitions(-DPS5)
    set(CMAKE_C_STANDARD 11)
endif()

set(RAWR_INCLUDE_DIRS include)
set(RAWR_LINK_LIBS)
if (NOT WIN32 AND NOT PS5)
	list(APPEND RAWR_LINK_LIBS resolv)
endif()

set(RAWR_MBEDTLS OFF CACHE BOOL "Builds and links mbedtls as a dependency")
set(RAWR_LWS OFF CACHE BOOL "Builds and links libwebsockets as a dependency")
set(RAWR_PORTAUDIO ON CACHE BOOL "Builds and links portaudio as a dependency")
set(RAWR_AWSCOMMON ON CACHE BOOL "Builds and links aws-c-common as a dependency")
set(RAWR_MN ON CACHE BOOL "Builds and links libmn as a dependency")
set(RAWR_OPUS ON CACHE BOOL "Builds and links opus as a dependency")
set(RAWR_RE ON CACHE BOOL "Builds and links libre as a dependency")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

if (NOT WIN32 AND NOT PS5)
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
endif()

# libwebsockets
if (RAWR_LWS)
    set(EXT_NAME libwebsockets)
    ExternalProject_Add(libwebsockets
        DEPENDS           mbedtls
        GIT_REPOSITORY    https://github.com/warmcat/libwebsockets.git
        GIT_TAG           v4.1.6
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
    set(RAWR_LWS_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/deps/install/include)
    list(APPEND RAWR_INCLUDE_DIRS ${RAWR_LWS_INCLUDE_DIRS})
    set(RAWR_EXT_DIR ${CMAKE_CURRENT_BINARY_DIR}/deps/install/lib)
    set(RAWR_EXT_LIBNAME websockets)
    if (WIN32)
        set(RAWR_EXT_LIBNAME websockets_static)
    endif()
    set(RAWR_EXT_LIBFILE ${CMAKE_STATIC_LIBRARY_PREFIX}${RAWR_EXT_LIBNAME}${CMAKE_STATIC_LIBRARY_SUFFIX})
    list(APPEND RAWR_LWS_LIBRARIES ${RAWR_EXT_DIR}/${RAWR_EXT_LIBFILE})
endif()

# aws-c-common
if (RAWR_AWSCOMMON)
	add_subdirectory(deps/aws-c-common)
	list(APPEND RAWR_LINK_LIBS aws-c-common)
	list(APPEND RAWR_INCLUDE_DIRS deps/aws-c-common/include)
endif()

# mn
if (RAWR_MN)
	add_subdirectory(deps/mn)
	list(APPEND RAWR_LINK_LIBS mn)
	list(APPEND RAWR_INCLUDE_DIRS deps/mn/include)
endif()

# opus
if (RAWR_OPUS)
    set(OPUS_BUILD_TESTING OFF CACHE INTERNAL "")
    set(BUILD_TESTING OFF CACHE INTERNAL "")
    set(OPUS_BUILD_PROGRAMS OFF CACHE INTERNAL "")
	add_subdirectory(deps/opus)
	list(APPEND RAWR_LINK_LIBS opus)
	list(APPEND RAWR_INCLUDE_DIRS deps/opus/include)
endif()

# portaudio
if (RAWR_PORTAUDIO AND NOT PS5)
	set(PA_BUILD_SHARED ON CACHE INTERNAL "")
	set(PA_BUILD_STATIC ON CACHE INTERNAL "")
	set(PA_DISABLE_INSTALL ON CACHE INTERNAL "")
	set(PA_BUILD_TESTS OFF CACHE INTERNAL "")
	set(PA_BUILD_EXAMPLES OFF CACHE INTERNAL "")
	add_subdirectory(deps/portaudio)
	list(APPEND RAWR_LINK_LIBS portaudio_static)
	list(APPEND RAWR_INCLUDE_DIRS deps/opus/include)
endif()

# re
if (RAWR_RE)
	add_subdirectory(deps/re)
	list(APPEND RAWR_LINK_LIBS re)
	list(APPEND RAWR_INCLUDE_DIRS deps/re/include)
endif()

set(RAWR_SRC
    include/rawr/Audio.h
    include/rawr/Call.h
    include/rawr/Endpoint.h
    include/rawr/Net.h
    include/rawr/Codec.h
    include/rawr/MemoryBarrier.h
    include/rawr/RingBuffer.h
    include/rawr/Stun.h
    include/rawr/Util.h
    src/Call.c
    src/Endpoint.c
    src/Net.c
    src/Codec.c
    src/RingBuffer.c
    src/Stun.c
    src/Util.c
)

if (PS5)
    set(RAWR_SRC ${RAWR_SRC}
        src/AudioPS5.c
    )
    list(APPEND RAWR_LINK_LIBS
        libSceAudioIn_stub_weak.a
        libSceAudioOut_stub_weak.a
        libSceAudioOut2_stub_weak.a
        libSceNetCtl_stub_weak.a
        libSceNet_stub_weak.a
        libScePosix_stub_weak.a
        libSceUserService_stub_weak.a
    )
else()
    set(RAWR_SRC ${RAWR_SRC}
        src/AudioPA.c
    )
endif()

#
# dynamic shared object

add_library(rawrDSO MODULE src/dso/dso.c)
target_compile_definitions(rawrDSO PRIVATE RAWR_EXPORTS)
target_link_libraries(rawrDSO ${RAWR_LINK_LIBS})
target_include_directories(rawrDSO PRIVATE ${RAWR_INCLUDE_DIRS})

#
# library

macro(rawr_add_library rname)
    add_library(${rname} ${ARGN})
    target_link_libraries(${rname} ${RAWR_LINK_LIBS})
    target_include_directories(${rname} PRIVATE ${RAWR_INCLUDE_DIRS})
endmacro()

rawr_add_library(rawr ${RAWR_SRC})


#
# executable

macro(rawr_add_executable rname)
    add_executable(${rname} ${ARGN})
    target_compile_definitions(${rname} PRIVATE ${RE_DEFINES})
	target_link_libraries(${rname} rawr ${RAWR_LINK_LIBS})
    target_include_directories(${rname} PRIVATE ${RAWR_INCLUDE_DIRS})
endmacro()

# playground: portaudio + opus
if (TARGET portaudio AND TARGET opus)
    rawr_add_executable(playground_pa src/playground/pa/playground_pa.c)
endif()

if (TARGET opus AND TARGET re AND TARGET mn)
    rawr_add_executable(rawr_demo src/playground/call/playground_call.c res/rawr.rc)
endif()

# playground: portaudio
if (TARGET portaudio)
    rawr_add_executable(playground_pa_callbacks src/playground/pa/playground_pa_callbacks.c)
    target_link_libraries(playground_pa_callbacks portaudio_static)
endif()

# playground: libwebsockets
if (TARGET libwebsockets)
	rawr_add_executable(playground_lws src/playground/lws/playground_lws.c)
	add_dependencies(playground_lws libwebsockets)
    target_link_libraries(playground_lws ${RAWR_LWS_LIBRARIES} ${RAWR_MBEDTLS_LIBRARIES})
    add_custom_command(TARGET playground_lws POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/cmake/AmazonRootCA1.cer $<TARGET_FILE_DIR:playground_lws>
    )
endif()

# playground: re
if (TARGET re)
	rawr_add_executable(playground_re_sip src/playground/re/playground_re_sip.c)
    rawr_add_executable(playground_ice src/playground/re/playground_ice.c)
    rawr_add_executable(playground_stun src/playground/re/playground_stun.c)
endif()

if (PS5)
    rawr_add_executable(playground_ps5 src/playground/ps5/playground_ps5.c)
endif()

#
# cmake target

add_custom_target(CMake SOURCES
	CMakeLists.txt
	cmake/rawr_folders.cmake
)

include(cmake/rawr_folders.cmake)
