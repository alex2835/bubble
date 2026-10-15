file( GLOB_RECURSE BUBBLE_EDITING_SOURCES CONFIGURE_DEPENDS src/*.cpp )
file( GLOB_RECURSE BUBBLE_EDITING_HEADERS CONFIGURE_DEPENDS include/*.hpp )

# What the editor changes and how: documents, their history, the edits. The
# engine never includes it, and a runner does not link it.
add_library( bubble_editing STATIC ${BUBBLE_EDITING_SOURCES} ${BUBBLE_EDITING_HEADERS} )
target_include_directories( bubble_editing PUBLIC include )
bubble_target_warnings( bubble_editing )
target_link_libraries( bubble_editing PUBLIC bubble_engine )
