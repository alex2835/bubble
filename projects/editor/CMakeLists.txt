file( GLOB_RECURSE BUBBLE_EDITOR_SOURCES CONFIGURE_DEPENDS src/*.cpp )
file( GLOB_RECURSE BUBBLE_EDITOR_HEADERS CONFIGURE_DEPENDS include/*.hpp )

# The editor's font, built in, so the editor reads no file for it.
set( BUBBLE_EDITOR_FONT_SOURCE ${PROJECT_SOURCE_DIR}/deps/roboto/Roboto-Medium.ttf )
set( BUBBLE_EDITOR_FONT ${CMAKE_CURRENT_BINARY_DIR}/generated/editor_font.cpp )
add_custom_command( OUTPUT ${BUBBLE_EDITOR_FONT}
                    COMMAND ${CMAKE_COMMAND} -DINPUT=${BUBBLE_EDITOR_FONT_SOURCE} -DOUTPUT=${BUBBLE_EDITOR_FONT}
                            -DNAME=EditorFont -DLABEL=deps/roboto/Roboto-Medium.ttf
                            -P ${PROJECT_SOURCE_DIR}/cmake/embed_file.cmake
                    DEPENDS ${BUBBLE_EDITOR_FONT_SOURCE} ${PROJECT_SOURCE_DIR}/cmake/embed_file.cmake
                    COMMENT "Building the editor's font in"
                    VERBATIM )

# The editor's panels on Dear ImGui, over bubble_editing's documents and
# operators: ImGui's backends of our own - the platform's events in, the GPU
# out -, the worlds that show documents, the viewport. The engine, import
# and editing never include it.
# Its icons, Lucide's font, built in the same way and merged into the font.
set( BUBBLE_EDITOR_ICONS_SOURCE ${PROJECT_SOURCE_DIR}/deps/lucide/lucide.ttf )
set( BUBBLE_EDITOR_ICONS ${CMAKE_CURRENT_BINARY_DIR}/generated/editor_icons_font.cpp )
add_custom_command( OUTPUT ${BUBBLE_EDITOR_ICONS}
                    COMMAND ${CMAKE_COMMAND} -DINPUT=${BUBBLE_EDITOR_ICONS_SOURCE} -DOUTPUT=${BUBBLE_EDITOR_ICONS}
                            -DNAME=EditorIconsFont -DLABEL=deps/lucide/lucide.ttf
                            -P ${PROJECT_SOURCE_DIR}/cmake/embed_file.cmake
                    DEPENDS ${BUBBLE_EDITOR_ICONS_SOURCE} ${PROJECT_SOURCE_DIR}/cmake/embed_file.cmake
                    COMMENT "Building the editor's icons in"
                    VERBATIM )

add_library( bubble_editor STATIC ${BUBBLE_EDITOR_SOURCES} ${BUBBLE_EDITOR_HEADERS} ${BUBBLE_EDITOR_FONT}
             ${BUBBLE_EDITOR_ICONS} )
target_include_directories( bubble_editor PUBLIC include )
bubble_target_warnings( bubble_editor )
target_link_libraries( bubble_editor PUBLIC bubble_editing imgui PRIVATE imguizmo imgui_node_editor )
# The MCP server's sockets.
if( WIN32 )
    target_link_libraries( bubble_editor PRIVATE ws2_32 )
endif()

# bubble_editor <project folder>: the desktop alone for now.
if( NOT EMSCRIPTEN )
    add_executable( bubble_editor_app main.cpp )
    set_target_properties( bubble_editor_app PROPERTIES OUTPUT_NAME bubble_editor )
    target_link_libraries( bubble_editor_app PRIVATE bubble_editor )
    bubble_executable( bubble_editor_app )
endif()
