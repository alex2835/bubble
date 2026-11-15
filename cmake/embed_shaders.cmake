# Writes OUTPUT: a C++ file holding every .wgsl file in SOURCE_DIR as a raw
# string literal, by its file name - the engine's own shaders built into it,
# so the web reads no files for them. Run by the engine's build whenever a
# shader changes.
file( GLOB shaders "${SOURCE_DIR}/*.wgsl" )
list( SORT shaders )
list( LENGTH shaders count )
set( entries "" )
foreach( shader ${shaders} )
    get_filename_component( name "${shader}" NAME )
    file( READ "${shader}" text )
    string( FIND "${text}" ")wgsl\"" closing )
    if( NOT closing EQUAL -1 )
        message( FATAL_ERROR "${shader} holds )wgsl\", which would end its literal early" )
    endif()
    string( APPEND entries "    EmbeddedShader{ \"${name}\", R\"wgsl(${text})wgsl\" },\n" )
endforeach()

set( source "// Made by cmake/embed_shaders.cmake from projects/engine/shaders: edit those.
#include \"bubble/render/gpu/shaders.hpp\"
#include \"bubble/types/containers.hpp\"
#include \"bubble/types/format.hpp\"

namespace bubble
{
namespace
{
struct EmbeddedShader
{
    string_view mName;
    string_view mText;
};

constexpr array<EmbeddedShader, ${count}> cShaders{
${entries}};
}

string_view EngineShader( string_view name )
{
    for ( const EmbeddedShader& shader : cShaders )
        if ( shader.mName == name )
            return shader.mText;
    throw logic_error( format( \"no engine shader {}\", name ) );
}
}
" )
file( WRITE "${OUTPUT}" "${source}" )
