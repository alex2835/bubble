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
        "loadTexture", &Loader::LoadTexture2D,
        "loadModel", &Loader::LoadModel,
        "loadShader", &Loader::LoadShader,
        "loadScript", &Loader::LoadScript
    );
}
}