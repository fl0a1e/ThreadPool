cmake_minimum_required(VERSION 3.15)

project(threadPool)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include_directories(${PROJECT_SOURCE_DIR})

add_compile_options(-std=c++17 -O2 -lpthread -Wall)

add_executable(threadPool main.cpp)