project (rlib)

cmake_minimum_required(VERSION 2.8)

## use C++11 features
add_definitions(-std=c++14)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_COMPILER /usr/bin/g++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

## path to ralloc lib, which you need to build manually
set(RALLOC_LIB "${PROJECT_SOURCE_DIR}/lib/libssmalloc.a")

## gflags
set(gflags_DIR "${CMAKE_SOURCE_DIR}/examples/gflags")
add_subdirectory(${gflags_DIR})
include_directories(${gflags_DIR}/include)
set_property(DIRECTORY ${gflags_DIR} PROPERTY COMPILE_DEFINITIONS STRIP_FLAG_HELP=1)

## include paths
include_directories(ralloc)

include( ExternalProject )
set(SSMALLOC_INSTALL_DIR ${CMAKE_SOURCE_DIR}/)
ExternalProject_Add(ralloc
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/ralloc
  CONFIGURE_COMMAND mkdir -p  ${SSMALLOC_INSTALL_DIR}/lib
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
  INSTALL_COMMAND make install
)

SET(TESTS
    rnic
    simple_connect
    qp
    pclient
    pserver
    )

add_executable(rnic "examples/rnic.cc")
add_executable(simple_connect "examples/simple_connect.cc")
add_executable(qp "examples/qp_test.cc")
add_executable(pclient "examples/rc_pingpong/client.cc")
add_executable(pserver "examples/rc_pingpong/server.cc")

FOREACH(TEST ${TESTS}) 
             add_dependencies(${TEST} ralloc gflags)
             target_link_libraries(${TEST} -lpthread ibverbs ${RALLOC_LIB} ${gflags_DIR}/libgflags_nothreads.a)
ENDFOREACH()


