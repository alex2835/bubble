include( CheckCXXCompilerFlag )

file( GLOB_RECURSE BUBBLE_TEST_SOURCES CONFIGURE_DEPENDS *.cpp )

add_executable( bubble_tests ${BUBBLE_TEST_SOURCES} )
target_link_libraries( bubble_tests PRIVATE bubble_engine doctest luau luau_analysis )
# Folders follow the engine's: core/, scripts/, scripts/lua/; deps/ checks
# the vendored libraries. Shared helpers are included from the tests' root.
target_include_directories( bubble_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} )
bubble_executable( bubble_tests )

# doctest's TEST_CASE names its functions with __COUNTER__, which clang 22
# calls a C2y extension under -Wpedantic. Older clang has no such warning.
check_cxx_compiler_flag( -Wno-c2y-extensions BUBBLE_HAS_C2Y_WARNING )
if( BUBBLE_HAS_C2Y_WARNING )
    target_compile_options( bubble_tests PRIVATE -Wno-c2y-extensions )
endif()

add_test( NAME bubble_tests COMMAND bubble_tests )
