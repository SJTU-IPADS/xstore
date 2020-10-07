include_directories(../deps)

file(GLOB SOURCES  "../src/*cc")
file(GLOB MSOURCES  "../src/msg/*.cc")
file(GLOB RSOURCES  "../src/rpc/*.cc")
file(GLOB TSOURCES  "../tests/*.cc")
add_executable(coretest "${TSOURCES}" "${SOURCES}" "${MSOURCES}" "${RSOURCES}")

target_link_libraries(coretest gtest gtest_main ibverbs ssmalloc
                               boost_coroutine boost_chrono boost_thread boost_context boost_system
                               )
