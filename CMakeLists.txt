cmake_minimum_required(VERSION 3.15)
project(rawr LANGUAGES C)
enable_testing()

#
# setup

set(RAWR_INCLUDE_DIRS include)
set(RAWR_LINK_LIBS)

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

# aws-c-common
if (RAWR_AWSCOMMON)
	add_subdirectory(deps/aws-c-common)
	list(APPEND RAWR_LINK_LIBS aws-c-common)
	list(APPEND RAWR_INCLUDE_DIRS
		deps/aws-c-common/include
	)
endif()

# libuv
if (RAWR_LIBUV)
	add_subdirectory(deps/libuv)
	list(APPEND RAWR_LINK_LIBS uv)
	list(APPEND RAWR_INCLUDE_DIRS
		deps/libuv/include
	)
endif()

# mn
if (RAWR_MN)
	add_subdirectory(deps/mn)
	list(APPEND RAWR_LINK_LIBS mn)
	list(APPEND RAWR_INCLUDE_DIRS
		deps/mn/include
	)
endif()

# opus
if (RAWR_OPUS)
	add_subdirectory(deps/opus)
	list(APPEND RAWR_LINK_LIBS opus)
	list(APPEND RAWR_INCLUDE_DIRS
		deps/opus/include
	)
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
	list(APPEND RAWR_INCLUDE_DIRS 
		deps/opus/include
	)
endif()

# libsrtp
if (RAWR_LIBSRTP)
	add_subdirectory(deps/libsrtp)
	list(APPEND RAWR_LINK_LIBS srtp2)
	list(APPEND RAWR_INCLUDE_DIRS 
		deps/libsrtp/include
		deps/libsrtp/test
	)
endif()

# libstrophe-xep
if (RAWR_STROPHE)
	add_subdirectory(deps/libstrophe-xep)
endif()

#
# library

macro(rawr_add_library rname)
    add_library(${rname} ${ARGN})
    target_link_libraries(${rname}
	    ${RAWR_LINK_LIBS}
    )
    target_include_directories(${rname} PRIVATE
		${RAWR_INCLUDE_DIRS}
    )
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
	target_link_libraries(${rname}
		rawr
        ${RAWR_LINK_LIBS}
    )
    target_include_directories(${rname} PRIVATE
	    ${RAWR_INCLUDE_DIRS}
	)
	
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
if (TARGET srtp2)
    rawr_add_executable(playground_srtp_send
        src/playground/srtp/rtp.c
        src/playground/srtp/util.c
        src/playground/srtp/playground_srtp.c
        src/playground/srtp/playground_srtp_send.c
    )
    target_include_directories(playground_srtp_send PRIVATE ${CMAKE_BINARY_DIR}/deps/libsrtp)

    rawr_add_executable(playground_srtp_recv
        src/playground/srtp/rtp.c
        src/playground/srtp/util.c
        src/playground/srtp/playground_srtp.c
        src/playground/srtp/playground_srtp_recv.c
    )
    target_include_directories(playground_srtp_recv PRIVATE ${CMAKE_BINARY_DIR}/deps/libsrtp)
endif()

# playground: strophe
if (TARGET strophe)
    rawr_add_executable(playground_strophe src/playground/strophe/playground_strophe.c)
endif()

#
# cmake target

add_custom_target(CMake SOURCES
	CMakeLists.txt
	cmake/rawr_folders.cmake
)

include(cmake/rawr_folders.cmake)
