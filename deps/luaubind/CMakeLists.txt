# luaubind - a small C++ binding for Luau. Depends on Luau only; built with
# the same warnings as the engine, since it is our own code.
file( GLOB LUAUBIND_SOURCES CONFIGURE_DEPENDS src/*.cpp )
file( GLOB LUAUBIND_HEADERS CONFIGURE_DEPENDS include/luaubind/*.hpp )

add_library( luaubind STATIC ${LUAUBIND_SOURCES} ${LUAUBIND_HEADERS} )
target_include_directories( luaubind PUBLIC include )
target_link_libraries( luaubind PUBLIC luau )
bubble_target_warnings( luaubind )

# Native code where Luau can make it; the web and iOS interpret.
if( TARGET luau_codegen )
    target_compile_definitions( luaubind PRIVATE LUAUBIND_CODEGEN )
endif()
