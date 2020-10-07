cmake_minimum_required(VERSION 3.5)

project(demo)

INCLUDE_DIRECTORIES(../include)

add_executable(sub1 sub_one.cpp)
add_executable(sub2 sub_two.cpp)
add_executable(pub0 pub.cpp)
#add_executable(pub pub_demo.cpp)
#add_executable(sub sub_demo.cpp)

target_link_libraries(sub1 libzmq.so)
target_link_libraries(sub2 libzmq.so)
target_link_libraries(pub0 libzmq.so)
#target_link_libraries(pub libzmq.so)
#target_link_libraries(sub libzmq.so)
#target_link_libraries(demo pthread)
