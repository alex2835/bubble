cmake_minimum_required( VERSION 3.0 )
project( argagg CXX )

add_library(
  argagg
  INTERFACE
)

add_library(
  argagg::argagg
  ALIAS
  argagg
)

target_include_directories(
  argagg
  INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)