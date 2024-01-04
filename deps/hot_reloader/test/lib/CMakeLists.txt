cmake_minimum_required(VERSION 3.0.0)
project(HotReloaderTestLib VERSION 0.1.0)

add_library(HotReloaderTestLib MODULE test_lib.cpp)
target_link_libraries(HotReloaderTestLib HotReloaderExport)