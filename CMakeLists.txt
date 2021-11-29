cmake_minimum_required(VERSION 3.20)
project(adhoc_simulation)

set(CMAKE_CXX_STANDARD 14)

set(Boost_INCLUDE_DIR /Users/zoudikai/Workspace/boost_1_77_0)
include_directories(${Boost_INCLUDE_DIR})

add_executable(server server_main.cpp server.h message.h utils.h)
add_executable(client client_main.cpp client.h message.h wormhole.h aodv.h message_handler.h utils.h)
add_executable(blackhole BlackHole.cpp BlackHole.h message.h)