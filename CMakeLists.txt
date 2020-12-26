cmake_minimum_required(VERSION 3.15)
project(rawr LANGUAGES CXX)
enable_testing()

#
# setup

set(RAWR_INCLUDE_DIRS)
set(RAWR_LINK_LIBS)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

if (NOT WIN32)
	set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
	find_package(OpenSSL REQUIRED)
endif()

#
# dependencies

# aws-c-common
add_subdirectory(deps/aws-c-common)
list(APPEND RAWR_LINK_LIBS aws-c-common)
list(APPEND RAWR_INCLUDE_DIRS
	deps/aws-c-common/include
)

# libuv
add_subdirectory(deps/libuv)
list(APPEND RAWR_LINK_LIBS libuv)
list(APPEND RAWR_INCLUDE_DIRS
	deps/libuv/include
)

# mn
add_subdirectory(deps/mn)
list(APPEND RAWR_LINK_LIBS mn)
list(APPEND RAWR_INCLUDE_DIRS
	deps/mn/include
)

# opus
add_subdirectory(deps/opus)
list(APPEND RAWR_LINK_LIBS opus)
list(APPEND RAWR_INCLUDE_DIRS
	deps/opus/include
)

# portaudio
set(PA_BUILD_SHARED ON CACHE INTERNAL "")
set(PA_BUILD_STATIC ON CACHE INTERNAL "")
set(PA_DISABLE_INSTALL ON CACHE INTERNAL "")
set(PA_BUILD_TESTS ON CACHE INTERNAL "")
set(PA_BUILD_EXAMPLES ON CACHE INTERNAL "")
add_subdirectory(deps/portaudio)

# libsrtp
add_subdirectory(deps/libsrtp)

# libstrophe-xep
add_subdirectory(deps/libstrophe-xep)

#
# library

macro(rawr_add_library rname)
    add_library(${rname} ${ARGN})
    target_link_libraries(${rname}
	    mn
        portaudio
    )
    target_include_directories(${rname} PRIVATE
	    include
	    deps/mn/include
        deps/portaudio/include
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
    )
    target_include_directories(${rname} PRIVATE
	    include
	    deps/mn/include
        deps/portaudio/include
    )
    add_custom_command(TARGET ${rname} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:portaudio> $<TARGET_FILE_DIR:${rname}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:uv> $<TARGET_FILE_DIR:${rname}>
    )
endmacro()

rawr_add_executable(playground_ui src/playground_ui.c)

rawr_add_executable(playground_pa src/playground_pa.c)

#
# cmake target

add_custom_target(CMake SOURCES
	CMakeLists.txt
	cmake/rawr_folders.cmake
)

include(cmake/rawr_folders.cmake)
