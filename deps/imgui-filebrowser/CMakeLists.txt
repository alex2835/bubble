cmake_minimum_required(VERSION 3.18)
project(imguifilebrowser VERSION 0.1.0)

SET(SOURCE
   "imfilebrowser.h"
)

add_library(imguifilebrowser INTERFACE ${SOURCE})

target_include_directories(imguifilebrowser INTERFACE
   ${CMAKE_CURRENT_SOURCE_DIR}
)
