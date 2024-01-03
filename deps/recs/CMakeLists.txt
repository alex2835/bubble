cmake_minimum_required(VERSION 3.0.0)
project(recs VERSION 0.1.0)

SET(SOURCE
   "src/pool.cpp"
   "src/registry.cpp"
   "src/registry_meta.cpp"
)

add_library(recs STATIC ${SOURCE})

target_include_directories( recs PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)
