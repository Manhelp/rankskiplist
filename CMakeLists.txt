cmake_minimum_required(VERSION 3.6)
project(rankskiplist)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -pipe -std=c++11")

set(SOURCE_FILES
        test.cpp
        rankskip_list.h)


add_executable(ranktest ${SOURCE_FILES})

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)
