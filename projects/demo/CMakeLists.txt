# bubble_demo: a scene made in code, played on the engine alone - never the
# editor or the importers, which its build checks -; the web's build too.
add_executable( bubble_demo main.cpp )
target_link_libraries( bubble_demo PRIVATE bubble_engine )
bubble_executable( bubble_demo )
if( EMSCRIPTEN )
    # A page with a canvas for the window: emscripten's own shell.
    set_target_properties( bubble_demo PROPERTIES SUFFIX ".html" )
endif()
