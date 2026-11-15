# The runner: a game on its own, on the engine alone - never the editor or
# the importers, which its build checks.
add_executable( bubble_runner main.cpp )
target_link_libraries( bubble_runner PRIVATE bubble_engine )
bubble_executable( bubble_runner )
if( EMSCRIPTEN )
    # A page with a canvas for the window: emscripten's own shell.
    set_target_properties( bubble_runner PROPERTIES SUFFIX ".html" )
endif()
