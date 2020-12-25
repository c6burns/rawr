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
	src/rawr_term.h
	src/rawr_term.c
)

#
#
# folder organization

# aws-c-common
set_target_properties(aws-c-common PROPERTIES FOLDER "deps")
set_target_properties(aws-c-common-assert-tests PROPERTIES FOLDER "deps")
set_target_properties(aws-c-common-tests PROPERTIES FOLDER "deps")

# mn
set_target_properties(mn PROPERTIES FOLDER "deps")
set_target_properties(mn-tests PROPERTIES FOLDER "tests/mn")

# libxml2
set_target_properties(LibXml2 PROPERTIES FOLDER "deps")
set_target_properties(runsuite PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(runtest PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(runxmlconf PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testapi PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testAutomata PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testC14N PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testchar PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testdict PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testHTML PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testlimits PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testModule PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testReader PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testrecurse PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testRegexp PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testRelax PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testSAX PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testSchemas PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testThreads PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testURI PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(testXPath PROPERTIES FOLDER "tests/LibXml2")
set_target_properties(LibXml2Mod PROPERTIES FOLDER "extras/LibXml2")
set_target_properties(xmlcatalog PROPERTIES FOLDER "examples/LibXml2")
set_target_properties(xmllint PROPERTIES FOLDER "examples/LibXml2")

# opus
set_target_properties(opus PROPERTIES FOLDER "deps")
set_target_properties(test_opus_api PROPERTIES FOLDER "tests/opus")
set_target_properties(test_opus_decode PROPERTIES FOLDER "tests/opus")
set_target_properties(test_opus_encode PROPERTIES FOLDER "tests/opus")
set_target_properties(test_opus_padding PROPERTIES FOLDER "tests/opus")

# strophe
set_target_properties(strophe PROPERTIES FOLDER "deps")
set_target_properties(strophe-xep PROPERTIES FOLDER "deps")
set_target_properties(active PROPERTIES FOLDER "examples/strophe")
set_target_properties(basic PROPERTIES FOLDER "examples/strophe")
set_target_properties(bot PROPERTIES FOLDER "examples/strophe")
set_target_properties(component PROPERTIES FOLDER "examples/strophe")
set_target_properties(register PROPERTIES FOLDER "examples/strophe")
set_target_properties(roster PROPERTIES FOLDER "examples/strophe")
set_target_properties(uuid PROPERTIES FOLDER "examples/strophe")

# portaudio
set_target_properties(portaudio PROPERTIES FOLDER "deps")
set_target_properties(portaudio_static PROPERTIES FOLDER "extras/portaudio")
set_target_properties(patest_longsine PROPERTIES FOLDER "tests/portaudio")
set_target_properties(pa_devs PROPERTIES FOLDER "examples/portaudio")
set_target_properties(pa_fuzz PROPERTIES FOLDER "examples/portaudio")
set_target_properties(paex_ocean_shore PROPERTIES FOLDER "examples/portaudio")
set_target_properties(paex_pink PROPERTIES FOLDER "examples/portaudio")
set_target_properties(paex_read_write_wire PROPERTIES FOLDER "examples/portaudio")
set_target_properties(paex_record PROPERTIES FOLDER "examples/portaudio")
set_target_properties(paex_record_file PROPERTIES FOLDER "examples/portaudio")
set_target_properties(paex_saw PROPERTIES FOLDER "examples/portaudio")
set_target_properties(paex_sine PROPERTIES FOLDER "examples/portaudio")
set_target_properties(paex_write_sine PROPERTIES FOLDER "examples/portaudio")
set_target_properties(paex_write_sine_nonint PROPERTIES FOLDER "examples/portaudio")
set_target_properties(paex_sine_c++ PROPERTIES FOLDER "examples/portaudio")
if (WIN32)
	set_target_properties(paex_wmme_ac3 PROPERTIES FOLDER "examples/portaudio")
	set_target_properties(paex_wmme_surround PROPERTIES FOLDER "examples/portaudio")
endif()