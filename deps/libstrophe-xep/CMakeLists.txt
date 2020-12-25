cmake_minimum_required(VERSION 3.15)
project(strophe-xep)

# libstrophe - dependency
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libstrophe ${CMAKE_CURRENT_BINARY_DIR}/libstrophe)

set(STROPHE_XEP_SRC
	include/wksxep0047.h
	source/wksxep0047.c
	include/xmpp_ibb.h
	source/xmpp_ibb.c
)

add_library(strophe-xep ${STROPHE_XEP_SRC})
target_link_libraries(strophe-xep strophe)
target_include_directories(strophe-xep PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/../libstrophe
	${CMAKE_CURRENT_SOURCE_DIR}/../libstrophe/src
	${CMAKE_CURRENT_SOURCE_DIR}/../mn/include
)

add_executable(strophe-xep_example example/main.c)
target_link_libraries(strophe-xep_example
	strophe
	strophe-xep
	mn
)
target_include_directories(strophe-xep_example PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/../libstrophe
	${CMAKE_CURRENT_SOURCE_DIR}/../libstrophe/src
	${CMAKE_CURRENT_SOURCE_DIR}/../mn/include
)
