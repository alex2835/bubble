cmake_minimum_required(VERSION 3.12)

# Engine tests that need no window: a console executable per area, plain
# asserts, exit code is the verdict. Run one directly or all through ctest.
set(TARGET_NAME bubble_tests)

file(GLOB_RECURSE SRCS *.cpp)
add_executable(${TARGET_NAME} ${SRCS})

add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS)
add_definitions(-DSOL_ALL_SAFETIES_ON)
target_link_libraries(${TARGET_NAME} bubble_engine)
target_copy_webgpu_binaries(${TARGET_NAME})

enable_testing()
add_test(NAME bubble_tests COMMAND ${TARGET_NAME})
