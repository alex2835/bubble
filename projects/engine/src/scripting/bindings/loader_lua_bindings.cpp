#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include "engine/loader/loader.hpp"
#include "engine/scripting/bindings/loader_lua_bindings.hpp"

namespace bubble
{
void CreateLoaderBidnings( sol::state& lua )
{
    lua.new_usertype<Loader>(
        "Loader",
        "LoadTexture",
        []( Loader& loader, const string& str ) { return loader.LoadTexture2D( str ); },
        "LoadModel",
        []( Loader& loader, const string& str ) { return loader.LoadModel( str ); },
        "LoadShader",
        []( Loader& loader, const string& str ) { return loader.LoadShader( str ); },
        "LoadScript",
        []( Loader& loader, const string& str ) { return loader.LoadScript( str ); }
    );
}
}