cmake_minimum_required(VERSION 3.0.0)
project(hot_reloader_import VERSION 0.1.0)

SET(SOURCE
   "src/import.cpp"
)

add_library( hot_reloader_import STATIC ${SOURCE})

target_include_directories( hot_reloader_import PUBLIC
   "include"
   "../common"
   "../dependencies"
)


