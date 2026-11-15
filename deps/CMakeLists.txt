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

# enkiTS 1.12 - the one pool of threads (bubble/core/jobs.hpp), its C++
# scheduler only. Not on the web: there is no -pthread there, and Jobs runs
# everything on the main thread itself.
if( NOT EMSCRIPTEN )
    find_package( Threads REQUIRED )
    add_library( enkiTS STATIC ${CMAKE_CURRENT_SOURCE_DIR}/enkiTS/src/TaskScheduler.cpp )
    target_include_directories( enkiTS SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/enkiTS/src )
    target_link_libraries( enkiTS PUBLIC Threads::Threads )
    if( MSVC )
        target_compile_options( enkiTS PRIVATE /w )
    else()
        target_compile_options( enkiTS PRIVATE -w )
    endif()
endif()

# stb_image 2.32 - images decoded by the texture importer, in the editor and
# the cook; a runner reads textures already imported. Its implementation is
# compiled once in stb_image.cpp.
add_library( stb_image STATIC ${CMAKE_CURRENT_SOURCE_DIR}/stb/stb_image.cpp )
target_include_directories( stb_image SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stb )
if( MSVC )
    target_compile_options( stb_image PRIVATE /w )
else()
    target_compile_options( stb_image PRIVATE -w )
endif()

# meshoptimizer 1.3 - meshes ordered for the GPU's caches and simplified
# into LOD levels by the model importer.
file( GLOB MESHOPTIMIZER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/meshoptimizer/src/*.cpp )
add_library( meshoptimizer STATIC ${MESHOPTIMIZER_SOURCES} )
target_include_directories( meshoptimizer SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/meshoptimizer/src )
if( MSVC )
    target_compile_options( meshoptimizer PRIVATE /w )
else()
    target_compile_options( meshoptimizer PRIVATE -w )
endif()

# assimp 6.0.6 - models read by the model importer, in the editor and the
# cook: glTF, FBX and OBJ, nothing written. Not on the web, where neither
# runs. Its own CMake, its tests, tools and other formats left out, its zlib
# its own.
if( NOT EMSCRIPTEN )
    set( BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE )
    foreach( option ASSIMP_BUILD_TESTS ASSIMP_BUILD_ASSIMP_TOOLS ASSIMP_BUILD_SAMPLES ASSIMP_BUILD_DOCS
                    ASSIMP_INSTALL ASSIMP_WARNINGS_AS_ERRORS ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT
                    ASSIMP_INJECT_DEBUG_POSTFIX ASSIMP_BUILD_USE_CCACHE ASSIMP_BUILD_DRACO ASSIMP_BUILD_USD_IMPORTER )
        set( ${option} OFF CACHE BOOL "" FORCE )
    endforeach()
    foreach( option ASSIMP_NO_EXPORT ASSIMP_IGNORE_GIT_HASH ASSIMP_BUILD_ZLIB ASSIMP_BUILD_GLTF_IMPORTER
                    ASSIMP_BUILD_FBX_IMPORTER ASSIMP_BUILD_OBJ_IMPORTER )
        set( ${option} ON CACHE BOOL "" FORCE )
    endforeach()
    add_subdirectory( assimp EXCLUDE_FROM_ALL )
    # Its headers under its own warnings, not ours.
    set_property( TARGET assimp PROPERTY SYSTEM TRUE )
    # Its warnings are its own business, and so are its zlib's.
    foreach( target assimp zlibstatic )
        if( TARGET ${target} )
            if( MSVC )
                target_compile_options( ${target} PRIVATE /w )
            else()
                target_compile_options( ${target} PRIVATE -w )
            endif()
        endif()
    endforeach()
endif()

# EnTT 3.16.0 - header only: the scene's registry, later the reflection's
# meta. SYSTEM, so its headers are under its own warnings, not ours.
add_library( entt INTERFACE )
target_include_directories( entt SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/entt/include )

# GLM 1.0.3 - header only: vectors, matrices, quaternions.
add_library( glm INTERFACE )
target_include_directories( glm SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/glm )

# utf8proc 2.12.0 - Unicode normalization (NFC) and grapheme boundaries for
# Text. One C file; its tables come in through #include of utf8proc_data.c.
add_library( utf8proc STATIC ${CMAKE_CURRENT_SOURCE_DIR}/utf8proc/utf8proc.c )
target_include_directories( utf8proc SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/utf8proc )
target_compile_definitions( utf8proc PUBLIC UTF8PROC_STATIC )
if( MSVC )
    target_compile_options( utf8proc PRIVATE /w )
else()
    target_compile_options( utf8proc PRIVATE -w )
endif()

# luaubind - our own small C++ binding for Luau, kept apart from the engine so
# it depends on Luau alone.
add_subdirectory( luaubind )
