# # # # sol2
# The MIT License (MIT)
# 
# Copyright (c) 2013-2022 Rapptz, ThePhD, and contributors
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# protect from multiple inclusion
if(lua_jit_build_included)
	return()
endif(lua_jit_build_included)
set(lua_jit_build_included true)

# import necessary standard modules
include(ExternalProject)

# MD5 hashes taken off of LuaJIT's website
# must be updated whenever a new version appears
set(LUA_JIT_MD5_2.1.0-beta3.tar.gz eae40bc29d06ee5e3078f9444fcea39b)
set(LUA_JIT_MD5_2.1.0-beta3.zip 58d0480e1af0811e7ecee45498d62e2d)
set(LUA_JIT_MD5_2.1.0-beta2.tar.gz fa14598d0d775a7ffefb138a606e0d7b)
set(LUA_JIT_MD5_2.1.0-beta2.zip b5d943c0174ca217736e2ddc2d9721c3)
set(LUA_JIT_MD5_2.1.0-beta1.tar.gz 5a5bf71666e77cf6e7a1ae851127b834)
set(LUA_JIT_MD5_2.1.0-beta1.zip 4b5c2c9aef0e7c0b622b09e7c84d566b)
set(LUA_JIT_MD5_2.0.5.tar.gz 48353202cbcacab84ee41a5a70ea0a2c)
set(LUA_JIT_MD5_2.0.5.zip f7cf52a049d74aee4e624bdc1160b80d)
set(LUA_JIT_MD5_2.0.4.tar.gz dd9c38307f2223a504cbfb96e477eca0)
set(LUA_JIT_MD5_2.0.4.zip ed1f0caf3d390171f423f6f1b5c57aac)
set(LUA_JIT_MD5_2.0.3.tar.gz f14e9104be513913810cd59c8c658dc0)
set(LUA_JIT_MD5_2.0.3.zip 6c0f6958d5e1f67734fb1ff514ec4c84)
set(LUA_JIT_MD5_2.0.2.tar.gz 112dfb82548b03377fbefbba2e0e3a5b)
set(LUA_JIT_MD5_2.0.2.zip a57c7d1b8eaf46559303dcfd56404045)
set(LUA_JIT_MD5_2.0.1.tar.gz 85e406e8829602988eb1233a82e29f1f)
set(LUA_JIT_MD5_2.0.1.zip cf4aee0e40b220054ee3cffbe0cd6ed5)
set(LUA_JIT_MD5_2.0.0.tar.gz 97a2b87cc0490784f54b64cfb3b8f5ad)
set(LUA_JIT_MD5_2.0.0.zip 467f4f531f7e08ee252f5030ecada7ed)
set(LUA_JIT_MD5_2.0.0-beta11.tar.gz 824aa2684a11e3cc3abe87350a7b6139)
set(LUA_JIT_MD5_2.0.0-beta11.zip 8629401437048e477c94bd791b0a823a)
set(LUA_JIT_MD5_2.0.0-beta10.tar.gz ed66689b96f7ad7bfeffe0b4ff2d63d4)
set(LUA_JIT_MD5_2.0.0-beta10.zip f6bbd472726b761b29438c4a06b5ab3c)
set(LUA_JIT_MD5_2.0.0-beta9.tar.gz e7e03e67e2550817358bc28b44270c6d)
set(LUA_JIT_MD5_2.0.0-beta9.zip 3f9ca0309f26e789c6c3246c83696f84)
set(LUA_JIT_MD5_2.0.0-beta8.tar.gz f0748a73ae268d49b1d01f56c4fe3e61)
set(LUA_JIT_MD5_2.0.0-beta8.zip fb096a90c9e799c8922f32095ef3d93c)
set(LUA_JIT_MD5_2.0.0-beta7.tar.gz b845dec15dd9eba2fd17d865601a52e5)
set(LUA_JIT_MD5_2.0.0-beta7.zip e10fc2b19de52d8770cdf24f1791dd77)
set(LUA_JIT_MD5_2.0.0-beta6.tar.gz bfcbe2a11162cfa84d5a1693b442c8bf)
set(LUA_JIT_MD5_2.0.0-beta6.zip f64945c5ecaf3ea71a829fdbb5cb196c)
set(LUA_JIT_MD5_2.0.0-beta5.tar.gz 7e0dfa03a140148149a1021d4ffd5c57)
set(LUA_JIT_MD5_2.0.0-beta5.zip be8087fcb576c30ffbb6368ebc284498)
set(LUA_JIT_MD5_2.0.0-beta4.tar.gz 5c5a9305b3e06765e1dae138e1a95c3a)
set(LUA_JIT_MD5_2.0.0-beta4.zip d0b241be6207fa7d97b6afc41368e05f)
set(LUA_JIT_MD5_2.0.0-beta3.tar.gz 313b6f164e93e1bbac7bf87abb58d4a1)
set(LUA_JIT_MD5_2.0.0-beta3.zip bea9c7bcd5084f98830e31956f276ff6)
set(LUA_JIT_MD5_2.0.0-beta2.tar.gz 2ebcc38fa1d9756dc2e341f191701120)
set(LUA_JIT_MD5_2.0.0-beta2.zip 94086f99f647d46a8360adeb11851d66)
set(LUA_JIT_MD5_2.0.0-beta1.tar.gz 9ed7646d03580a1cec4abeb74ca44843)
set(LUA_JIT_MD5_2.0.0-beta1.zip 19d5bac616fa739343c7158d9d99a3f3)
set(LUA_JIT_MD5_1.1.8.tar.gz ad0e319483fa235e3979537a748631e9)
set(LUA_JIT_MD5_1.1.8.zip 92870c80f504c34c9b7547cd6c5562d0)
set(LUA_JIT_MD5_1.1.7.tar.gz 3aed0795f7c8725d3613269cd56f8e5a)
set(LUA_JIT_MD5_1.1.7.zip 40a8dbc214306bb4b9849fcf026c4ee0)
set(LUA_JIT_MD5_1.1.6.tar.gz 1a1320e09d0cd5b793014556fb7d64c9)
set(LUA_JIT_MD5_1.1.6.zip 350d7b9230637056fbd6158b95e8fa11)
set(LUA_JIT_MD5_1.1.5.tar.gz b99d244ba4fc1979946ae1025368fc5c)
set(LUA_JIT_MD5_1.1.5.zip d3ffbae3bfcd5914b02dc00b1118a59d)
set(LUA_JIT_MD5_1.1.4.tar.gz 9fe29cfb8126bc9c4302701c06965f1c)
set(LUA_JIT_MD5_1.1.4.zip 30d318e3287000ecf4c93b29e8783183)
set(LUA_JIT_MD5_1.1.3.tar.gz f5db1a147ed3d34677ad1ef310c56da7)
set(LUA_JIT_MD5_1.1.3.zip 5949e7bce9d97c37c282e1cbe85aa378)
set(LUA_JIT_MD5_1.1.2.tar.gz 4ae25ce7e3f301d1fcf0b713016edab0)
set(LUA_JIT_MD5_1.1.0.tar.gz 16d880a98a1ff6608ac1039c802233db)
set(LUA_JIT_MD5_1.0.3.tar.gz d0a63d5394cca549889bd820a05b32d2)

# Clean up some variables
string(TOLOWER ${LUA_VERSION} LUA_JIT_NORMALIZED_LUA_VERSION)
if (LUA_JIT_NORMALIZED_LUA_VERSION MATCHES "([0-9]+\\.[0-9]+\\.[0-9]+(-[A-Za-z0-9_-]+)?)")
	# 3-digit with optional beta1/beta2/beta3 (or whatever): probably okay?
	set(LUA_JIT_VERSION ${CMAKE_MATCH_1})
elseif (LUA_JIT_NORMALIZED_LUA_VERSION MATCHES "([0-9]+\\.[0-9]+)")
	# extend version number with prefix
	if (${CMAKE_MATCH_1} EQUAL 2)
		if (${CMAKE_MATCH_2} EQUAL 0)
			set(LUA_JIT_VERSION 2.0)
		elseif (${CMAKE_MATCH_2} EQUAL 1)
			set(LUA_JIT_VERSION 2.1)
		endif()
	endif()
	if (NOT LUA_JIT_VERSION)
		# Just pick a default version and roll with it
		set(LUA_JIT_VERSION ${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.0)			
	endif()
elseif (LUA_JIT_NORMALIZED_LUA_VERSION MATCHES "latest")
	set(LUA_JIT_VERSION 2.1)
else()
	MESSAGE(FATAL "Cannot deduce LuaJIT version from ${LUA_VERSION}")
endif()

FIND_PACKAGE_MESSAGE(LUABUILD
	"Selecting LuaJIT ${LUA_JIT_VERSION} from '${LUA_VERSION}' and building a ${LUA_BUILD_LIBRARY_TYPE} library..."
	"[${LUA_JIT_VERSION}][${LUA_VERSION}][${LUA_BUILD_LIBRARY_TYPE}]")

# Get hashes for the build
# LuaJIT unfortunately does not give us SHA1 hashes as well
# set(LUA_JIT_SHA1 ${LUA_JIT_SHA1_${LUA_JIT_VERSION}})
if (WIN32)
	set(LUA_JIT_MD5 ${LUA_JIT_MD5_${LUA_JIT_VERSION}.zip})
	set(LUA_JIT_DOWNLOAD_URI http://luajit.org/download/LuaJIT-${LUA_JIT_VERSION}.zip)
else()
	set(LUA_JIT_MD5 ${LUA_JIT_MD5_${LUA_JIT_VERSION}.tar.gz})
	set(LUA_JIT_DOWNLOAD_URI http://luajit.org/download/LuaJIT-${LUA_JIT_VERSION}.tar.gz)
endif()

if (LUA_JIT_MD5)
	set(LUA_JIT_DOWNLOAD_MD5_COMMAND URL_MD5 ${LUA_JIT_MD5})
else ()
	set(LUA_JIT_DOWNLOAD_MD5_COMMAND "")
endif()
if (LUA_JIT_SHA1)
	set(LUA_JIT_DOWNLOAD_SHA1_COMMAND URL_HASH SHA1=${LUA_JIT_SHA1})
else ()
	set(LUA_JIT_DOWNLOAD_SHA1_COMMAND "")
endif()

set(LUA_JIT_SOURCE_DIR "${LUA_BUILD_TOPLEVEL}/src")
set(LUA_JIT_INSTALL_DIR "${LUA_BUILD_TOPLEVEL}/install")
set(LUA_JIT_INCLUDE_DIRS "${LUA_JIT_SOURCE_DIR}")
file(MAKE_DIRECTORY ${LUA_JIT_SOURCE_DIR})
file(MAKE_DIRECTORY ${LUA_JIT_INSTALL_DIR})

set(LUA_JIT_LIB_FILENAME "${CMAKE_STATIC_LIBRARY_PREFIX}${LUA_BUILD_LIBNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(LUA_JIT_IMP_LIB_FILENAME "${CMAKE_IMPORT_LIBRARY_PREFIX}${LUA_BUILD_LIBNAME}${CMAKE_IMPORT_LIBRARY_SUFFIX}")
set(LUA_JIT_LIB_EXP_FILENAME "${LUA_BUILD_LIBNAME}.exp")
set(LUA_JIT_DLL_FILENAME "${CMAKE_SHARED_LIBRARY_PREFIX}${LUA_BUILD_LIBNAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(LUA_JIT_EXE_FILENAME "${LUA_BUILD_LIBNAME}${CMAKE_EXECUTABLE_SUFFIX}")

set(LUA_JIT_LIB_FILE "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${LUA_JIT_LIB_FILENAME}")
set(LUA_JIT_IMP_LIB_FILE "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${LUA_JIT_IMP_LIB_FILENAME}")
set(LUA_JIT_LIB_EXP_FILE "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${LUA_JIT_LIB_EXP_FILENAME}")
set(LUA_JIT_DLL_FILE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${LUA_JIT_DLL_FILENAME}")
set(LUA_JIT_EXE_FILE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${LUA_JIT_EXE_FILENAME}")

# # # Do the build
if (MSVC)
	# Visual C++ is predicated off running msvcbuild.bat
	# which requires a Visual Studio Command Prompt
	# make sure to find the right one
	find_file(VCVARS_ALL_BAT NAMES vcvarsall.bat
		HINTS "C:/Program Files (x86)/Microsoft Visual Studio/2017/Community/VC/Auxiliary/Build"
		"C:/Program Files (x86)/Microsoft Visual Studio/2017/Community/VC/Auxiliary"
		"C:/Program Files (x86)/Microsoft Visual Studio/2017/Community/VC"
		"C:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC/Auxiliary/Build"
		"C:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC/Auxiliary"
		"C:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC"
		"C:/Program Files (x86)/Microsoft Visual Studio/2017/Enterprise/VC/Auxiliary/Build"
		"C:/Program Files (x86)/Microsoft Visual Studio/2017/Enterprise/VC/Auxiliary"
		"C:/Program Files (x86)/Microsoft Visual Studio/2017/Enterprise/VC"

		"C:/Program Files/Microsoft Visual Studio/2017/Community/VC/Auxiliary/Build"
		"C:/Program Files/Microsoft Visual Studio/2017/Community/VC/Auxiliary"
		"C:/Program Files/Microsoft Visual Studio/2017/Community/VC"
		"C:/Program Files/Microsoft Visual Studio/2017/Professional/VC/Auxiliary/Build"
		"C:/Program Files/Microsoft Visual Studio/2017/Professional/VC/Auxiliary"
		"C:/Program Files/Microsoft Visual Studio/2017/Professional/VC"
		"C:/Program Files/Microsoft Visual Studio/2017/Enterprise/VC/Auxiliary/Build"
		"C:/Program Files/Microsoft Visual Studio/2017/Enterprise/VC/Auxiliary"
		"C:/Program Files/Microsoft Visual Studio/2017/Enterprise/VC"

		"C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Auxiliary/Build"
		"C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Auxiliary"
		"C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC"
		"C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/VC/Auxiliary/Build"
		"C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/VC/Auxiliary"
		"C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/VC"
		"C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/VC/Auxiliary/Build"
		"C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/VC/Auxiliary"
		"C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/VC"

		"C:/Program Files/Microsoft Visual Studio/2019/Community/VC/Auxiliary/Build"
		"C:/Program Files/Microsoft Visual Studio/2019/Community/VC/Auxiliary"
		"C:/Program Files/Microsoft Visual Studio/2019/Community/VC"
		"C:/Program Files/Microsoft Visual Studio/2019/Professional/VC/Auxiliary/Build"
		"C:/Program Files/Microsoft Visual Studio/2019/Professional/VC/Auxiliary"
		"C:/Program Files/Microsoft Visual Studio/2019/Professional/VC"
		"C:/Program Files/Microsoft Visual Studio/2019/Enterprise/VC/Auxiliary/Build"
		"C:/Program Files/Microsoft Visual Studio/2019/Enterprise/VC/Auxiliary"
		"C:/Program Files/Microsoft Visual Studio/2019/Enterprise/VC"

		"C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build"
		"C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/VC/Auxiliary"
		"C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/VC"
		"C:/Program Files (x86)/Microsoft Visual Studio/2022/Professional/VC/Auxiliary/Build"
		"C:/Program Files (x86)/Microsoft Visual Studio/2022/Professional/VC/Auxiliary"
		"C:/Program Files (x86)/Microsoft Visual Studio/2022/Professional/VC"
		"C:/Program Files (x86)/Microsoft Visual Studio/2022/Enterprise/VC/Auxiliary/Build"
		"C:/Program Files (x86)/Microsoft Visual Studio/2022/Enterprise/VC/Auxiliary"
		"C:/Program Files (x86)/Microsoft Visual Studio/2022/Enterprise/VC"

		"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build"
		"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary"
		"C:/Program Files/Microsoft Visual Studio/2022/Community/VC"
		"C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Auxiliary/Build"
		"C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Auxiliary"
		"C:/Program Files/Microsoft Visual Studio/2022/Professional/VC"
		"C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Auxiliary/Build"
		"C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Auxiliary"
		"C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC")
	if (VCVARS_ALL_BAT MATCHES "VCVARS_ALL_BAT-NOTFOUND")
		MESSAGE(FATAL_ERROR "Cannot find 'vcvarsall.bat' file or similar needed to build LuaJIT ${LUA_VERSION} on Windows")
	endif()
	if (CMAKE_SIZEOF_VOID_P LESS_EQUAL 4)
		set(LUA_JIT_MAKE_COMMAND "${VCVARS_ALL_BAT}" x86)
	else()
		set(LUA_JIT_MAKE_COMMAND "${VCVARS_ALL_BAT}" x64)
	endif()
	set(LUA_JIT_MAKE_COMMAND ${LUA_JIT_MAKE_COMMAND} && cd src && msvcbuild.bat)
	if (BUILD_LUA_NOGC64)
		set(LUA_JIT_MAKE_COMMAND ${LUA_JIT_MAKE_COMMAND} nogc64)
	endif()

	set(LUA_JIT_MAKE_COMMAND ${LUA_JIT_MAKE_COMMAND} $<$<CONFIG:Debug>:debug>)
	if (NOT BUILD_LUA_AS_DLL)
		set(LUA_JIT_MAKE_COMMAND ${LUA_JIT_MAKE_COMMAND} static)
	endif()

	set(LUA_JIT_PREBUILT_LIB "lua51.lib")
	set(LUA_JIT_PREBUILT_IMP_LIB "lua51.lib")
	set(LUA_JIT_PREBUILT_DLL "lua51.dll")
	set(LUA_JIT_PREBUILT_EXP "lua51.exp")
	set(LUA_JIT_PREBUILT_EXE "luajit.exe")
else()
	# get the make command we need for this system
	find_program(MAKE_PROGRAM NAMES make mingw32-make mingw64-make)
	if (MAKE_PROGRAM MATCHES "MAKE_PROGRAM-NOTFOUND")
		MESSAGE(FATAL_ERROR "Cannot find 'make' program or similar needed to build LuaJIT ${LUA_VERSION} (perhaps place it in the PATH environment variable if it is not already?)")
	endif()

	# we can simply reuse the makefile here
	# so define it as an external project and then just have the proper
	# build/install/test commands
	# make sure to apply -pagezero_size 10000 -image_base 100000000 (done later for XCode Targets)
	set(LUA_JIT_MAKE_BUILD_MODIFICATIONS "LUAJIT_T=${LUA_JIT_EXE_FILENAME}")
	list(APPEND LUA_JIT_MAKE_BUILD_MODIFICATIONS "LUAJIT_A=${LUA_JIT_LIB_FILENAME}")
	set(LUA_JIT_MAKE_CFLAGS_MODIFICATIONS "")
	set(LUA_JIT_MAKE_XCFLAGS_MODIFICATIONS "")
	set(LUA_JIT_MAKE_HOST_CFLAGS_MODIFICATIONS "")
	set(LUA_JIT_MAKE_TARGET_CFLAGS_MODIFICATIONS "-fPIC")
	if (BUILD_LUA_AS_DLL)
		list(APPEND LUA_JIT_MAKE_BUILD_MODIFICATIONS "LUAJIT_SO=${LUA_JIT_DLL_FILENAME}" "TARGET_SONAME=${LUA_JIT_DLL_FILENAME}" "TARGET_DYLIBNAME=${LUA_JIT_DLL_FILENAME}" "TARGET_DLLNAME=${LUA_JIT_DLL_FILENAME}")
		list(APPEND LUA_JIT_MAKE_BUILD_MODIFICATIONS "BUILDMODE=dynamic")
	else()
		list(APPEND LUA_JIT_MAKE_BUILD_MODIFICATIONS "BUILDMODE=static")
	endif()
	if (IS_X86)
		list(APPEND LUA_JIT_MAKE_BUILD_MODIFICATIONS "CC=${CMAKE_C_COMPILER} -m32")
		list(APPEND LUA_JIT_MAKE_BUILD_MODIFICATIONS "LDFLAGS=-m32")
		#set(LUA_JIT_MAKE_CFLAGS_MODIFICATIONS "${LUA_JIT_MAKE_CFLAGS_MODIFICATIONS} -m32")
		#set(LUA_JIT_MAKE_HOST_CFLAGS_MODIFICATIONS "${LUA_JIT_MAKE_HOST_CFLAGS_MODIFICATIONS} -m32")
		#set(LUA_JIT_MAKE_TARGET_CFLAGS_MODIFICATIONS "${LUA_JIT_MAKE_TARGET_CFLAGS_MODIFICATIONS} -m32")
	endif()

	set(LUA_JIT_PREBUILT_DLL ${LUA_JIT_DLL_FILENAME})
	set(LUA_JIT_PREBUILT_LIB ${LUA_JIT_LIB_FILENAME})
	set(LUA_JIT_PREBUILT_IMP_LIB ${LUA_JIT_IMP_LIB_FILENAME})
	set(LUA_JIT_PREBUILT_EXE ${LUA_JIT_EXE_FILENAME})
	set(LUA_JIT_PREBUILT_EXP ${LUA_JIT_LIB_EXP_FILENAME})

	if (WIN32)
		list(APPEND LUA_JIT_MAKE_BUILD_MODIFICATIONS "HOST_SYS=Windows" "TARGET_SYS=Windows" "TARGET_AR=ar rcus")
	endif()
	list(APPEND LUA_JIT_MAKE_BUILD_MODIFICATIONS $<$<CONFIG:Debug>:"CCDEBUG= -g">)
	if (BUILD_LUA_52_COMPATIBILITY)
		list(APPEND LUA_JIT_MAKE_XCFLAGS_MODIFICATIONS "-DLUAJIT_ENABLE_LUA52COMPAT")
	endif()
	if (NOT BUILD_LUA_NOGC64)
		list(APPEND LUA_JIT_MAKE_XCFLAGS_MODIFICATIONS "-DLUAJIT_ENABLE_GC64")
	endif()
	list(APPEND LUA_JIT_MAKE_BUILD_MODIFICATIONS "CFLAGS=\"${LUA_JIT_MAKE_CFLAGS_MODIFICATIONS}\"")
	list(APPEND LUA_JIT_MAKE_BUILD_MODIFICATIONS "XCFLAGS=\"${LUA_JIT_MAKE_XCFLAGS_MODIFICATIONS}\"")
	list(APPEND LUA_JIT_MAKE_BUILD_MODIFICATIONS "TARGET_CFLAGS=${LUA_JIT_MAKE_TARGET_CFLAGS_MODIFICATIONS}")
	list(APPEND LUA_JIT_MAKE_BUILD_MODIFICATIONS "HOST_CFLAGS=${LUA_JIT_MAKE_HOST_CFLAGS_MODIFICATIONS}")
	set(LUA_JIT_MAKE_COMMAND "${MAKE_PROGRAM}" ${LUA_JIT_MAKE_BUILD_MODIFICATIONS})
endif()

set(LUA_JIT_BUILD_COMMAND BUILD_COMMAND ${LUA_JIT_MAKE_COMMAND})

set(lualib luajit_lib_${LUA_JIT_VERSION})
set(luainterpreter luajit_${LUA_JIT_VERSION})

file(TO_CMAKE_PATH "${LUA_JIT_SOURCE_DIR}/${LUA_JIT_PREBUILT_LIB}" LUA_JIT_SOURCE_LUA_LIB)
file(TO_CMAKE_PATH "${LUA_JIT_SOURCE_DIR}/${LUA_JIT_PREBUILT_IMP_LIB}" LUA_JIT_SOURCE_LUA_IMP_LIB)
file(TO_CMAKE_PATH "${LUA_JIT_SOURCE_DIR}/${LUA_JIT_PREBUILT_EXE}" LUA_JIT_SOURCE_LUA_INTERPRETER)
file(TO_CMAKE_PATH "${LUA_JIT_SOURCE_DIR}/${LUA_JIT_PREBUILT_DLL}" LUA_JIT_SOURCE_LUA_DLL)
file(TO_CMAKE_PATH "${LUA_JIT_SOURCE_DIR}/${LUA_JIT_PREBUILT_EXP}" LUA_JIT_SOURCE_LUA_LIB_EXP)

file(TO_CMAKE_PATH "${LUA_JIT_DLL_FILE}" LUA_JIT_DESTINATION_LUA_DLL)
file(TO_CMAKE_PATH "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${LUA_JIT_LIB_EXP_FILENAME}" LUA_JIT_DESTINATION_LUA_LIB_EXP)
file(TO_CMAKE_PATH "${LUA_JIT_IMP_LIB_FILE}" LUA_JIT_DESTINATION_LUA_IMP_LIB)
file(TO_CMAKE_PATH "${LUA_JIT_LIB_FILE}" LUA_JIT_DESTINATION_LUA_LIB)
file(TO_CMAKE_PATH "${LUA_JIT_EXE_FILE}" LUA_JIT_DESTINATION_LUA_INTERPRETER)

if(LUA_JIT_NORMALIZED_LUA_VERSION MATCHES "latest")
	set(LUA_JIT_PULL_LATEST TRUE)
endif()

set(LUA_JIT_BYPRODUCTS "${LUA_JIT_SOURCE_LUA_INTERPRETER}")
set(LUA_JIT_INSTALL_BYPRODUCTS "${LUA_JIT_DESTINATION_LUA_INTERPRETER}")

if (BUILD_LUA_AS_DLL)
	set(LUA_JIT_BYPRODUCTS ${LUA_JIT_BYPRODUCTS} "${LUA_JIT_SOURCE_LUA_DLL}")
	set(LUA_JIT_INSTALL_BYPRODUCTS ${LUA_JIT_INSTALL_BYPRODUCTS} "${LUA_JIT_SOURCE_LUA_DLL}")
	if (MSVC)
		set(LUA_JIT_BYPRODUCTS ${LUA_JIT_BYPRODUCTS} "${LUA_JIT_SOURCE_LUA_LIB_EXP}")
		set(LUA_JIT_INSTALL_BYPRODUCTS ${LUA_JIT_INSTALL_BYPRODUCTS} "${LUA_JIT_DESTINATION_LUA_LIB_EXP}")
	endif()
endif()

if (CMAKE_IMPORT_LIBRARY_SUFFIX AND BUILD_LUA_AS_DLL)
	set(LUA_JIT_BYPRODUCTS ${LUA_JIT_BYPRODUCTS} "${LUA_JIT_SOURCE_LUA_IMP_LIB}")
	set(LUA_JIT_INSTALL_BYPRODUCTS "${LUA_JIT_INSTALL_BYPRODUCTS}" "${LUA_JIT_DESTINATION_LUA_IMP_LIB}")
else()
	set(LUA_JIT_BYPRODUCTS ${LUA_JIT_BYPRODUCTS} "${LUA_JIT_SOURCE_LUA_LIB}")
	set(LUA_JIT_INSTALL_BYPRODUCTS ${LUA_JIT_INSTALL_BYPRODUCTS} "${LUA_JIT_DESTINATION_LUA_LIB}")
endif()

# # Post-Build moving steps for necessary items
# Add post-step to move library afterwards
set(LUA_JIT_POSTBUILD_COMMENTS "Executable - Moving \"${LUA_JIT_SOURCE_LUA_INTERPRETER}\" to \"${LUA_JIT_DESTINATION_LUA_INTERPRETER}\"...")
set(LUA_JIT_POSTBUILD_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUA_JIT_SOURCE_LUA_INTERPRETER} ${LUA_JIT_DESTINATION_LUA_INTERPRETER})
if (BUILD_LUA_AS_DLL)
	if (MSVC)
		set(LUA_JIT_POSTBUILD_COMMENTS "${LUA_JIT_POSTBUILD_COMMENTS} Import Library - Moving \"${LUA_JIT_SOURCE_LUA_IMP_LIB}\" to \"${LUA_JIT_DESTINATION_LUA_IMP_LIB}\"...")
		set(LUA_JIT_POSTBUILD_COMMANDS ${LUA_JIT_POSTBUILD_COMMANDS} COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${LUA_JIT_SOURCE_LUA_IMP_LIB} ${LUA_JIT_DESTINATION_LUA_IMP_LIB})
		
		set(LUA_JIT_POSTBUILD_COMMENTS "${LUA_JIT_POSTBUILD_COMMENTS} Library - Moving \"${LUA_JIT_SOURCE_LUA_LIB_EXP}\" to \"${LUA_JIT_DESTINATION_LUA_LIB_EXP}\"...")
		set(LUA_JIT_POSTBUILD_COMMANDS ${LUA_JIT_POSTBUILD_COMMANDS} COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUA_JIT_SOURCE_LUA_LIB_EXP} ${LUA_JIT_DESTINATION_LUA_LIB_EXP})
	endif()
	set(LUA_JIT_POSTBUILD_COMMENTS "${LUA_JIT_POSTBUILD_COMMENTS} Dynamic Library - Moving \"${LUA_JIT_SOURCE_LUA_DLL}\" to \"${LUA_JIT_DESTINATION_LUA_DLL}\"...")
	set(LUA_JIT_POSTBUILD_COMMANDS ${LUA_JIT_POSTBUILD_COMMANDS} COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUA_JIT_SOURCE_LUA_DLL} ${LUA_JIT_DESTINATION_LUA_DLL})
else()
	set(LUA_JIT_POSTBUILD_COMMENTS "${LUA_JIT_POSTBUILD_COMMENTS} Library - Moving \"${LUA_JIT_SOURCE_LUA_LIB}\" to \"${LUA_JIT_DESTINATION_LUA_LIB}\"...")
	set(LUA_JIT_POSTBUILD_COMMANDS ${LUA_JIT_POSTBUILD_COMMANDS} COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUA_JIT_SOURCE_LUA_LIB} ${LUA_JIT_DESTINATION_LUA_LIB})
endif()

if (LUA_LOCAL_DIR)
	cmake_path(SET LUAJIT_BUILD_LOCAL_DIR NORMALIZE ${LUA_LOCAL_DIR})
endif()
if (LUA_BUILD_TOPLEVEL)
	cmake_path(SET LUA_BUILD_TOPLEVEL NORMALIZE ${LUA_BUILD_TOPLEVEL})
endif()

if (LUAJIT_BUILD_LOCAL_DIR)
	MESSAGE(STATUS "Using LuaJIT ${LUA_JIT_VERSION} from local directory \"${LUAJIT_BUILD_LOCAL_DIR}\"")
	file(GLOB_RECURSE LUAJIT_BUILD_LOCAL_DIR_FILES_NATIVE
		LIST_DIRECTORIES FALSE
		CONFIGURE_DEPENDS
		${LUAJIT_BUILD_LOCAL_DIR}/*
	)
	cmake_path(CONVERT "${LUAJIT_BUILD_LOCAL_DIR_FILES_NATIVE}" TO_CMAKE_PATH_LIST LUAJIT_BUILD_LOCAL_DIR_FILES)
	list(TRANSFORM LUAJIT_BUILD_LOCAL_DIR_FILES REPLACE "${LUAJIT_BUILD_LOCAL_DIR}(.*)" "${LUA_BUILD_TOPLEVEL}\\1" OUTPUT_VARIABLE LUAJIT_BUILD_TOPLEVEL_FILES)
	add_custom_command(OUTPUT ${LUAJIT_BUILD_TOPLEVEL_FILES}
		COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${LUAJIT_BUILD_LOCAL_DIR} ${LUA_BUILD_TOPLEVEL}
		WORKING_DIRECTORY "${LUA_BUILD_TOPLEVEL}"
		DEPENDS ${LUAJIT_BUILD_LOCAL_DIR_FILES}
		COMMENT "Copying LuaJIT files from \"${LUAJIT_BUILD_LOCAL_DIR}\" to \"${LUA_BUILD_TOPLEVEL}\" ..."
	)
	add_custom_command(OUTPUT ${LUA_JIT_BYPRODUCTS}
		COMMAND ${LUA_JIT_MAKE_COMMAND}
		WORKING_DIRECTORY "${LUA_BUILD_TOPLEVEL}"
		DEPENDS ${LUAJIT_BUILD_TOPLEVEL_FILES}
		COMMENT "Building LuaJIT ${LUA_JIT_VERSION} from \"${LUA_BUILD_TOPLEVEL}\" ..."
	)
	add_custom_target(LUA_JIT-bat-build
		DEPENDS ${LUA_JIT_BYPRODUCTS}
	)
	add_custom_command(OUTPUT ${LUA_JIT_INSTALL_BYPRODUCTS}
		${LUA_JIT_POSTBUILD_COMMANDS}
		COMMENT "${LUA_JIT_POSTBUILD_COMMENTS}"
		WORKING_DIRECTORY "${LUA_BUILD_TOPLEVEL}"
		DEPENDS ${LUA_JIT_BYPRODUCTS}
	)
	add_custom_target(LUA_JIT-move
		DEPENDS LUA_JIT-bat-build ${LUA_JIT_INSTALL_BYPRODUCTS}
	)
elseif (LUA_JIT_GIT_COMMIT OR LUA_JIT_PULL_LATEST)
	if (LUA_JIT_PULL_LATEST)
		MESSAGE(STATUS "Latest LuaJIT has been requested: pulling from git...")
	elseif (LUA_JIT_GIT_COMMIT)
		MESSAGE(STATUS "LuaJIT '${LUA_VERSION}' requested has broken static library builds: using git '${LUA_JIT_GIT_COMMIT}'...")
		set(LUA_JIT_GIT_TAG GIT_TAG ${LUA_JIT_GIT_COMMIT})
	endif()
	ExternalProject_Add(LUA_JIT
		BUILD_IN_SOURCE TRUE
		BUILD_ALWAYS FALSE
		PREFIX "${LUA_BUILD_TOPLEVEL}"
		SOURCE_DIR "${LUA_BUILD_TOPLEVEL}"
		DOWNLOAD_DIR "${LUA_BUILD_TOPLEVEL}"
		TMP_DIR "${LUA_BUILD_TOPLEVEL}-tmp"
		STAMP_DIR "${LUA_BUILD_TOPLEVEL}-stamp"
		INSTALL_DIR "${LUA_BUILD_INSTALL_DIR}"
		GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
		GIT_REMOTE_NAME origin
		${LUA_JIT_GIT_TAG}
		GIT_SHALLOW TRUE
		CONFIGURE_COMMAND ""
		${LUA_JIT_BUILD_COMMAND}
		INSTALL_COMMAND ""
		TEST_COMMAND ""
		BUILD_BYPRODUCTS ${LUA_JIT_BYPRODUCTS})
else()
	ExternalProject_Add(LUA_JIT
		BUILD_IN_SOURCE TRUE
		BUILD_ALWAYS TRUE
		# LuaJIT does not offer a TLS/SSL port
		TLS_VERIFY FALSE
		PREFIX "${LUA_BUILD_TOPLEVEL}"
		SOURCE_DIR "${LUA_BUILD_TOPLEVEL}"
		DOWNLOAD_DIR "${LUA_BUILD_TOPLEVEL}"
		TMP_DIR "${LUA_BUILD_TOPLEVEL}-tmp"
		STAMP_DIR "${LUA_BUILD_TOPLEVEL}-stamp"
		INSTALL_DIR "${LUA_BUILD_INSTALL_DIR}"
		URL "${LUA_JIT_DOWNLOAD_URI}"
		${LUA_JIT_DOWNLOAD_MD5_COMMAND}
		${LUA_JIT_DOWNLOAD_SHA1_COMMAND}
		CONFIGURE_COMMAND ""
		${LUA_JIT_BUILD_COMMAND}
		INSTALL_COMMAND ""
		TEST_COMMAND ""
		BUILD_BYPRODUCTS ${LUA_JIT_BYPRODUCTS})
endif()

if (NOT LUAJIT_BUILD_LOCAL_DIR)
	ExternalProject_Add_Step(LUA_JIT move
		ALWAYS TRUE
		${LUA_JIT_POSTBUILD_COMMANDS}
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" 
		COMMENT ${LUA_JIT_POSTBUILD_COMMENTS}
		DEPENDEES build
		DEPENDS ${LUA_JIT_BYPRODUCTS}
		BYPRODUCTS ${LUA_JIT_INSTALL_BYPRODUCTS})
	ExternalProject_Add_StepTargets(LUA_JIT move)
endif()

# # Lua Library
add_library(${lualib} INTERFACE)
add_library(Lua::Lua ALIAS ${lualib})
add_dependencies(${lualib} LUA_JIT-move)
target_include_directories(${lualib}
	INTERFACE
		"${LUA_JIT_SOURCE_DIR}")
target_link_libraries(${lualib}
	INTERFACE
		${CMAKE_DL_LIBS})
if (BUILD_LUA_AS_DLL)
	if (MSVC)
		target_link_libraries(${lualib}
			INTERFACE "${LUA_JIT_DESTINATION_LUA_LIB}")
	else()
		target_link_libraries(${lualib}
			INTERFACE "${LUA_JIT_DESTINATION_LUA_DLL}")
	endif()
else()
	target_link_libraries(${lualib}
		INTERFACE "${LUA_JIT_DESTINATION_LUA_LIB}")
endif()

if (XCODE)
	target_compile_options(${lualib}
		INTERFACE -pagezero_size 10000 -image_base 100000000)
endif ()
	
# # set externally-visible target indicator
set(LUA_LIBRARIES Lua::Lua)
set(LUA_INTERPRETER "")
set(LUA_INCLUDE_DIRS "${LUA_JIT_INCLUDE_DIRS}")
