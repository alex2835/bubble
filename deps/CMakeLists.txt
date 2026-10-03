# Third-party code, vendored. Each library keeps its own license file beside it.

# doctest 2.4.12 - header only
add_library( doctest INTERFACE )
target_include_directories( doctest SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/doctest )

# Tracy 0.14.1 - the profiler's client, only with BUBBLE_PROFILE. On demand:
# it records nothing until the profiler connects.
if( BUBBLE_PROFILE )
    find_package( Threads REQUIRED )
    add_library( tracy_client STATIC ${CMAKE_CURRENT_SOURCE_DIR}/tracy/public/TracyClient.cpp )
    target_include_directories( tracy_client SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tracy/public )
    target_compile_definitions( tracy_client PUBLIC TRACY_ENABLE TRACY_ON_DEMAND )
    target_link_libraries( tracy_client PUBLIC Threads::Threads ${CMAKE_DL_LIBS} )
    # Its warnings are its own business.
    if( MSVC )
        target_compile_options( tracy_client PRIVATE /w )
    else()
        target_compile_options( tracy_client PRIVATE -w )
    endif()
    if( WIN32 )
        target_link_libraries( tracy_client PUBLIC ws2_32 dbghelp )
    endif()
endif()
