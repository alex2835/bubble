file( GLOB_RECURSE BUBBLE_IMPORT_SOURCES CONFIGURE_DEPENDS src/*.cpp )
file( GLOB_RECURSE BUBBLE_IMPORT_HEADERS CONFIGURE_DEPENDS include/*.hpp )

# The project's files as sources: their ids in .import files, the scan that
# keeps them right, the importers and their cache, the project kept in step
# as files change. The editor and the cook use it; the engine never includes
# it, and a runner does not link it - nor stb_image.
add_library( bubble_import STATIC ${BUBBLE_IMPORT_SOURCES} ${BUBBLE_IMPORT_HEADERS} )
target_include_directories( bubble_import PUBLIC include )
bubble_target_warnings( bubble_import )
target_link_libraries( bubble_import PUBLIC bubble_engine PRIVATE stb_image )
# Models where assimp is built: not on the web.
if( TARGET assimp )
    target_link_libraries( bubble_import PRIVATE assimp meshoptimizer )
    target_compile_definitions( bubble_import PUBLIC BUBBLE_IMPORT_MODELS )
endif()
