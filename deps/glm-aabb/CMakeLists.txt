cmake_minimum_required(VERSION 3.12)

SET( SOURCES
  "glm-aabb/AABB.cpp"
  "glm-aabb/AABB.hpp"
  )

# Build the library.
add_library(glm_aabb STATIC ${SOURCES})

target_link_libraries(glm_aabb PUBLIC glm)

target_include_directories(glm_aabb PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)

