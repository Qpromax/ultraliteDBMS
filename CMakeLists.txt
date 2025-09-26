cmake_minimum_required(VERSION 3.31)
project(ultraliteDBMS)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ultraliteDBMS main.cpp ulDBMS.cpp)