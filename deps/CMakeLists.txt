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

# Jolt Physics 5.7.0 - rigid bodies, the character controller, queries; the
# physics module's alone. Jolt/ as released, less what its hair simulation
# runs on, which is not used: the GPU compute backends (Compute/DX12, MTL,
# VK), the HLSL and its CPU wrappers (Shaders/ keeps its headers only).
# Deterministic across platforms - a replay plays the same - which
# also wants no fused multiply-add anywhere Jolt's inline code is compiled.
# The defines are public: Jolt checks that its users were built as it was.
file( GLOB_RECURSE jolt_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/jolt/Jolt/*.cpp )
add_library( jolt STATIC ${jolt_sources} )
target_include_directories( jolt SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/jolt )
target_compile_definitions( jolt PUBLIC JPH_CROSS_PLATFORM_DETERMINISTIC $<$<CONFIG:Debug>:JPH_ENABLE_ASSERTS> )
if( MSVC )
    target_compile_options( jolt PRIVATE /w PUBLIC /clang:-ffp-contract=off )
else()
    target_compile_options( jolt PRIVATE -w PUBLIC -ffp-contract=off )
endif()

# miniaudio 0.11.26 - the audio device, mixing, decoding wav, flac and mp3;
# the audio module's alone. One C file, built as C: under Emscripten it emits
# Web Audio JavaScript and must not get a -std=c flag. Encoders left out.
add_library( miniaudio STATIC ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio/miniaudio.c )
target_include_directories( miniaudio SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio )
if( MSVC )
    target_compile_options( miniaudio PRIVATE /w )
else()
    target_compile_options( miniaudio PRIVATE -w )
endif()
if( UNIX AND NOT APPLE AND NOT EMSCRIPTEN )
    target_link_libraries( miniaudio PUBLIC dl pthread m )
endif()

# ozz-animation 0.17.0 - skeletons and clips. Built here from its sources, not
# by its own CMake, which sets flags for the whole build. ozz_runtime - the
# maths, archives and jobs: sampling, blending, local to model - is the
# animation module's alone; ozz_offline - the raw types and their builders,
# the optimizer - the model importer's, in the editor and the cook. The web
# has no SSE: the reference maths there.
file( GLOB_RECURSE ozz_runtime_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ozz/src/base/*.cc
      ${CMAKE_CURRENT_SOURCE_DIR}/ozz/src/animation/runtime/*.cc )
add_library( ozz_runtime STATIC ${ozz_runtime_sources} )
target_include_directories( ozz_runtime SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ozz/include )
target_include_directories( ozz_runtime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ozz/src )
if( EMSCRIPTEN )
    target_compile_definitions( ozz_runtime PUBLIC OZZ_BUILD_SIMD_REF )
endif()
file( GLOB ozz_offline_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ozz/src/animation/offline/*.cc )
add_library( ozz_offline STATIC ${ozz_offline_sources} )
target_include_directories( ozz_offline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ozz/src )
target_link_libraries( ozz_offline PUBLIC ozz_runtime )
foreach( target ozz_runtime ozz_offline )
    if( MSVC )
        target_compile_options( ${target} PRIVATE /w )
    else()
        target_compile_options( ${target} PRIVATE -w )
    endif()
endforeach()

# Yoga 3.2.1 - flexbox layout for the ui module, behind src/ui alone.
file( GLOB_RECURSE yoga_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/yoga/yoga/*.cpp )
add_library( yoga STATIC ${yoga_sources} )
target_include_directories( yoga SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/yoga )
if( MSVC )
    target_compile_options( yoga PRIVATE /w )
else()
    target_compile_options( yoga PRIVATE -w )
endif()

# Dear ImGui 1.92.0 (docking) - the editor's panels, on backends of its own
# (projects/editor): bubble's events in, bubble's GPU out, so ImGui sees
# neither SDL nor WebGPU. With imgui_stdlib, InputText on a std::string.
# Obsolete names off, so a release that drops them changes nothing.
set( imgui_dir ${CMAKE_CURRENT_SOURCE_DIR}/imgui )
add_library( imgui STATIC ${imgui_dir}/imgui.cpp ${imgui_dir}/imgui_demo.cpp ${imgui_dir}/imgui_draw.cpp
                          ${imgui_dir}/imgui_tables.cpp ${imgui_dir}/imgui_widgets.cpp
                          ${imgui_dir}/misc/cpp/imgui_stdlib.cpp )
target_include_directories( imgui SYSTEM PUBLIC ${imgui_dir} ${imgui_dir}/misc/cpp )
target_compile_definitions( imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS )
if( MSVC )
    target_compile_options( imgui PRIVATE /w )
else()
    target_compile_options( imgui PRIVATE -w )
endif()

# ImGuizmo 1.89 WIP - the editor's gizmo: move, turn and scale in the
# viewport, on Dear ImGui.
add_library( imguizmo STATIC ${CMAKE_CURRENT_SOURCE_DIR}/imguizmo/ImGuizmo.cpp )
target_include_directories( imguizmo SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/imguizmo )
target_link_libraries( imguizmo PUBLIC imgui )
if( MSVC )
    target_compile_options( imguizmo PRIVATE /w )
else()
    target_compile_options( imguizmo PRIVATE -w )
endif()

# imgui-node-editor 0.9.4 (develop, 021aa0e) - the canvas the editor draws an
# animation graph on: states as nodes, transitions as links, pan and zoom; on
# Dear ImGui 1.92, as the old engine built it.
set( node_editor_dir ${CMAKE_CURRENT_SOURCE_DIR}/imgui_node_editor )
add_library( imgui_node_editor STATIC ${node_editor_dir}/crude_json.cpp ${node_editor_dir}/imgui_canvas.cpp
                                      ${node_editor_dir}/imgui_node_editor.cpp
                                      ${node_editor_dir}/imgui_node_editor_api.cpp )
target_include_directories( imgui_node_editor SYSTEM PUBLIC ${node_editor_dir} )
target_link_libraries( imgui_node_editor PUBLIC imgui )
if( MSVC )
    target_compile_options( imgui_node_editor PRIVATE /w )
else()
    target_compile_options( imgui_node_editor PRIVATE -w )
endif()

# FreeType 2.15.0 and msdfgen 1.13 - a font's glyphs into a multi-channel
# signed distance field atlas, by the font importer in the editor and the
# cook; a runner draws the atlas it made. Not on the web, where neither
# runs. FreeType with the modules TrueType and OpenType fonts need
# (freetype/include/freetype/config/ftmodule.h), built as C.
if( NOT EMSCRIPTEN )
    set( freetype_dir ${CMAKE_CURRENT_SOURCE_DIR}/freetype )
    set( freetype_sources )
    foreach( file ftbase ftbbox ftbitmap ftdebug ftfstype ftgasp ftglyph ftinit ftmm ftstroke ftsynth ftsystem )
        list( APPEND freetype_sources ${freetype_dir}/src/base/${file}.c )
    endforeach()
    foreach( file autofit/autofit cff/cff psaux/psaux psnames/psnames pshinter/pshinter sfnt/sfnt smooth/smooth
                  truetype/truetype gzip/ftgzip svg/svg )
        list( APPEND freetype_sources ${freetype_dir}/src/${file}.c )
    endforeach()
    add_library( freetype STATIC ${freetype_sources} )
    target_include_directories( freetype SYSTEM PUBLIC ${freetype_dir}/include )
    target_compile_definitions( freetype PRIVATE FT2_BUILD_LIBRARY )
    if( MSVC )
        target_compile_options( freetype PRIVATE /w )
    else()
        target_compile_options( freetype PRIVATE -w )
    endif()

    file( GLOB msdfgen_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/msdfgen/core/*.cpp )
    add_library( msdfgen STATIC ${msdfgen_sources} ${CMAKE_CURRENT_SOURCE_DIR}/msdfgen/ext/import-font.cpp )
    target_include_directories( msdfgen SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/msdfgen )
    target_compile_definitions( msdfgen PUBLIC MSDFGEN_PUBLIC= MSDFGEN_USE_CPP11 )
    target_link_libraries( msdfgen PUBLIC freetype )
    if( MSVC )
        target_compile_options( msdfgen PRIVATE /w )
    else()
        target_compile_options( msdfgen PRIVATE -w )
    endif()
endif()

# SDL 3.4.20 - windows, input and the frame loop on every platform, the web
# too; seen only inside bubble/platform. Static, with what the engine uses:
# video and events, gamepads, haptics and sensors, file dialogs for the
# editor. Its 2D renderer, GPU API, camera and audio stay out, and so do its
# tests, examples and install rules.
set( SDL_SHARED OFF CACHE BOOL "" FORCE )
set( SDL_STATIC ON CACHE BOOL "" FORCE )
foreach( option SDL_TEST_LIBRARY SDL_TESTS SDL_EXAMPLES SDL_INSTALL SDL_UNINSTALL SDL_AUDIO SDL_GPU SDL_RENDER
                SDL_CAMERA )
    set( ${option} OFF CACHE BOOL "" FORCE )
endforeach()
add_subdirectory( SDL EXCLUDE_FROM_ALL )
# Its headers under its own warnings, not ours, and its code too.
set_property( TARGET SDL3-static SDL3_Headers PROPERTY SYSTEM TRUE )
if( MSVC )
    target_compile_options( SDL3-static PRIVATE /w )
else()
    target_compile_options( SDL3-static PRIVATE -w )
endif()

# wgpu-native v29.0.1.1 - WebGPU on the desktop: Rust's wgpu behind the
# standard webgpu.h, seen only inside bubble/render/gpu. The one library not
# kept in the repository: its prebuilt release is downloaded once, at
# configure time, into out/downloads - pinned by version and SHA-256, so a
# changed file stops the build - and used as a shared library, which works
# with any build type (the static one is built against the release CRT). The
# web has emdawnwebgpu instead, which comes with Emscripten.
if( NOT EMSCRIPTEN )
    set( BUBBLE_WGPU_VERSION v29.0.1.1 )
    if( WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64" )
        set( wgpu_asset wgpu-windows-x86_64-msvc-release )
        set( wgpu_sha256 7e67d7445c42aeb85e30f88930fd8d7d83ee769e3390aeb1ada75ebf3cf78132 )
    elseif( CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64" )
        set( wgpu_asset wgpu-linux-x86_64-release )
        set( wgpu_sha256 95a4d90c071005a98d03eab348beaa6b07e16eb00d1dcdb9f8348f75eb97ec5a )
    else()
        message( FATAL_ERROR "no wgpu-native build for ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR} yet" )
    endif()
    # Shared by every preset's build tree.
    set( wgpu_dir ${PROJECT_SOURCE_DIR}/out/downloads/${wgpu_asset}-${BUBBLE_WGPU_VERSION} )
    if( NOT EXISTS ${wgpu_dir}/.complete )
        set( wgpu_url https://github.com/gfx-rs/wgpu-native/releases/download/${BUBBLE_WGPU_VERSION}/${wgpu_asset}.zip )
        message( STATUS "Downloading ${wgpu_url}" )
        file( DOWNLOAD ${wgpu_url} ${wgpu_dir}.zip EXPECTED_HASH SHA256=${wgpu_sha256} TLS_VERIFY ON STATUS wgpu_status )
        list( GET wgpu_status 0 wgpu_code )
        if( NOT wgpu_code EQUAL 0 )
            file( REMOVE ${wgpu_dir}.zip )
            message( FATAL_ERROR "wgpu-native did not download: ${wgpu_status}" )
        endif()
        file( REMOVE_RECURSE ${wgpu_dir} )
        file( ARCHIVE_EXTRACT INPUT ${wgpu_dir}.zip DESTINATION ${wgpu_dir} )
        file( REMOVE ${wgpu_dir}.zip )
        file( TOUCH ${wgpu_dir}/.complete )
    endif()
    add_library( wgpu_native SHARED IMPORTED GLOBAL )
    set_target_properties( wgpu_native PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${wgpu_dir}/include
                                                  INTERFACE_INCLUDE_DIRECTORIES ${wgpu_dir}/include )
    if( WIN32 )
        set_target_properties( wgpu_native PROPERTIES IMPORTED_LOCATION ${wgpu_dir}/lib/wgpu_native.dll
                                                      IMPORTED_IMPLIB ${wgpu_dir}/lib/wgpu_native.dll.lib )
    else()
        set_target_properties( wgpu_native PROPERTIES IMPORTED_LOCATION ${wgpu_dir}/lib/libwgpu_native.so
                                                      IMPORTED_NO_SONAME TRUE )
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

# wgsl_reflect - our own reflection of WGSL source: materials' parameters
# and bindings. The standard library only.
add_subdirectory( wgsl_reflect )
