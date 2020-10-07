cmake_minimum_required(VERSION 3.2)

project(rtwo)

ADD_DEFINITIONS(-std=c++14)

## install dependies
include(deps/deps.cmake)

find_library(jemalloc_lib NAMES jemalloc PATHS ./deps/jemalloc PATH_SUFFIXES lib
               NO_DEFAULT_PATH)

if(NOT jemalloc_lib)

	set(jemalloc_lib "")

endif()

## tests
#add_subdirectory(tests)
include(tests/tests.cmake)

## examples
include(examples/examples.cmake)

enable_testing()

add_test(NAME test COMMAND tests/coretest)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose
                  DEPENDS tests/coretest )
