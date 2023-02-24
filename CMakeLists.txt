cmake_minimum_required(VERSION 2.9)

project(anisotropinator)

set(CMAKE_CXX_STANDARD 20)

add_executable(anisotropinator main.cpp)

target_include_directories(anisotropinator PRIVATE 3rdParty)

file(COPY data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})