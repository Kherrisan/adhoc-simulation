cmake_minimum_required(VERSION 3.20)
project(adhoc_simulation)

set(CMAKE_CXX_STANDARD 14)

set(Boost_INCLUDE_DIR /Users/zoudikai/Workspace/boost_1_77_0)
#set(Boost_LIBRARY_DIR /Users/zoudikai/Workspace/boost_1_77_0/libs)
include_directories(${Boost_INCLUDE_DIR})
#link_directories(${Boost_LIBRARY_DIR})

add_executable(server server_main.cpp server.h message.h)
add_executable(client client_main.cpp client.h message.h)
