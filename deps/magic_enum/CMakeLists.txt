
cmake_minimum_required(VERSION 3.18)
project(magic_enum VERSION 0.1.0)

add_library(magic_enum INTERFACE)

target_include_directories(magic_enum INTERFACE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)