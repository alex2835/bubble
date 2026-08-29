#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"
#include "engine/scripting/bindings/loader_lua_bindings.hpp"
#include <sol/sol.hpp>

namespace bubble
{
void CreateLoaderBindings( Loader& loader, sol::state& lua )
{
  lua["load_texture"] = [&]( const string& str ) { return loader.LoadTexture2D( str ); };
  lua["load_model"] = [&]( const string& str ) { return loader.LoadModel( str ); };
  lua["load_shader"] = [&]( const string& str ) { return loader.LoadShader( str ); };
  lua["load_script"] = [&]( const string& str ) { return loader.LoadScript( str ); };
}
}