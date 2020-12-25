cmake_minimum_required(VERSION 3.15)
project(rawr)
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

# libstrophe-xep
add_subdirectory(deps/libstrophe-xep)

#
# executable

add_executable(rawr_term
	include/rawr/buf.h
	include/rawr/term.h
	src/buf.c
	src/term.c
)
target_link_libraries(rawr_term
	mn
)
target_include_directories(rawr_term PRIVATE
	include
	deps/mn/include
)

#
# cmake target

add_custom_target(CMake SOURCES
	CMakeLists.txt
	cmake/rawr_folders.cmake
)

include(cmake/rawr_folders.cmake)
