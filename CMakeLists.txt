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
option( BUBBLE_PROFILE "Tracy profiler zones (bubble/core/profile.hpp)" OFF )

# Tracy's client needs sockets and threads the browser does not give.
if( EMSCRIPTEN AND BUBBLE_PROFILE )
    message( WARNING "BUBBLE_PROFILE is not available on the web; turned off" )
    set( BUBBLE_PROFILE OFF CACHE BOOL "" FORCE )
endif()

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
        # The CRT's "use fopen_s" deprecations: the code is portable C++.
        target_compile_definitions( ${target} PRIVATE _CRT_SECURE_NO_WARNINGS )
        if( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
            target_compile_options( ${target} PRIVATE -Wpedantic )
        else()
            target_compile_options( ${target} PRIVATE /permissive- )
        endif()
    else()
        target_compile_options( ${target} PRIVATE -Wall -Wextra -Wpedantic -Werror )
    endif()
endfunction()

# Every executable: warnings, and on Windows the manifest that makes the
# process's code page UTF-8 (see bubble/core/process.hpp).
function( bubble_executable target )
    bubble_target_warnings( ${target} )
    if( WIN32 )
        target_sources( ${target} PRIVATE ${PROJECT_SOURCE_DIR}/cmake/utf8.manifest )
    endif()
endfunction()

# Script errors are C++ exceptions; the web needs them switched on, and
# native wasm exceptions are the fast kind.
if( EMSCRIPTEN )
    add_compile_options( -fwasm-exceptions )
    add_link_options( -fwasm-exceptions )
endif()

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
