#include <sol/sol.hpp>
#include "engine/loader/loader.hpp"
#include "engine/scripting/bindings/loader_lua_bindings.hpp"

namespace bubble
{
void CreateLoaderBidnings( Loader& loader, sol::state& lua )
{
    lua["LoadTexture"] = [&]( const string& str ) { return loader.LoadTexture2D( str ); };
    lua["LoadModel"] = [&]( const string& str ) { return loader.LoadModel( str ); };
    lua["LoadShader"] = [&]( const string& str ) { return loader.LoadShader( str ); };
    lua["LoadScript"] = [&]( const string& str ) { return loader.LoadScript( str ); };
}
}