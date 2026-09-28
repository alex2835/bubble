cmake_minimum_required(VERSION 3.18)
project(entt VERSION 3.16.0)

add_library(entt INTERFACE)

target_include_directories(entt INTERFACE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Empty components (FolderComponent) are real objects here: the engine copies,
# serializes and draws every component through a pointer to it, which the empty
# type optimization would leave null.
target_compile_definitions(entt INTERFACE ENTT_NO_ETO)
