cmake_minimum_required(VERSION 3.15)
project(mn LANGUAGES C)
include(CTest)

set(MN_BUILD_TESTING OFF CACHE BOOL "Builds associated tests")

#
# dependencies

# mn
set(MN_SRC
    src/allocator.c
    src/atomic.c
    src/buffer.c
    src/buffer_pool.c
    src/list_ptr.c
    src/log.c
    src/map.c
    src/mutex.c
    src/queue_spsc.c
    src/system.c
    src/thread.c
    src/time.c
    src/uuid.c
    include/mn/allocator.h
    include/mn/atomic.h
    include/mn/buffer.h
    include/mn/buffer_pool.h
    include/mn/error.h
    include/mn/list_ptr.h
    include/mn/log.h
    include/mn/map.h
    include/mn/mutex.h
    include/mn/queue_spsc.h
    include/mn/system.h
    include/mn/thread.h
    include/mn/time.h
    include/mn/uuid.h
)
add_library(mn ${MN_SRC})
target_link_libraries(mn aws-c-common)
target_include_directories(mn PRIVATE
    include
    ../aws-c-common/include
    ${CMAKE_BINARY_DIR}/deps/aws-c-common/generated/include
)

#
# tests

if (MN_BUILD_TESTING)

    # enable testing
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../aws-c-common/cmake)
    include(mn_testing)
    enable_testing()

    set(MN_TESTS_C
        tests/test_buffer.c
	    tests/test_list_ptr.c
	    tests/test_queue_spsc.c
    )
    set(TESTS ${MN_TESTS_C})

    mn_add_test_case(mn_buffer_ops)
    mn_add_test_case(mn_list_ptr_create)
    mn_add_test_case(mn_list_ptr_ops)
    mn_add_test_case(mn_queue_spsc_stress)
    mn_add_test_case(mn_queue_spsc_testempty)
    mn_add_test_case(mn_queue_spsc_testfull)
    mn_add_test_case(mn_queue_spsc_npot)

    mn_generate_test_driver(mn-tests mn)
    target_include_directories(mn-tests PRIVATE
        ../aws-c-common/include
        ${CMAKE_BINARY_DIR}/deps/aws-c-common/generated/include
    )

    # runner for single test for easy debug step through an impl issue
    # mn_test_single(test_devnet_route mn_devnet_route_count_add_increments_count mn)
endif()
