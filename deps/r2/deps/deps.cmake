cmake_minimum_required(VERSION 3.2)

include( ExternalProject )

## install & compile RLib's rmalloc
set(SSMALLOC_INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/rlib)
ExternalProject_Add(ralloc
  SOURCE_DIR ${SSMALLOC_INSTALL_DIR}/ralloc
  CONFIGURE_COMMAND mkdir -p  ${SSMALLOC_INSTALL_DIR}/lib
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
  INSTALL_COMMAND make install
)
set(LIBSSMALLOC_LIBRARIES ${SSMALLOC_INSTALL_DIR}/lib )

set(jemalloc_INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/jemalloc)
ExternalProject_Add(jemalloc
	       SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/jemalloc
               CONFIGURE_COMMAND ./configure --with-jemalloc-prefix=je --prefix=${jemalloc_INSTALL_DIR}
               BUILD_COMMAND make -j12
               BUILD_IN_SOURCE 1
               INSTALL_COMMAND "")
include_directories(./deps/jemalloc/include)

include_directories(BEFORE ${CMAKE_SOURCE_DIR}/deps)

## install boost
set( BOOST_INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/boost )
ExternalProject_Add(libboost1.61
#  URL $ENV{HOME}/download/boost_1_61_0.tar.bz2
  URL https://sourceforge.net/projects/boost/files/boost/1.61.0/boost_1_61_0.tar.gz
#  URL_HASH SHA256=a547bd06c2fd9a71ba1d169d9cf0339da7ebf4753849a8f7d6fdb8feee99b640
  CONFIGURE_COMMAND ./bootstrap.sh --prefix=${BOOST_INSTALL_DIR} --with-libraries=system,coroutine
  BUILD_COMMAND ./b2
  BUILD_IN_SOURCE 1
  INSTALL_COMMAND ./b2 install
)
set(LIBBOOST_HEADERS ${BOOST_INSTALL_DIR}/include )
set(LIBBOOST_LIBRARIES ${BOOST_INSTALL_DIR}/lib )
include_directories(BEFORE ${LIBBOOST_HEADERS} )

## install gflags
set(gflags_DIR "${CMAKE_SOURCE_DIR}/deps/gflags")
add_subdirectory(${gflags_DIR})
include_directories(${gflags_DIR}/include)
set_property(DIRECTORY ${gflags_DIR} PROPERTY COMPILE_DEFINITIONS STRIP_FLAG_HELP=1)

## gtest
set(gtest_DIR "${CMAKE_SOURCE_DIR}/deps/googletest")
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/googletest)
include_directories(${CMAKE_SOURCE_DIR}/deps/googletest/googletest/include)

## find other libs
find_library(LIBIBVERBS NAMES ibverbs)

  add_library( ssmalloc STATIC IMPORTED )
  set_target_properties( ssmalloc PROPERTIES
    IMPORTED_LOCATION ${LIBSSMALLOC_LIBRARIES}/libssmalloc.a
    )
  add_library( boost_system STATIC IMPORTED )
  set_target_properties( boost_system PROPERTIES
    IMPORTED_LOCATION ${LIBBOOST_LIBRARIES}/libboost_system.a
    )
  add_library( boost_coroutine STATIC IMPORTED )
  set_target_properties( boost_coroutine PROPERTIES
    IMPORTED_LOCATION ${LIBBOOST_LIBRARIES}/libboost_coroutine.a
    )
  add_library( boost_chrono STATIC IMPORTED )
  set_target_properties( boost_chrono PROPERTIES
    IMPORTED_LOCATION ${LIBBOOST_LIBRARIES}/libboost_chrono.a
    )
  add_library( boost_thread STATIC IMPORTED )
  set_target_properties( boost_thread PROPERTIES
    IMPORTED_LOCATION ${LIBBOOST_LIBRARIES}/libboost_thread.a
    )
  add_library( boost_context STATIC IMPORTED )
  set_target_properties( boost_context PROPERTIES
    IMPORTED_LOCATION ${LIBBOOST_LIBRARIES}/libboost_context.a
    )

