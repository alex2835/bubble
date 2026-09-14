cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 23)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS}\
	    -Werror\
		-Wno-format-security\
		-Wno-unused-variable\
		-Wno-misleading-indentation")
	if(APPLE)
		SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -stdlib=libc++")

		# A non-Apple clang (Homebrew LLVM) compiles against its own libc++
		# headers but links against the SDK's older libc++ by default, so
		# symbols the newer headers introduced - __hash_memory,
		# __from_chars_floating_point - come up undefined. Point the link at
		# the toolchain's own runtime, and bake in an rpath so the binary
		# finds it at run time too.
		get_filename_component(BUBBLE_TOOLCHAIN_ROOT "${CMAKE_CXX_COMPILER}" DIRECTORY)
		get_filename_component(BUBBLE_TOOLCHAIN_ROOT "${BUBBLE_TOOLCHAIN_ROOT}" DIRECTORY)
		if(EXISTS "${BUBBLE_TOOLCHAIN_ROOT}/lib/c++/libc++.dylib")
			add_link_options("-L${BUBBLE_TOOLCHAIN_ROOT}/lib/c++"
			                 "-Wl,-rpath,${BUBBLE_TOOLCHAIN_ROOT}/lib/c++")
			if(EXISTS "${BUBBLE_TOOLCHAIN_ROOT}/lib/unwind")
				add_link_options("-L${BUBBLE_TOOLCHAIN_ROOT}/lib/unwind" "-lunwind")
			endif()
		endif()
	endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /WX")
endif()

add_subdirectory(editor)
add_subdirectory(engine)
add_subdirectory(tests)