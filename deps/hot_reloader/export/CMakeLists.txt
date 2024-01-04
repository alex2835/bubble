cmake_minimum_required(VERSION 3.0.0)
project(hot_reloader_export VERSION 0.1.0)

SET(SOURCE
   "src/function_registry.cpp"
   "src/export.cpp"
)

add_library(hot_reloader_export STATIC ${SOURCE})

target_include_directories( hot_reloader_export PUBLIC
   "include"
   "../common"
   "../dependencies"
)
