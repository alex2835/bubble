cmake_minimum_required( VERSION 3.25 )
project( bubble LANGUAGES C CXX )

set( CMAKE_CXX_STANDARD 23 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )

option( BUBBLE_BUILD_TESTS "Build bubble_tests" ON )
option( BUBBLE_ASAN "Address sanitizer" OFF )

# Clang everywhere: clang-cl on Windows (MSVC's STL, linker and PDBs),
# clang on Linux, and the clang inside Emscripten, the NDK and Xcode.
if( NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    message( WARNING "bubble is built with clang; ${CMAKE_CXX_COMPILER_ID} is not checked by CI" )
endif()

# Our own code is built with warnings as errors; deps are not.
# clang-cl takes MSVC-style switches, and /W4 there is -Wall -Wextra.
function( bubble_target_warnings target )
    if( MSVC )
        target_compile_options( ${target} PRIVATE /W4 /WX /utf-8 )
        if( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
            target_compile_options( ${target} PRIVATE -Wpedantic )
        else()
            target_compile_options( ${target} PRIVATE /permissive- )
        endif()
    else()
        target_compile_options( ${target} PRIVATE -Wall -Wextra -Wpedantic -Werror )
    endif()
endfunction()

if( BUBBLE_ASAN )
    if( MSVC )
        add_compile_options( /fsanitize=address )
        add_link_options( /INCREMENTAL:NO )
    else()
        add_compile_options( -fsanitize=address -fno-omit-frame-pointer )
        add_link_options( -fsanitize=address )
    endif()
endif()

if( BUBBLE_BUILD_TESTS )
    enable_testing()
endif()

add_subdirectory( deps )
add_subdirectory( projects )
