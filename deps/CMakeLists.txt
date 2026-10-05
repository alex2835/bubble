# Third-party code, vendored. Each library keeps its own license file beside it.

# doctest 2.4.12 - header only
add_library( doctest INTERFACE )
target_include_directories( doctest SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/doctest )

# Luau 0.741 - the VM and the compiler, the type checker (Analysis, for the
# editor and import) and native code generation (CodeGen, desktop and
# Android: the web and iOS cannot make machine code at run time).
# Built as C++: a script error is a C++ exception, so destructors in our
# bindings run when a script fails.
function( bubble_luau_library name dir )
    file( GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/luau/${dir}/src/*.cpp )
    add_library( ${name} STATIC ${sources} )
    target_include_directories( ${name} SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/luau/${dir}/include )
    if( MSVC )
        target_compile_options( ${name} PRIVATE /w )
    else()
        target_compile_options( ${name} PRIVATE -w )
    endif()
endfunction()

bubble_luau_library( luau_common Common )
bubble_luau_library( luau_ast Ast )
bubble_luau_library( luau_bytecode Bytecode )
bubble_luau_library( luau_compiler Compiler )
bubble_luau_library( luau_vm VM )
target_link_libraries( luau_ast PUBLIC luau_common )
target_link_libraries( luau_bytecode PUBLIC luau_common )
target_link_libraries( luau_compiler PUBLIC luau_ast luau_bytecode )
target_link_libraries( luau_vm PUBLIC luau_common )
# sqrt lowers to one instruction without errno.
if( NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" )
    target_compile_options( luau_vm PRIVATE -fno-math-errno )
endif()

bubble_luau_library( luau_config Config )
bubble_luau_library( luau_analysis Analysis )
target_link_libraries( luau_config PUBLIC luau_ast PRIVATE luau_compiler luau_vm )
target_link_libraries( luau_analysis PUBLIC luau_ast luau_config PRIVATE luau_compiler luau_vm )

add_library( luau INTERFACE )
target_link_libraries( luau INTERFACE luau_compiler luau_vm )

if( NOT EMSCRIPTEN )
    bubble_luau_library( luau_codegen CodeGen )
    # Code generation reaches into the VM's internals.
    target_include_directories( luau_codegen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/luau/VM/src )
    target_link_libraries( luau_codegen PUBLIC luau_common PRIVATE luau_vm )
    target_link_libraries( luau INTERFACE luau_codegen )
    target_compile_definitions( luau INTERFACE BUBBLE_LUAU_CODEGEN )
endif()

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

# luaubind - our own small C++ binding for Luau, kept apart from the engine so
# it depends on Luau alone.
add_subdirectory( luaubind )
