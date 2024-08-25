cmake_minimum_required(VERSION 3.18)
project(imguifilebrowser VERSION 0.1.0)

SET(SOURCE
   "imfilebrowser.h"
   "imfilebrowser.cpp"
)

add_library(imguifilebrowser STATIC ${SOURCE})
target_link_libraries(imguifilebrowser PUBLIC imgui)

target_include_directories(imguifilebrowser PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}
)
