cmake_minimum_required(VERSION 3.0.0)
project(recs VERSION 0.1.0)

SET(SOURCE
   "src/pool.cpp"
   "src/registry.cpp"
)

add_library(recs STATIC ${SOURCE})

#add_definitions(-DRECS_STATIC)
#add_executable(recs ${SOURCE} tests/test.cpp)

target_include_directories( recs PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)
