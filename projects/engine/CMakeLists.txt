file( GLOB_RECURSE BUBBLE_ENGINE_SOURCES CONFIGURE_DEPENDS src/*.cpp )
file( GLOB_RECURSE BUBBLE_ENGINE_HEADERS CONFIGURE_DEPENDS include/*.hpp )

# The engine's own WGSL, built into it as text: the web reads no files for
# them. Rewritten when a shader changes.
file( GLOB BUBBLE_ENGINE_SHADERS CONFIGURE_DEPENDS shaders/*.wgsl )
set( BUBBLE_EMBEDDED_SHADERS ${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_shaders.cpp )
add_custom_command( OUTPUT ${BUBBLE_EMBEDDED_SHADERS}
                    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/shaders
                            -DOUTPUT=${BUBBLE_EMBEDDED_SHADERS} -P ${PROJECT_SOURCE_DIR}/cmake/embed_shaders.cmake
                    DEPENDS ${BUBBLE_ENGINE_SHADERS} ${PROJECT_SOURCE_DIR}/cmake/embed_shaders.cmake
                    COMMENT "Building the engine's shaders in"
                    VERBATIM )

add_library( bubble_engine STATIC ${BUBBLE_ENGINE_SOURCES} ${BUBBLE_ENGINE_HEADERS} ${BUBBLE_ENGINE_SHADERS}
                                  ${BUBBLE_EMBEDDED_SHADERS} )
target_include_directories( bubble_engine PUBLIC include )
bubble_target_warnings( bubble_engine )
# Luau through luaubind: the VM, native code generation, and the compiler for
# the editor and tests - a runner that never calls CompileScript links none of it.
target_link_libraries( bubble_engine PUBLIC luaubind )
# The scene is an EnTT registry, and its component templates live in its header.
target_link_libraries( bubble_engine PUBLIC entt )
# Vectors in Value and, later, everywhere; utf8proc only behind Text.
target_link_libraries( bubble_engine PUBLIC glm PRIVATE utf8proc )

# Windows, input and the frame loop: SDL, behind bubble/platform alone.
target_link_libraries( bubble_engine PRIVATE SDL3::SDL3-static )
# WebGPU: wgpu-native on the desktop, the browser's own through emdawnwebgpu
# on the web. Its types are in bubble/render/gpu's headers.
if( EMSCRIPTEN )
    target_compile_options( bubble_engine PUBLIC --use-port=emdawnwebgpu )
    target_link_options( bubble_engine PUBLIC --use-port=emdawnwebgpu )
else()
    target_link_libraries( bubble_engine PUBLIC wgpu_native )
endif()

# Jobs' pool; the web has none and runs jobs on the main thread. Assets
# come by fetch there.
if( EMSCRIPTEN )
    target_compile_definitions( bubble_engine PRIVATE BUBBLE_JOBS_INLINE )
    target_link_options( bubble_engine PUBLIC -sFETCH=1 )
else()
    target_link_libraries( bubble_engine PRIVATE enkiTS )
endif()

if( BUBBLE_PROFILE )
    target_link_libraries( bubble_engine PUBLIC tracy_client )
    target_compile_definitions( bubble_engine PUBLIC BUBBLE_PROFILE_ENABLED )
endif()
