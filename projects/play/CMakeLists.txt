# bubble_play: a project played from its folder - imported as it starts and
# again as its files change -, on the engine and the importers. What the
# editor's Play will be, without the editor; the desktop alone.
if( NOT EMSCRIPTEN )
    add_executable( bubble_play main.cpp )
    target_link_libraries( bubble_play PRIVATE bubble_engine bubble_import )
    bubble_executable( bubble_play )
endif()
