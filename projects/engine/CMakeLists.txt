file( GLOB_RECURSE BUBBLE_ENGINE_SOURCES CONFIGURE_DEPENDS src/*.cpp )
file( GLOB_RECURSE BUBBLE_ENGINE_HEADERS CONFIGURE_DEPENDS include/*.hpp )

add_library( bubble_engine STATIC ${BUBBLE_ENGINE_SOURCES} ${BUBBLE_ENGINE_HEADERS} )
target_include_directories( bubble_engine PUBLIC include )
bubble_target_warnings( bubble_engine )
