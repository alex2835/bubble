# bubble_runner: a project's game run from its folder - imported as it
# starts and again as its files change -, on the engine and the importers.
# What the editor's Play is, without the editor; the desktop alone.
if( NOT EMSCRIPTEN )
    add_executable( bubble_runner main.cpp )
    target_link_libraries( bubble_runner PRIVATE bubble_engine bubble_import )
    bubble_executable( bubble_runner )
endif()
