cmake_minimum_required(VERSION 3.12)

set(SOURCES
	miniaudio.c
	miniaudio.h
)
add_library(miniaudio ${SOURCES})

target_include_directories(miniaudio PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)

if(EMSCRIPTEN)
	# The default Web Audio backend needs no link options and works out of the
	# box. AudioWorklets are the lower latency path and are opt-in, because they
	# cost -sASYNCIFY across the whole program:
	#   -DMA_ENABLE_AUDIO_WORKLETS -sAUDIO_WORKLET=1 -sWASM_WORKERS=1 -sASYNCIFY
	# Browsers will not start an AudioContext without a user gesture either way,
	# see AudioEngine::ResumeAfterUserGesture.
elseif(APPLE)
	# Required by MA_NO_RUNTIME_LINKING, see the top of miniaudio.c.
	target_link_libraries(miniaudio PUBLIC
		"-framework CoreFoundation"
		"-framework CoreAudio"
		"-framework AudioToolbox"
	)
	target_link_libraries(miniaudio PUBLIC pthread m)
elseif(UNIX)
	target_link_libraries(miniaudio PUBLIC dl pthread m)
endif()
