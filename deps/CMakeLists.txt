
# Suppres third party warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -w -stdlib=libc++")
	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -w -stdlib=libc++")
endif()


if(NOT EMSCRIPTEN)
	#GIT_TAG 3.4  GIT_REPOSITORY "https://github.com/glfw/glfw.git"
	# 3.4+ is required: glfw3webgpu calls glfwGetPlatform().
	option(GLFW_BUILD_EXAMPLES "" OFF)
	option(GLFW_BUILD_TESTS "" OFF)
	option(GLFW_BUILD_DOCS "" OFF)
	add_subdirectory(glfw)
endif()

# WebGPU. One 'webgpu' target; the implementation is chosen by WEBGPU_BACKEND
# (WGPU / DAWN / EMSCRIPTEN / EMDAWNWEBGPU), defaulting to WGPU natively and to
# EMDAWNWEBGPU under emcmake. See deps/webgpu/VENDORED_VERSION.txt.
add_subdirectory(webgpu)

# WGSL reflection. WebGPU exposes none of its own, so anything that needs to
# know what a shader declares has to read it out of the source.
add_subdirectory(wgsl_reflect)

# Creates the WGPUSurface for a GLFW window, per platform.
#GIT_TAG v1.3.0-alpha  GIT_REPOSITORY "https://github.com/eliemichel/glfw3webgpu.git"
add_subdirectory(glfw3webgpu)

#GIT_TAG v1.92.0-docking  GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
add_subdirectory(imgui)
add_subdirectory(imgui_gizmo)
add_subdirectory(imgui-filebrowser)

#GIT_TAG v6.0.5  GIT_REPOSITORY "https://github.com/assimp/assimp.git"
option(ASSIMP_BUILD_TESTS "" OFF)
option(ASSIMP_BUILD_DOCS "" OFF)
option(BUILD_SHARED_LIBS "" OFF)
option(ASSIMP_NO_EXPORT "" ON)
option(ASSIMP_INJECT_DEBUG_POSTFIX "" OFF)
option(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT "" FALSE)
set(ASSIMP_BUILD_OBJ_IMPORTER TRUE)
# Skinned characters come in as glTF or FBX; OBJ carries no skeleton.
set(ASSIMP_BUILD_GLTF_IMPORTER TRUE)
set(ASSIMP_BUILD_FBX_IMPORTER TRUE)
add_subdirectory(assimp)

#GIT_TAG 0.9.9.8  GIT_REPOSITORY "https://github.com/g-truc/glm.git" 
add_subdirectory(glm)
add_subdirectory(glm-aabb)

add_subdirectory(lua)
add_subdirectory(sol2-3.5.0)
add_subdirectory(glm_lua_bindings)
add_subdirectory(json)
add_subdirectory(recs)
#add_subdirectory(hot_reloader)
add_subdirectory(argagg)
add_subdirectory(stb_image)

#GIT_TAG 0.11.25  GIT_REPOSITORY "https://github.com/mackron/miniaudio.git"
# Single header, vendored. Device I/O, decoding, mixing and 3D spatialization
# in one target with no dependencies of its own - including under Emscripten,
# where it talks to Web Audio directly.
add_subdirectory(miniaudio)
add_subdirectory(magic_enum)
add_subdirectory(thread_pool)

#GIT_TAG 0.17.0  GIT_REPOSITORY "https://github.com/guillaumeblanc/ozz-animation.git"
# Skeletal animation runtime: sampling, blending, local-to-model, skinning.
# Library targets only; see ozz/VENDORED_VERSION.txt for what was dropped.
option(ozz_build_tools "" OFF)
option(ozz_build_fbx "" OFF)
option(ozz_build_gltf "" OFF)
option(ozz_build_samples "" OFF)
option(ozz_build_howtos "" OFF)
option(ozz_build_tests "" OFF)
option(ozz_build_postfix "" OFF)
# ozz defaults to the static CRT on MSVC; everything else here uses the DLL one.
option(ozz_build_msvc_rt_dll "" ON)
if(EMSCRIPTEN)
	# No SSE in wasm without -msimd128 emulation; the scalar path is the one
	# ozz's own WebAssembly CI builds.
	set(ozz_build_simd_ref ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(ozz)


option(BUILD_BULLET3 OFF)
# Bullet's multithreaded world/dispatcher/solver only parallelise when the
# library is built with BT_THREADSAFE; otherwise the *Mt classes run serially.
# Left off for the web build, where pthreads need SharedArrayBuffer and the
# COOP/COEP headers to go with it.
if(NOT EMSCRIPTEN)
	set(BULLET2_MULTITHREADING ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(bullet)
if(BULLET2_MULTITHREADING)
	# Bullet only adds the define to its own directory, but btThreads.h's
	# inline btMutexLock/Unlock compile to nothing without it - so every
	# consumer must see the same value as the library or the locks are skipped.
	target_compile_definitions(BulletIncludeFile INTERFACE BT_THREADSAFE=1)
endif()
# btScalar.h defines operator+/-/* on __m128 for its own convenience, and so
# do ozz's SSE headers; a translation unit that includes both - anything with
# the PCH and an ozz job - fails on the redefinition. ozz's inline math needs
# its operators, Bullet's public API does not (BT_USE_SSE_IN_API is off on
# Windows), so Bullet is the one that goes without in consumers.
target_compile_definitions(BulletIncludeFile INTERFACE BT_NO_SIMD_OPERATOR_OVERLOADS)

