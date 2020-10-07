file(GLOB TSOURCES  "tests/*.cc" "src/*.cc" "src/rpc/*.cc" "src/msg/*.cc")
file(GLOB coretest_SORUCES ""  "deps/r2/src/logging.cc"  "tests/*.cc"  "src/stores/*.cc"  "src/utils/*.cc" )
add_executable(coretest ${coretest_SORUCES} ${TSOURCES} )

find_library(boost_serialization_lib NAMES boost_serialization PATHS ./deps/boost PATH_SUFFIXES lib
               NO_DEFAULT_PATH)

if(NOT boost_serialization_lib)

	set(boost_serialization_lib "")

endif()

target_link_libraries(coretest gtest gtest_main ${rocksdb_lib} ${mkl_rt_lib} ${boost_serialization_lib} ${jemalloc_lib} ibverbs boost_coroutine boost_chrono boost_thread boost_context boost_system)
add_dependencies(coretest jemalloc )
